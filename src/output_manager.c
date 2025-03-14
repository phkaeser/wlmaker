/* ========================================================================= */
/**
 * @file output_manager.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "output_manager.h"

#include <libbase/libbase.h>
#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_management_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Implementation of the wlr output manager. */
struct _wlmaker_output_manager_t {
    /** Points to wlroots `struct wlr_output_manager_v1`. */
    struct wlr_output_manager_v1 *wlr_output_manager_v1_ptr;

    /** Listener for wlr_output_manager_v1::events.destroy. */
    struct wl_listener        destroy_listener;
};

static void _wlmaker_output_manager_destroy(
    wlmaker_output_manager_t *output_mgr_ptr);

static void _wlmaker_output_manager_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_output_manager_t *wlmaker_output_manager_create(
    struct wl_display *wl_display_ptr)
{
    wlmaker_output_manager_t *output_mgr_ptr = logged_calloc(
        1, sizeof(wlmaker_output_manager_t));
    if (NULL == output_mgr_ptr) return NULL;

    output_mgr_ptr->wlr_output_manager_v1_ptr =
        wlr_output_manager_v1_create(wl_display_ptr);
    if (NULL == output_mgr_ptr->wlr_output_manager_v1_ptr) {
        _wlmaker_output_manager_destroy(output_mgr_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &output_mgr_ptr->wlr_output_manager_v1_ptr->events.destroy,
        &output_mgr_ptr->destroy_listener,
        _wlmaker_output_manager_handle_destroy);

    return output_mgr_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Dtor. */
void _wlmaker_output_manager_destroy(wlmaker_output_manager_t *output_mgr_ptr)
{
    if (NULL != output_mgr_ptr->wlr_output_manager_v1_ptr) {
        wlmtk_util_disconnect_listener(
            &output_mgr_ptr->destroy_listener);
        output_mgr_ptr->wlr_output_manager_v1_ptr = NULL;
    }

    free(output_mgr_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.destroy. Cleans up. */
void _wlmaker_output_manager_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_output_manager_t *output_mgr_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_manager_t, destroy_listener);
    _wlmaker_output_manager_destroy(output_mgr_ptr);
}

/* == End of output_manager.c ============================================== */
