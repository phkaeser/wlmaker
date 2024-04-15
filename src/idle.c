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

/* == Declarations ========================================================= */

/** State of the idle monitor. */
struct _wlmaker_idle_monitor_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Reference to the event loop. */
    struct wl_event_loop      *wl_event_loop_ptr;
    /** The timer's event source. */
    struct wl_event_source    *timer_event_source_ptr;

    // FIXME: When "unlock" happens: re-arm the timer.
};

static int _wlmaker_idle_monitor_timer(void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_idle_monitor_t *wlmaker_idle_monitor_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_idle_monitor_t *idle_monitor_ptr = logged_calloc(
        1, sizeof(wlmaker_idle_monitor_t));
    if (NULL == idle_monitor_ptr) return NULL;
    idle_monitor_ptr->server_ptr = server_ptr;  // FIXME: needed?
    idle_monitor_ptr->wl_event_loop_ptr = wl_display_get_event_loop(
        server_ptr->wl_display_ptr);

    idle_monitor_ptr->timer_event_source_ptr = wl_event_loop_add_timer(
        idle_monitor_ptr->wl_event_loop_ptr,
        _wlmaker_idle_monitor_timer,
        idle_monitor_ptr);
    if (NULL == idle_monitor_ptr->timer_event_source_ptr) {
        bs_log(BS_ERROR, "Failed wl_event_loop_add_timer(%p, %p, %p)",
               idle_monitor_ptr->wl_event_loop_ptr,
               _wlmaker_idle_monitor_timer,
               idle_monitor_ptr);
        wlmaker_idle_monitor_destroy(idle_monitor_ptr);
        return NULL;
    }

    if (0 != wl_event_source_timer_update(
            idle_monitor_ptr->timer_event_source_ptr,
            config_idle_lock_msec)) {
        bs_log(BS_ERROR, "Failed wl_event_source_timer_update(%p, 1000)",
               idle_monitor_ptr->timer_event_source_ptr);
        wlmaker_idle_monitor_destroy(idle_monitor_ptr);
        return NULL;
    }

    return idle_monitor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_idle_monitor_destroy(wlmaker_idle_monitor_t *idle_monitor_ptr)
{
    if (NULL != idle_monitor_ptr->timer_event_source_ptr) {
        wl_event_source_remove(idle_monitor_ptr->timer_event_source_ptr);
        idle_monitor_ptr->timer_event_source_ptr = NULL;
    }

    free(idle_monitor_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_idle_monitor_reset(wlmaker_idle_monitor_t *idle_monitor_ptr)
{
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

    // FIXME: Issue a lock.
    bs_log(BS_ERROR, "FIXME: Timer at %p", idle_monitor_ptr);

    return 0;
}

/* == End of idle.c ======================================================== */
