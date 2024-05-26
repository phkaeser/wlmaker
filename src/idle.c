/* ========================================================================= */
/**
 * @file idle.c
 *
 * @copyright
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "idle.h"

#include "config.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_idle_inhibit_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Forward declaration: Handle of an idle inhibitor. */
typedef struct _wlmaker_idle_inhibitor_t wlmaker_idle_inhibitor_t;

/** State of the idle monitor. */
struct _wlmaker_idle_monitor_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Dictionnary holding the 'ScreenLock' configuration. */
    wlmcfg_dict_t             *lock_config_dict_ptr;

    /** Reference to the event loop. */
    struct wl_event_loop      *wl_event_loop_ptr;
    /** The timer's event source. */
    struct wl_event_source    *timer_event_source_ptr;

    /** Listener for `new_inhibitor` of wlr_idle_inhibit_manager_v1`. */
    struct wl_listener        new_inhibitor_listener;
    /** Lists registered inhibitors: @ref wlmaker_idle_inhibitor_t::dlnode. */
    bs_dllist_t               idle_inhibitors;

    /** Listener for @ref wlmaker_root_t::unlock_event. */
    struct wl_listener        unlock_listener;

    /** The wlroots idle inhibit manager. */
    struct wlr_idle_inhibit_manager_v1 *wlr_idle_inhibit_manager_v1_ptr;

    /** Whether the idle monitor is locked. Prevents timer registry. */
    bool                      locked;
};

/** State of an idle inhibitor. */
struct _wlmaker_idle_inhibitor_t {
    /** Back-link to the idle monitor. */
    wlmaker_idle_monitor_t *idle_monitor_ptr;
    /** The idle inhibitor tied to this inhibitor. */
    struct wlr_idle_inhibitor_v1 *wlr_idle_inhibitor_v1_ptr;

    /** List node, part of @ref wlmaker_idle_monitor_t::idle_inhibitors. */
    bs_dllist_node_t          dlnode;

    /** Listener for the `destroy` signal of `wlr_idle_inhibitor_v1`. */
    struct wl_listener        destroy_listener;
};

static int _wlmaker_idle_monitor_timer(void *data_ptr);

static bool _wlmaker_idle_monitor_add_inhibitor(
    wlmaker_idle_monitor_t *idle_monitor_ptr,
    struct wlr_idle_inhibitor_v1 *wlr_idle_inhibitor_v1_ptr);
