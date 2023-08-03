/* ========================================================================= */
/**
 * @file xdg_popup.c
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

#include "xdg_popup.h"

#include <libbase/libbase.h>

#include "util.h"

/* == Declarations ========================================================= */

/** State of the XDG popup handle. */
struct _wlmaker_xdg_popup_t {
    /** Links to the corresponding wlroots XDG popup. */
    struct wlr_xdg_popup      *wlr_xdg_popup_ptr;
    /** Scene node of this popup surfaces (and it's sub-surfaces). */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Listener for the `destroy` signal of the `wlr_xdg_surface` (base). */
    struct wl_listener        destroy_listener;
    /** Listener for the `new_popup` signal of the `wlr_xdg_surface` (base). */
    struct wl_listener        new_popup_listener;
};

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr)
{
    wlmaker_xdg_popup_t *xdg_popup_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_popup_t));
    if (NULL == xdg_popup_ptr) return NULL;
    xdg_popup_ptr->wlr_xdg_popup_ptr = wlr_xdg_popup_ptr;

    wlm_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.destroy,
        &xdg_popup_ptr->destroy_listener,
        handle_destroy);
    wlm_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.new_popup,
        &xdg_popup_ptr->new_popup_listener,
        handle_new_popup);

    xdg_popup_ptr->wlr_scene_tree_ptr = wlr_scene_xdg_surface_create(
        parent_wlr_scene_tree_ptr,
        wlr_xdg_popup_ptr->base);
    if (NULL == xdg_popup_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_xdg_surface_create().");
        wlmaker_xdg_popup_destroy(xdg_popup_ptr);
        return NULL;
    }

    bs_log(BS_INFO, "Created XDG popup %p, from surface %p (parent tree %p)",
           xdg_popup_ptr, wlr_xdg_popup_ptr->base, wlr_xdg_popup_ptr->parent);
    return xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_t *xdg_popup_ptr)
{
    wl_list_remove(&xdg_popup_ptr->destroy_listener.link);
    wl_list_remove(&xdg_popup_ptr->new_popup_listener.link);
    free(xdg_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the `wlr_xdg_surface` (base). Calls
 * into wlmaker_xdg_popup_destroy().
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, destroy_listener);
    wlmaker_xdg_popup_destroy(xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_popup` signal of the `wlr_xdg_surface` (base).
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    // This popup is eg. invoked when opening a nested sub-menu on Google
    // Chrome.
    wlmaker_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *new_xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr, xdg_popup_ptr->wlr_scene_tree_ptr);
    bs_log(BS_INFO, "Created XDG popup %p.", new_xdg_popup_ptr);
}

/* == End of xdg_popup.c ================================================== */
