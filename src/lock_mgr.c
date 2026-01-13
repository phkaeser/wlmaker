/* ========================================================================= */
/**
 * @file lock_mgr.c
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

#include "lock_mgr.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/util/box.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Type of wlr_session_lock_surface_v1_configure(). */
typedef uint32_t (*_wlmaker_lock_surface_configure_t)(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height);
/** Type of wlr_session_lock_v1_send_locked(). */
typedef void (*_wlmaker_lock_send_locked_t)(
    struct wlr_session_lock_v1 *lock);

/** State of the session lock manager. */
struct _wlmaker_lock_mgr_t {
    /** The wlroots session lock manager. */
    struct wlr_session_lock_manager_v1 *wlr_session_lock_manager_v1_ptr;

    /** Reference to the wlmaker server. */
    wlmaker_server_t          *server_ptr;

    /** Listener for the `new_lock` signal of `wlr_session_lock_manager_v1`. */
    struct wl_listener        new_lock_listener;
    /** Listener for the `destroy` signal of `wlr_session_lock_manager_v1`. */
    struct wl_listener        destroy_listener;
};

/** Forward declaration: Lock. */
typedef struct _wlmaker_lock_t wlmaker_lock_t;
/** Forward declaration: Output. */
typedef struct _wlmaker_lock_output_t wlmaker_lock_output_t;

/** State of the session lock. */
struct _wlmaker_lock_t {
    /** The wlroots session lock. */
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr;
    /** Seat for the session. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The root this lock is applied for. */
    wlmtk_root_t              *root_ptr;

    /** The output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Injected method: Configure the lock surface. */
    _wlmaker_lock_surface_configure_t injected_surface_configure;
    /** Injected method: Confirm session lock. */
    _wlmaker_lock_send_locked_t injected_send_locked;

    /** Container holding the lock surfaces. */
    wlmtk_container_t         container;

    /** Listener for the `new_surface` signal of `wlr_session_lock_v1`. */
    struct wl_listener        new_surface_listener;
    /** Listener for the `unlock` signal of `wlr_session_lock_v1`. */
    struct wl_listener        unlock_listener;
    /** Listener for the `destroy` signal of `wlr_session_lock_v1`. */
    struct wl_listener        destroy_listener;

    /** Tracks all the outputs. */
    wlmtk_output_tracker_t    *output_tracker_ptr;
    /** Outputs with surface. Via @ref wlmaker_lock_output_t::dlnode. */
    bs_dllist_t               outputs;
};

/** An active output, that should then also get locked. */
struct _wlmaker_lock_output_t {
    /** Element of @ref wlmaker_lock_t::outputs. */
    bs_dllist_node_t          dlnode;

    /** The wlroots session lock surface. */
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr;
    /** Toolkit surface for the associated wl_surface. */
    wlmtk_surface_t           *wlmtk_surface_ptr;
    /** Back-link to the lock. */
    wlmaker_lock_t            *lock_ptr;
    /** Whether this lock surface got committed, ie. is ready to lock. */
    bool                      committed;

    /** Serial returned by `wlr_session_lock_surface_v1_configure`. */
    uint32_t                  configure_serial;

    /** Listener for the `destroy` signal of `wlr_session_lock_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for `commit` signal of `wlr_session_lock_surface_v1::surface`. */
    struct wl_listener        surface_commit_listener;
};

static void _wlmaker_lock_mgr_handle_new_lock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr,
    _wlmaker_lock_surface_configure_t injected_surface_configure,
    _wlmaker_lock_send_locked_t injected_send_locked);
static void _wlmaker_lock_destroy(wlmaker_lock_t *lock_ptr);
static wlmtk_element_t *_wlmaker_lock_element(wlmaker_lock_t *lock_ptr);

static void _wlmaker_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void *_wlmaker_lock_output_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);
static void _wlmaker_lock_output_update(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);
static void _wlmaker_lock_output_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

static bool _wlmaker_lock_output_create_surface(
    wlmaker_lock_output_t *lock_output_ptr,
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr);
static void _wlmaker_lock_output_destroy_surface(
    wlmaker_lock_output_t *lock_output_ptr);
