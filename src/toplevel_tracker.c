/* ========================================================================= */
/**
 * @file toplevel_tracker.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "toplevel_tracker.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the toplevel tracker. */
struct wlmaker_toplevel_tracker {
    /** The tracked toplevels. */
    bs_dllist_t               toplevels;

    /** Handle for the `ext-foreign-toplevel-list-v1` protocol. */
    struct wlr_ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_ptr;

    /** Connects to a `struct wl_display` destroy notification. */
    struct wl_listener        display_destroy_listener;
};

/** Handle of one tracked toplevel. */
struct wlmaker_toplevel_handle {
    /** Back-link to the toplevel tracker. */
    struct wlmaker_toplevel_tracker *toplevel_tracker_ptr;
    /** List node, to @ref wlmaker_toplevel_tracker::toplevels. */
    bs_dllist_node_t          dlnode;

    /** Handle for this toplevel for the ext-foreign-toplevel list. */
    struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_ptr;
    /** State of the toplevel. */
    struct wlr_ext_foreign_toplevel_handle_v1_state state;

    /** The window for this toplevel. */
    wlmtk_window_t            *window_ptr;
};

static void _wlmaker_toplevel_tracker_destroy(
    struct wlmaker_toplevel_tracker *toplevel_tracker_ptr);
static void _wlmaker_toplevel_tracker_handle_display_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmaker_toplevel_tracker *wlmaker_toplevel_tracker_create(
    struct wl_display *wl_display_ptr)
{
    struct wlmaker_toplevel_tracker *toplevel_tracker_ptr = logged_calloc(
        1, sizeof(*toplevel_tracker_ptr));
    if (NULL == toplevel_tracker_ptr) return NULL;

    // Note: Will automatically be destroyed when wl_display_ptr destroys.
    toplevel_tracker_ptr->ext_foreign_toplevel_list_ptr =
        wlr_ext_foreign_toplevel_list_v1_create(wl_display_ptr, 1);

    toplevel_tracker_ptr->display_destroy_listener.notify =
        _wlmaker_toplevel_tracker_handle_display_destroy;
    wl_display_add_destroy_listener(
        wl_display_ptr, &toplevel_tracker_ptr->display_destroy_listener);
    return toplevel_tracker_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlmaker_toplevel_handle *wlmaker_toplevel_tracker_create_toplevel_handle(
    struct wlmaker_toplevel_tracker *toplevel_tracker_ptr,
    wlmtk_window_t *window_ptr)
{
    struct wlmaker_toplevel_handle *toplevel_ptr = logged_calloc(
        1, sizeof(*toplevel_ptr));
    if (NULL == toplevel_ptr) return NULL;
    toplevel_ptr->window_ptr = window_ptr;

    toplevel_ptr->ext_foreign_toplevel_handle_ptr =
        wlr_ext_foreign_toplevel_handle_v1_create(
            toplevel_tracker_ptr->ext_foreign_toplevel_list_ptr,
            &toplevel_ptr->state);

    bs_dllist_push_back(
        &toplevel_tracker_ptr->toplevels,
        &toplevel_ptr->dlnode);
    toplevel_ptr->toplevel_tracker_ptr = toplevel_tracker_ptr;
    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_toplevel_tracker_destroy_toplevel_handle(
    struct wlmaker_toplevel_handle *toplevel_ptr)
{
    if (NULL != toplevel_ptr->ext_foreign_toplevel_handle_ptr) {
        wlr_ext_foreign_toplevel_handle_v1_destroy(
            toplevel_ptr->ext_foreign_toplevel_handle_ptr);
        toplevel_ptr->ext_foreign_toplevel_handle_ptr = NULL;
    }

    if (NULL != toplevel_ptr->toplevel_tracker_ptr) {
        bs_dllist_remove(
            &toplevel_ptr->toplevel_tracker_ptr->toplevels,
            &toplevel_ptr->dlnode);
        toplevel_ptr->toplevel_tracker_ptr = NULL;
    }

    free(toplevel_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Destroys the toplevel tracker. */
void _wlmaker_toplevel_tracker_destroy(
    struct wlmaker_toplevel_tracker *toplevel_tracker_ptr)
{
    wlmtk_util_disconnect_listener(
        &toplevel_tracker_ptr->display_destroy_listener);

    free(toplevel_tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** wl_display destroy handler. Call @ref _wlmaker_toplevel_tracker_destroy. */
void _wlmaker_toplevel_tracker_handle_display_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
     struct wlmaker_toplevel_tracker *toplevel_tracker_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_toplevel_tracker,
        display_destroy_listener);
    _wlmaker_toplevel_tracker_destroy(toplevel_tracker_ptr);
}

/* == Unit Tests =========================================================== */

/* == End of toplevel_tracker.c ============================================ */