static void _wlmaker_idle_monitor_handle_destroy_inhibitor(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_idle_monitor_handle_new_inhibitor(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_idle_monitor_handle_unlock(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_idle_monitor_t *wlmaker_idle_monitor_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_idle_monitor_t *monitor_ptr = logged_calloc(
        1, sizeof(wlmaker_idle_monitor_t));
    if (NULL == monitor_ptr) return NULL;
    monitor_ptr->server_ptr = server_ptr;
    monitor_ptr->wl_event_loop_ptr = wl_display_get_event_loop(
        server_ptr->wl_display_ptr);

    monitor_ptr->lock_config_dict_ptr = wlmcfg_dict_get_dict(
        server_ptr->config_dict_ptr, "ScreenLock");
    if (NULL == monitor_ptr->lock_config_dict_ptr) {
        bs_log(BS_ERROR, "No 'ScreenLock' dict found in config.");
        wlmaker_idle_monitor_destroy(monitor_ptr);
        return NULL;
    }
    wlmcfg_dict_dup(monitor_ptr->lock_config_dict_ptr);

    monitor_ptr->wlr_idle_inhibit_manager_v1_ptr =
        wlr_idle_inhibit_v1_create(server_ptr->wl_display_ptr);
    if (NULL == monitor_ptr->wlr_idle_inhibit_manager_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_idle_inhibit_v1_create(%p)",
               server_ptr->wl_display_ptr);
        wlmaker_idle_monitor_destroy(monitor_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &monitor_ptr->wlr_idle_inhibit_manager_v1_ptr->events.new_inhibitor,
        &monitor_ptr->new_inhibitor_listener,
        _wlmaker_idle_monitor_handle_new_inhibitor);

    monitor_ptr->timer_event_source_ptr = wl_event_loop_add_timer(
        monitor_ptr->wl_event_loop_ptr,
        _wlmaker_idle_monitor_timer,
        monitor_ptr);
    if (NULL == monitor_ptr->timer_event_source_ptr) {
        bs_log(BS_ERROR, "Failed wl_event_loop_add_timer(%p, %p, %p)",
               monitor_ptr->wl_event_loop_ptr,
               _wlmaker_idle_monitor_timer,
               monitor_ptr);
        wlmaker_idle_monitor_destroy(monitor_ptr);
        return NULL;
    }

    if (0 != wl_event_source_timer_update(
            monitor_ptr->timer_event_source_ptr,
            config_idle_lock_msec)) {
        bs_log(BS_ERROR, "Failed wl_event_source_timer_update(%p, 1000)",
               monitor_ptr->timer_event_source_ptr);
        wlmaker_idle_monitor_destroy(monitor_ptr);
        return NULL;
    }


    return monitor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_idle_monitor_destroy(wlmaker_idle_monitor_t *idle_monitor_ptr)
{
    if (NULL != idle_monitor_ptr->unlock_listener.link.prev) {
        wl_list_remove(&idle_monitor_ptr->unlock_listener.link);
    }

    if (NULL != idle_monitor_ptr->timer_event_source_ptr) {
        wl_event_source_remove(idle_monitor_ptr->timer_event_source_ptr);
        idle_monitor_ptr->timer_event_source_ptr = NULL;
    }

    if (NULL != idle_monitor_ptr->lock_config_dict_ptr) {
        wlmcfg_dict_destroy(idle_monitor_ptr->lock_config_dict_ptr);
        idle_monitor_ptr->lock_config_dict_ptr = NULL;
    }

    // Note: The idle inhibit manager does not have a dtor.

    free(idle_monitor_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_idle_monitor_reset(wlmaker_idle_monitor_t *idle_monitor_ptr)
{
    if (idle_monitor_ptr->locked) return;

    int rv = wl_event_source_timer_update(
        idle_monitor_ptr->timer_event_source_ptr,
        config_idle_lock_msec);
    BS_ASSERT(0 == rv);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Timer function for the wayland event loop.
 *
 * @param data_ptr            Untyped pointer to @ref wlmaker_idle_monitor_t.
 *
 * @return Whether the event source is registered for re-check with
 *     wl_event_source_check(): 0 for all done, 1 for needing a re-check. If
 *     not registered, the return value is ignored and should be zero.
 */
int _wlmaker_idle_monitor_timer(void *data_ptr)
{
    wlmaker_idle_monitor_t *idle_monitor_ptr = data_ptr;

    // TODO(kaeser@gubbe.ch): We should better handle this via a subprocess.
    // And maybe keep monitoring the outcome?
    pid_t rv = fork();
    if (-1 == rv) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed fork()");
        return 0;
    } else if (0 == rv) {
        execl("/usr/bin/swaylock", "/usr/bin/swaylock", (void *)NULL);
    } else {
        idle_monitor_ptr->locked = true;
        wlmaker_root_connect_unlock_signal(
            idle_monitor_ptr->server_ptr->root_ptr,
            &idle_monitor_ptr->unlock_listener,
            _wlmaker_idle_monitor_handle_unlock);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates and adds a new inhibitor to the monitor.
 *
 * @param idle_monitor_ptr
 * @param wlr_idle_inhibitor_v1_ptr
 *
 * @return true on success.
 */
bool _wlmaker_idle_monitor_add_inhibitor(
    wlmaker_idle_monitor_t *idle_monitor_ptr,
    struct wlr_idle_inhibitor_v1 *wlr_idle_inhibitor_v1_ptr)
{
    wlmaker_idle_inhibitor_t *idle_inhibitor_ptr = logged_calloc(
        1, sizeof(wlmaker_idle_inhibitor_t));
    if (NULL == idle_inhibitor_ptr) return false;
    idle_inhibitor_ptr->idle_monitor_ptr = idle_monitor_ptr;
    idle_inhibitor_ptr->wlr_idle_inhibitor_v1_ptr = wlr_idle_inhibitor_v1_ptr;

    wlmtk_util_connect_listener_signal(
        &wlr_idle_inhibitor_v1_ptr->events.destroy,
        &idle_inhibitor_ptr->destroy_listener,
        _wlmaker_idle_monitor_handle_destroy_inhibitor);

    bs_dllist_push_back(&idle_monitor_ptr->idle_inhibitors,
                        &idle_inhibitor_ptr->dlnode);

    // Coming here: We know to have at least 1 inhibitor.
    if (0 != wl_event_source_timer_update(
            idle_monitor_ptr->timer_event_source_ptr, 0)) {
        // Huh. Failed. We'll keep the inhibitor nonetheless. Yes?
        bs_log(BS_WARNING, "Failed wl_event_source_timer_update(%p, 0)",
               idle_monitor_ptr->timer_event_source_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the inhibitor. Destroys it.
 *
 * @param listener_ptr
 * @param data_ptr            Unused.
 */
void _wlmaker_idle_monitor_handle_destroy_inhibitor(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_idle_inhibitor_t *idle_inhibitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_idle_inhibitor_t, destroy_listener);

    bs_dllist_remove(
        &idle_inhibitor_ptr->idle_monitor_ptr->idle_inhibitors,
        &idle_inhibitor_ptr->dlnode);
    if (bs_dllist_empty(
            &idle_inhibitor_ptr->idle_monitor_ptr->idle_inhibitors)) {
        wlmaker_idle_monitor_reset(idle_inhibitor_ptr->idle_monitor_ptr);
    }

    wl_list_remove(&idle_inhibitor_ptr->destroy_listener.link);
    free(idle_inhibitor_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_inhibitor` signal of the inhibit manager: Registers
 * the inhibitor.
 *
 * @param listener_ptr
 * @param data_ptr            Pointer to struct wlr_idle_inhibitor_v1.
 */
static void _wlmaker_idle_monitor_handle_new_inhibitor(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_idle_monitor_t *idle_monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_idle_monitor_t, new_inhibitor_listener);
    struct wlr_idle_inhibitor_v1 *wlr_idle_inhibitor_v1_ptr = data_ptr;

    if (!_wlmaker_idle_monitor_add_inhibitor(
            idle_monitor_ptr,
            wlr_idle_inhibitor_v1_ptr)) {
        wl_resource_post_error(
            wlr_idle_inhibitor_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmaker_idle_monitor_add_inhibitor(%p, %p)",
            idle_monitor_ptr,
            wlr_idle_inhibitor_v1_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for @ref wlmaker_root_t::unlock_event. Re-arms the timer.
 *
 * @param listener_ptr
 * @param data_ptr            unused.
 */
void _wlmaker_idle_monitor_handle_unlock(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_idle_monitor_t *idle_monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_idle_monitor_t, unlock_listener);

    wl_list_remove(&idle_monitor_ptr->unlock_listener.link);
    idle_monitor_ptr->locked = false;
    wlmaker_idle_monitor_reset(idle_monitor_ptr);
}

/* == End of idle.c ======================================================== */