static void _wlmaker_lock_output_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_output_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static bool _wlmaker_lock_output_surface_is_committed(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_lock_mgr_t *wlmaker_lock_mgr_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = logged_calloc(
        1, sizeof(wlmaker_lock_mgr_t));
    if (NULL == lock_mgr_ptr) return NULL;
    lock_mgr_ptr->server_ptr = server_ptr;

    lock_mgr_ptr->wlr_session_lock_manager_v1_ptr =
        wlr_session_lock_manager_v1_create(server_ptr->wl_display_ptr);
    if (NULL == lock_mgr_ptr->wlr_session_lock_manager_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_session_lock_manager_v1_create(%p)",
               server_ptr->wl_display_ptr);
        wlmaker_lock_mgr_destroy(lock_mgr_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_mgr_ptr->wlr_session_lock_manager_v1_ptr->events.new_lock,
        &lock_mgr_ptr->new_lock_listener,
        _wlmaker_lock_mgr_handle_new_lock);
    wlmtk_util_connect_listener_signal(
        &lock_mgr_ptr->wlr_session_lock_manager_v1_ptr->events.destroy,
        &lock_mgr_ptr->destroy_listener,
        _wlmaker_lock_mgr_handle_destroy);

    return lock_mgr_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_lock_mgr_destroy(wlmaker_lock_mgr_t *lock_mgr_ptr)
{
    wlmtk_util_disconnect_listener(&lock_mgr_ptr->destroy_listener);
    wlmtk_util_disconnect_listener(&lock_mgr_ptr->new_lock_listener);

    // Note: No destroy method for wlr_session_lock_manager_v1_ptr.

    free(lock_mgr_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_lock` signal of `wlr_session_lock_manager_v1`: creates
 * the corresponding lock.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_mgr_handle_new_lock(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_mgr_t, new_lock_listener);
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr = data_ptr;

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        wlr_session_lock_v1_ptr,
        lock_mgr_ptr->server_ptr->wlr_output_layout_ptr,
        lock_mgr_ptr->server_ptr->wlr_seat_ptr,
        lock_mgr_ptr->server_ptr->root_ptr,
        wlr_session_lock_surface_v1_configure,
        wlr_session_lock_v1_send_locked);
    if (NULL == lock_ptr) {
        wl_resource_post_error(
            wlr_session_lock_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmt_lock_create(%p, %p)",
            wlr_session_lock_v1_ptr,
            lock_mgr_ptr->server_ptr->root_ptr);
        bs_log(BS_WARNING, "Failed wlmt_lock_create(%p, %p)",
               wlr_session_lock_v1_ptr,
               lock_mgr_ptr->server_ptr->root_ptr);
        return;
    }

    bs_log(BS_INFO, "Lock manager %p: New lock %p", lock_mgr_ptr, lock_ptr);
}
/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_manager_v1`: Cleans
 * up associated resources.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_mgr_t, destroy_listener);

    wlmaker_lock_mgr_destroy(lock_mgr_ptr);
}

/* == lock methods ========================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Creates a session lock handle.
 *
 * @param wlr_session_lock_v1_ptr
 * @param wlr_output_layout_ptr
 * @param wlr_seat_ptr
 * @param root_ptr
 * @param injected_surface_configure
 * @param injected_send_locked
 *
 * @return The lock handle or NULL on error.
 */
wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr,
    _wlmaker_lock_surface_configure_t injected_surface_configure,
    _wlmaker_lock_send_locked_t injected_send_locked)
{
    wlmaker_lock_t *lock_ptr = logged_calloc(1, sizeof(wlmaker_lock_t));
    if (NULL == lock_ptr) return NULL;
    lock_ptr->wlr_session_lock_v1_ptr = wlr_session_lock_v1_ptr;
    lock_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    lock_ptr->wlr_seat_ptr = wlr_seat_ptr;
    lock_ptr->root_ptr = root_ptr;
    lock_ptr->injected_surface_configure = injected_surface_configure;
    lock_ptr->injected_send_locked = injected_send_locked;

    lock_ptr->output_tracker_ptr = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        lock_ptr,
        _wlmaker_lock_output_create,
        _wlmaker_lock_output_update,
        _wlmaker_lock_output_destroy);

    if (!wlmtk_container_init(&lock_ptr->container)) {
        _wlmaker_lock_destroy(lock_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.new_surface,
        &lock_ptr->new_surface_listener,
        _wlmaker_lock_handle_new_surface);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.unlock,
        &lock_ptr->unlock_listener,
        _wlmaker_lock_handle_unlock);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.destroy,
        &lock_ptr->destroy_listener,
        _wlmaker_lock_handle_destroy);

    return lock_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the session lock handle.
 *
 * @param lock_ptr
 */
