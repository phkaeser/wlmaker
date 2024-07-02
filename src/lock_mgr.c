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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_session_lock_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

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

static void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

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

/* == End of lock_mgr.c ==================================================== */
