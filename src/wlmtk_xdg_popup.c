/* ========================================================================= */
/**
 * @file wlmtk_xdg_popup.c
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

#include "wlmtk_xdg_popup.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static struct wlr_scene_node *_wlmtk_xdg_popup_surface_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/** Virtual methods for XDG popup surface, for the Element superclass. */
const wlmtk_element_vmt_t     _wlmtk_xdg_popup_surface_element_vmt = {
    .create_scene_node = _wlmtk_xdg_popup_surface_element_create_scene_node,
};

static void handle_reposition(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_xdg_popup_t *wlmtk_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = logged_calloc(
        1, sizeof(wlmtk_xdg_popup_t));
    if (NULL == xdg_popup_ptr) return NULL;
    xdg_popup_ptr->wlr_xdg_popup_ptr = wlr_xdg_popup_ptr;

    if (!wlmtk_surface_init(
            &xdg_popup_ptr->surface,
            wlr_xdg_popup_ptr->base->surface,
            env_ptr)) {
        wlmtk_xdg_popup_destroy(xdg_popup_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &xdg_popup_ptr->surface.super_element,
        &_wlmtk_xdg_popup_surface_element_vmt);

    if (!wlmtk_content_init(
            &xdg_popup_ptr->super_content,
            &xdg_popup_ptr->surface,
            env_ptr)) {
        wlmtk_xdg_popup_destroy(xdg_popup_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->events.reposition,
        &xdg_popup_ptr->reposition_listener,
        handle_reposition);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.destroy,
        &xdg_popup_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.new_popup,
        &xdg_popup_ptr->new_popup_listener,
        handle_new_popup);

    return xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_xdg_popup_destroy(wlmtk_xdg_popup_t *xdg_popup_ptr)
{
    wl_list_remove(&xdg_popup_ptr->new_popup_listener.link);
    wl_list_remove(&xdg_popup_ptr->destroy_listener.link);
    wl_list_remove(&xdg_popup_ptr->reposition_listener.link);

    wlmtk_content_fini(&xdg_popup_ptr->super_content);
    wlmtk_surface_fini(&xdg_popup_ptr->surface);
    free(xdg_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::create_scene_node. Create node. */
struct wlr_scene_node *_wlmtk_xdg_popup_surface_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_xdg_popup_t, surface.super_element);

    struct wlr_scene_tree *surface_wlr_scene_tree_ptr =
        wlr_scene_xdg_surface_create(
            wlr_scene_tree_ptr,
            xdg_popup_ptr->wlr_xdg_popup_ptr->base);
    return &surface_wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
/** Handles repositioning. Yet unimplemented. */
void handle_reposition(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_popup_t, reposition_listener);

    bs_log(BS_WARNING, "Unhandled: reposition on XDG popup %p", xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles popup destruction: Removes from parent content, and destroy. */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_popup_t, destroy_listener);

    wlmtk_element_t *element_ptr = wlmtk_content_element(
        &xdg_popup_ptr->super_content);
    wlmtk_container_remove_element(
        element_ptr->parent_container_ptr,
        element_ptr);

    wlmtk_xdg_popup_destroy(xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles further popups. Yet unimplemented. */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_popup_t, new_popup_listener);

    bs_log(BS_WARNING, "Unhandled: new_popup on XDG popup %p", xdg_popup_ptr);
}

/* == End of wlmtk_xdg_popup.c ============================================= */