void _wlmaker_lock_destroy(wlmaker_lock_t *lock_ptr)
{
    if (NULL != lock_ptr->output_tracker_ptr) {
        wlmtk_output_tracker_destroy(lock_ptr->output_tracker_ptr);
        lock_ptr->output_tracker_ptr = NULL;
    }

    wl_list_remove(&lock_ptr->destroy_listener.link);
    wl_list_remove(&lock_ptr->unlock_listener.link);
    wl_list_remove(&lock_ptr->new_surface_listener.link);

    wlmtk_root_lock_unreference(lock_ptr->root_ptr,
                                _wlmaker_lock_element(lock_ptr));
    wlmtk_container_fini(&lock_ptr->container);

    free(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * @returns Pointer to @ref wlmtk_element_t of @ref wlmaker_lock_t::container.
 * */
wlmtk_element_t *_wlmaker_lock_element(wlmaker_lock_t *lock_ptr)
{
    return &lock_ptr->container.super_element;
}

/* ------------------------------------------------------------------------- */
/** Locks the session, if all output surfaces are ready & not locked yet. */
void _wlmaker_lock_if_ready(wlmaker_lock_t *lock_ptr)
{
    if (bs_dllist_empty(&lock_ptr->outputs)) return;
    if (!bs_dllist_all(&lock_ptr->outputs,
                       _wlmaker_lock_output_surface_is_committed,
                       NULL)) return;
    if (wlmtk_root_locked(lock_ptr->root_ptr)) return;

    if (!wlmtk_root_lock(lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr))) {
        if (NULL != lock_ptr->wlr_session_lock_v1_ptr->resource) {
            wl_resource_post_error(
                lock_ptr->wlr_session_lock_v1_ptr->resource,
                WL_DISPLAY_ERROR_INVALID_METHOD,
                "Failed wlmtk_root_lock(%p, %p): Already locked?",
                lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr));
        }
        return;
    }
    wlmtk_element_set_visible(&lock_ptr->container.super_element, true);

    // Grant keyboard focus to the first-found surface that's committed.
    bs_dllist_node_t *dlnode_ptr = bs_dllist_find(
        &lock_ptr->outputs,
        _wlmaker_lock_output_surface_is_committed,
        NULL);
    if (NULL == dlnode_ptr) return;
    wlmaker_lock_output_t *lock_output_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_lock_output_t, dlnode);
    wlmtk_surface_set_activated(lock_output_ptr->wlmtk_surface_ptr, true);

    // Root is locked. Send confirmation to the client.
    lock_ptr->injected_send_locked(lock_ptr->wlr_session_lock_v1_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_surface` signal of `wlr_session_lock_v1`: Creates the
 * associated surface and enables it on the screenlock container.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, new_surface_listener);
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        data_ptr;

    // Guard clause: We expect the output to be set.
    if (NULL == wlr_session_lock_surface_v1_ptr->output) {
        bs_log(BS_ERROR, "Session lock surface %p does not have an output!",
               wlr_session_lock_surface_v1_ptr);
        if (NULL != wlr_session_lock_surface_v1_ptr->resource) {
            wl_resource_post_error(
                wlr_session_lock_surface_v1_ptr->resource,
                WL_DISPLAY_ERROR_INVALID_METHOD,
                "Session lock surface does not have an output!");
        }
        return;
    }

    // Additionally, we expect the output to be part of the output layout.
    wlmaker_lock_output_t *lock_output_ptr = wlmtk_output_tracker_get_output(
        lock_ptr->output_tracker_ptr,
        wlr_session_lock_surface_v1_ptr->output);
    if (NULL == lock_output_ptr) {
        bs_log(BS_ERROR, "Session lock surface %p refers to invalid output %p",
               wlr_session_lock_surface_v1_ptr,
               wlr_session_lock_surface_v1_ptr->output);
        if (NULL != wlr_session_lock_surface_v1_ptr->resource) {
            wl_resource_post_error(
                wlr_session_lock_surface_v1_ptr->resource,
                WL_DISPLAY_ERROR_INVALID_METHOD,
                "Session lock surface refers to invalid output!");
        }
        return;
    }

    if (!_wlmaker_lock_output_create_surface(
            lock_output_ptr,
            wlr_session_lock_surface_v1_ptr,
            lock_ptr)) {
        wl_resource_post_error(
            wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmaker_lock_output_create_surface(%p, %p, %p)",
            lock_output_ptr,
            wlr_session_lock_surface_v1_ptr->surface,
            lock_ptr);
        return;
    }

    bs_log(BS_INFO, "Lock %p, output %p: New lock surface %p",
           lock_ptr, lock_output_ptr, wlr_session_lock_surface_v1_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unlock` signal of `wlr_session_lock_v1`: Marks the session
 * as unlocked.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    __UNUSED__ wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, unlock_listener);

    wlmtk_element_set_visible(&lock_ptr->container.super_element, false);
    wlmtk_root_unlock(lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr));
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_v1`: Destroy the lock.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, destroy_listener);
    _wlmaker_lock_destroy(lock_ptr);
}

/* == Lock output methods ================================================== */

/* ------------------------------------------------------------------------- */
/** Ctor for the lock output. */
void *_wlmaker_lock_output_create(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    wlmaker_lock_t *lock_ptr = ud_ptr;
    wlmaker_lock_output_t *lock_output_ptr = logged_calloc(
        1, sizeof(wlmaker_lock_output_t));
    if (NULL == lock_output_ptr) return NULL;
    lock_output_ptr->lock_ptr = lock_ptr;

    bs_dllist_push_back(&lock_ptr->outputs, &lock_output_ptr->dlnode);
    return lock_output_ptr;
}

/* ------------------------------------------------------------------------- */
/** Layout update: Dimensions of the surface might have changed. Update. */
void _wlmaker_lock_output_update(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    wlmaker_lock_t *lock_ptr = ud_ptr;
    wlmaker_lock_output_t *lock_output_ptr = output_ptr;

    // The output dimensions may have changed. Send a configure().
    lock_ptr->injected_surface_configure(
        lock_output_ptr->wlr_session_lock_surface_v1_ptr,
        lock_output_ptr->wlr_session_lock_surface_v1_ptr->output->width,
        lock_output_ptr->wlr_session_lock_surface_v1_ptr->output->height);

    struct wlr_box box;
    wlr_output_layout_get_box(
        lock_ptr->wlr_output_layout_ptr,
        lock_output_ptr->wlr_session_lock_surface_v1_ptr->output,
        &box);
    wlmtk_element_set_position(
        wlmtk_surface_element(lock_output_ptr->wlmtk_surface_ptr),
        box.x, box.y);
}

/* ------------------------------------------------------------------------- */
/** Dtor for the lock output. */
void _wlmaker_lock_output_destroy(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    wlmaker_lock_t *lock_ptr = ud_ptr;
    wlmaker_lock_output_t *lock_output_ptr = output_ptr;

    _wlmaker_lock_output_destroy_surface(lock_output_ptr);
    bs_dllist_remove(
        &lock_output_ptr->lock_ptr->outputs,
        &lock_output_ptr->dlnode);
    free(lock_output_ptr);

    // Activating the first-found surface ensures there's still one that
    // is activated.
    bs_dllist_node_t *dlnode_ptr = bs_dllist_find(
        &lock_ptr->outputs,
        _wlmaker_lock_output_surface_is_committed,
        NULL);
    if (NULL == dlnode_ptr) return;
    lock_output_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_lock_output_t, dlnode);
    wlmtk_surface_set_activated(lock_output_ptr->wlmtk_surface_ptr, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a lock surface.
 *
 * @param lock_output_ptr
 * @param wlr_session_lock_surface_v1_ptr
 * @param lock_ptr
 *
 * @return The lock surface or NULL on error.
 */
bool _wlmaker_lock_output_create_surface(
    wlmaker_lock_output_t *lock_output_ptr,
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr)
{
    if (NULL != lock_output_ptr->wlr_session_lock_surface_v1_ptr) {
        bs_log(BS_ERROR, "Lock %p, output %p already has surface %p (vs %p)",
               lock_ptr,
               lock_output_ptr,
               lock_output_ptr->wlr_session_lock_surface_v1_ptr,
               wlr_session_lock_surface_v1_ptr);
        return false;
    }
    lock_output_ptr->wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_ptr;

    lock_output_ptr->wlmtk_surface_ptr = wlmtk_surface_create(
        wlr_session_lock_surface_v1_ptr->surface,
        lock_ptr->wlr_seat_ptr);
    if (NULL == lock_output_ptr->wlmtk_surface_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_surface_create(%p)",
               wlr_session_lock_surface_v1_ptr->surface);
        _wlmaker_lock_output_destroy_surface(lock_output_ptr);
        return false;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_session_lock_surface_v1_ptr->events.destroy,
        &lock_output_ptr->destroy_listener,
        _wlmaker_lock_output_handle_surface_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_session_lock_surface_v1_ptr->surface->events.commit,
        &lock_output_ptr->surface_commit_listener,
        _wlmaker_lock_output_handle_surface_commit);

    // We need computed & scaled output resolution for setting the lock
    // surface's dimensions.
    int w, h;
    wlr_output_effective_resolution(
        wlr_session_lock_surface_v1_ptr->output, &w, &h);
    lock_output_ptr->configure_serial = lock_ptr->injected_surface_configure(
        wlr_session_lock_surface_v1_ptr, w, h);

    struct wlr_box box;
    wlr_output_layout_get_box(
        lock_ptr->wlr_output_layout_ptr,
        wlr_session_lock_surface_v1_ptr->output,
        &box);
    wlmtk_element_set_position(
        wlmtk_surface_element(lock_output_ptr->wlmtk_surface_ptr),
        box.x, box.y);

    wlmtk_container_add_element(
        &lock_ptr->container,
        wlmtk_surface_element(lock_output_ptr->wlmtk_surface_ptr));
    wlmtk_element_set_visible(
        wlmtk_surface_element(lock_output_ptr->wlmtk_surface_ptr), true);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the lock surface.
 *
 * @param lock_output_ptr
 */
void _wlmaker_lock_output_destroy_surface(
    wlmaker_lock_output_t *lock_output_ptr)
{
    bs_log(BS_INFO, "Lock %p, output %p: Destroying lock surface %p",
           lock_output_ptr->lock_ptr,
           lock_output_ptr,
           lock_output_ptr->wlr_session_lock_surface_v1_ptr);

    if (NULL != lock_output_ptr->wlmtk_surface_ptr) {
        wlmtk_container_remove_element(
            &lock_output_ptr->lock_ptr->container,
            wlmtk_surface_element(lock_output_ptr->wlmtk_surface_ptr));

        wl_list_remove(&lock_output_ptr->surface_commit_listener.link);
        wl_list_remove(&lock_output_ptr->destroy_listener.link);

        wlmtk_surface_destroy(lock_output_ptr->wlmtk_surface_ptr);
        lock_output_ptr->wlmtk_surface_ptr =  NULL;
    }

    lock_output_ptr->committed = false;
    lock_output_ptr->wlr_session_lock_surface_v1_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_surface_v1`: Destroy
 * the surface.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_output_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_output_t *lock_output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_output_t, destroy_listener);
    _wlmaker_lock_output_destroy_surface(lock_output_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal of `wlr_session_lock_surface_v1::surface`.
 *
 * Checks whether the serial is at-or-above the 'configure' serial, and
 * reports the surface and output as locked. Once all surfaces are locked,
 * a 'send_locked' event will be sent.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_output_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_output_t *lock_output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_output_t, surface_commit_listener);
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        lock_output_ptr->wlr_session_lock_surface_v1_ptr;

    // Do not accept locking for commits before the requested configuration.
    if (wlr_session_lock_surface_v1_ptr->current.configure_serial >=
        lock_output_ptr->configure_serial) {
        lock_output_ptr->committed = true;
        _wlmaker_lock_if_ready(lock_output_ptr->lock_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Iterator for @ref wlmaker_lock_t::outputs. Is the output committed? */
bool _wlmaker_lock_output_surface_is_committed(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    wlmaker_lock_output_t *lock_output_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_lock_output_t, dlnode);
    return (NULL != lock_output_ptr->wlr_session_lock_surface_v1_ptr &&
            lock_output_ptr->committed);
}

/* == Unit tests =========================================================== */

static void test_lock_unlock(bs_test_t *test_ptr);
static void test_lock_crash(bs_test_t *test_ptr);
static void test_lock_multi_output(bs_test_t *test_ptr);

static uint32_t _mock_wlr_session_lock_surface_v1_configure(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height);
static void _mock_wlr_session_lock_v1_send_locked(
    struct wlr_session_lock_v1 *lock);
static void _init_test_session_lock(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr);
static void _init_test_surface(struct wlr_surface *wlr_surface_ptr);

/** Unit test cases. */
const bs_test_case_t wlmaker_lock_mgr_test_cases[] = {
    { true, "lock_unlock", test_lock_unlock },
    { true, "lock_crash", test_lock_crash },
    { true, "lock_multi_output", test_lock_multi_output },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_lock_mgr_test_set = BS_TEST_SET(
    true, "lock_mgr", wlmaker_lock_mgr_test_cases);

/** Return value for @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_serial;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_width;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_height;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static struct wlr_session_lock_surface_v1 *_mock_configure_lock_surface;
/** Arg of last call to @ref _mock_wlr_session_lock_v1_send_locked. */
static struct wlr_session_lock_v1 *_mock_send_locked_lock;

/** Mock for configure(). */
uint32_t _mock_wlr_session_lock_surface_v1_configure(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height)
{
    _mock_configure_lock_surface = lock_surface;
    _mock_configure_width = width;
    _mock_configure_height = height;
    return _mock_configure_serial;
}

/** Mock for send_locked(). */
void _mock_wlr_session_lock_v1_send_locked(
    struct wlr_session_lock_v1 *lock)
{
    _mock_send_locked_lock = lock;
}

/** Initializes the minimum required attributes of the session lock. */
void _init_test_session_lock(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr)
{
    wl_signal_init(&wlr_session_lock_v1_ptr->events.new_surface);
    wl_signal_init(&wlr_session_lock_v1_ptr->events.unlock);
    wl_signal_init(&wlr_session_lock_v1_ptr->events.destroy);
}

/** Initializes the minimum required attributes of the wlr_surface. */
void _init_test_surface(struct wlr_surface *wlr_surface_ptr)
{
    wl_list_init(&wlr_surface_ptr->current.subsurfaces_below);
    wl_list_init(&wlr_surface_ptr->current.subsurfaces_above);
    wl_signal_init(&wlr_surface_ptr->events.commit);
    wl_signal_init(&wlr_surface_ptr->events.destroy);
    wl_signal_init(&wlr_surface_ptr->events.map);
    wl_signal_init(&wlr_surface_ptr->events.unmap);
}

/* ------------------------------------------------------------------------- */
/** Tests locking & unlocking, proper sequence, single output. */
void test_lock_unlock(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &output);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface = {};
    _init_test_surface(&wlr_surface);
    struct wlr_session_lock_surface_v1 lock_surface = {
        .surface = &wlr_surface,
        .output = &output,
    };
    wl_signal_init(&lock_surface.events.destroy);

    // A new surface request will be greeted by a configure() event.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    // A commit, but with too-low serial. Will be ignored.
    lock_surface.current.configure_serial = 41;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // Another commit, with matching serial. Will mark as locked.
    wlr_surface.current.width = 1024;
    wlr_surface.current.height = 768;
    lock_surface.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // Client unlocks.
    wl_signal_emit(&wlr_session_lock_v1.events.unlock, NULL);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    _wlmaker_lock_destroy(lock_ptr);
    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests locking, and then the session lock going away: Must remain locked. */
void test_lock_crash(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &output);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface = {};
    _init_test_surface(&wlr_surface);
    struct wlr_session_lock_surface_v1 lock_surface = {
        .surface = &wlr_surface,
        .output = &output,
    };
    wl_signal_init(&lock_surface.events.destroy);

    // A new surface request will be greeted by a configure() event.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    // Commit with matching serial. Will mark as locked.
    wlr_surface.current.width = 1024;
    wlr_surface.current.height = 768;
    lock_surface.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // No unlock. If the session lock is destroyed without: Lock remains.
    _wlmaker_lock_destroy(lock_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests locking with multiple outputs. Lock only when all outputs covered. */
void test_lock_multi_output(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    struct wlr_output o2 = { .width = 1024, .height = 768, .scale = 1 };
    struct wlr_output o3 = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlmtk_test_wlr_output_init(&o2);
    wlmtk_test_wlr_output_init(&o3);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &o1);
    // But not: o2.
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &o3);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface1 = {};
    _init_test_surface(&wlr_surface1);
    struct wlr_session_lock_surface_v1 lock_surface1 = {
        .surface = &wlr_surface1,
        .output = &o1,
    };
    wl_signal_init(&lock_surface1.events.destroy);
    struct wlr_surface wlr_surface2 = {};
    _init_test_surface(&wlr_surface2);
    struct wlr_session_lock_surface_v1 lock_surface2 = {
        .surface = &wlr_surface2,
        .output = &o2,
    };
    wl_signal_init(&lock_surface2.events.destroy);
    struct wlr_surface wlr_surface3 = {};
    _init_test_surface(&wlr_surface3);
    struct wlr_session_lock_surface_v1 lock_surface3 = {
        .surface = &wlr_surface3,
        .output = &o3,
    };
    wl_signal_init(&lock_surface3.events.destroy);

    // Surface 1. Create, configure, commit. No lock yet.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface1);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface1, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    wlr_surface1.current.width = 1024;
    wlr_surface1.current.height = 768;
    lock_surface1.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface1.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wlmtk_surface_t *surface_ptr = wlr_surface1.data;
    int x, y;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y);

    // Surface 2. Create, configure, commit. Non-layout output -> ignored.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface2);
    // no 'configure'.

    wlr_surface2.current.width = 1024;
    wlr_surface2.current.height = 768;
    lock_surface2.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface2.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wl_signal_emit(&lock_surface2.events.destroy, NULL);

    // Surface 3.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface3);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface3, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    wlr_surface3.current.width = 1024;
    wlr_surface3.current.height = 768;
    lock_surface3.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface3.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    surface_ptr = wlr_surface3.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y);

    // o3 changes size & position. Test configure(). Remains locked.
    o3.width = 1920;
    o3.height = 1080;
    wlr_output_layout_add(server.wlr_output_layout_ptr, &o3, 1200, 200);
    _mock_configure_serial = 43;
    _mock_configure_lock_surface = NULL;
    wl_signal_emit(&server.wlr_output_layout_ptr->events.change,
                   server.wlr_output_layout_ptr);
    // Note: Issues two configure() events, the second one is for o3.
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface3, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1920, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 1080, _mock_configure_height);

    wlr_surface3.current.width = 1024;
    wlr_surface3.current.height = 768;
    lock_surface3.current.configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_surface3.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    surface_ptr = wlr_surface1.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 3120, x);
    BS_TEST_VERIFY_EQ(test_ptr, 200, y);

    surface_ptr = wlr_surface3.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 1200, x);
    BS_TEST_VERIFY_EQ(test_ptr, 200, y);

    // Confirm: The earliest added surface is active.
    surface_ptr = wlr_surface1.data;
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_surface_is_activated(surface_ptr));

    wlr_output_layout_remove(server.wlr_output_layout_ptr, &o1);
    _mock_configure_serial = 44;
    _mock_configure_lock_surface = NULL;
    wl_signal_emit(&server.wlr_output_layout_ptr->events.change,
                   server.wlr_output_layout_ptr);

    // Now want surface1 active.
    surface_ptr = wlr_surface3.data;
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_surface_is_activated(surface_ptr));

    // Unlock correctly.
    wl_signal_emit(&wlr_session_lock_v1.events.unlock, NULL);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    _wlmaker_lock_destroy(lock_ptr);
    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* == End of lock_mgr.c ==================================================== */
