/* ========================================================================= */
/**
 * @file xdg_toplevel.c
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

#include "xdg_toplevel.h"

/* == Declarations ========================================================= */

/** State of the content for an XDG toplevel surface. */
typedef struct {
    /** Super class. */
    wlmtk_content_t           super_content;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

    /** The corresponding wlroots XDG surface. */
    struct wlr_xdg_surface    *wlr_xdg_surface_ptr;

    /** Listener for the `map` signal of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;
} wlmtk_xdg_toplevel_content_t;

static wlmtk_xdg_toplevel_content_t *xdg_toplevel_content_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr);
static void xdg_toplevel_content_destroy(
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr);


static void handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void content_destroy(wlmtk_content_t *content_ptr);
static struct wlr_scene_node *content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr);
static void content_set_active(
    wlmtk_content_t *content_ptr,
    bool active);

/* == Data ================================================================= */

/** Method table for the `wlmtk_content_t` virtual methods. */
const wlmtk_content_impl_t    content_impl = {
    .destroy = content_destroy,
    .create_scene_node = content_create_scene_node,
    .get_size = content_get_size,
    .set_active = content_set_active,
};


/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create_from_xdg_toplevel(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmtk_xdg_toplevel_content_t *content_ptr = xdg_toplevel_content_create(
        wlr_xdg_surface_ptr, server_ptr);
    if (NULL == content_ptr) return NULL;

    wlmtk_window_t *wlmtk_window_ptr = wlmtk_window_create(
        &content_ptr->super_content);
    if (NULL == wlmtk_window_ptr) {
        wlmtk_content_destroy(&content_ptr->super_content);
        return NULL;
    }

    return wlmtk_window_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
wlmtk_xdg_toplevel_content_t *xdg_toplevel_content_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = logged_calloc(
        1, sizeof(wlmtk_xdg_toplevel_content_t));
    if (NULL == xdg_tl_content_ptr) return NULL;

    if (!wlmtk_content_init(&xdg_tl_content_ptr->super_content,
                            &content_impl,
                            server_ptr->wlr_seat_ptr)) {
        xdg_toplevel_content_destroy(xdg_tl_content_ptr);
        return NULL;
    }
    xdg_tl_content_ptr->wlr_xdg_surface_ptr = wlr_xdg_surface_ptr;
    xdg_tl_content_ptr->server_ptr = server_ptr;

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.map,
        &xdg_tl_content_ptr->surface_map_listener,
        handle_surface_map);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.unmap,
        &xdg_tl_content_ptr->surface_unmap_listener,
        handle_surface_unmap);

    return xdg_tl_content_ptr;
}

/* ------------------------------------------------------------------------- */
void xdg_toplevel_content_destroy(
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr)
{
    wl_list_remove(&xdg_tl_content_ptr->surface_map_listener.link);
    wl_list_remove(&xdg_tl_content_ptr->surface_unmap_listener.link);

    wlmtk_content_fini(&xdg_tl_content_ptr->super_content);
    free(xdg_tl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor. Wraps to @ref wlmtk_xdg_toplevel_content_destroy.
 *
 * @param content_ptr
 */
void content_destroy(wlmtk_content_t *content_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);
    xdg_toplevel_content_destroy(xdg_tl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates the wlroots scene graph API node, attached to `wlr_scene_tree_ptr`.
 *
 * @param content_ptr
 * @param wlr_scene_tree_ptr
 *
 * @return Scene graph API node that represents the content.
 */
struct wlr_scene_node *content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);

    struct wlr_scene_tree *surface_wlr_scene_tree_ptr =
        wlr_scene_xdg_surface_create(
            wlr_scene_tree_ptr,
            xdg_tl_content_ptr->wlr_xdg_surface_ptr);
    return &surface_wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
/**
 * Gets the dimensions of the element in pixels, relative to the position.
 *
 * @param content_ptr
 * @param width_ptr            Width of content. May be NULL.
 * @param height_ptr           Height of content. May be NULL.
 */
void content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->toplevel->base, &geo_box);
    if (NULL != width_ptr) *width_ptr = geo_box.width;
    if (NULL != height_ptr) *height_ptr = geo_box.height;
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the keyboard activation status for the surface.
 *
 * @param content_ptr
 * @param active
 */
void content_set_active(
    wlmtk_content_t *content_ptr,
    bool active)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);

    wlr_xdg_toplevel_set_activated(
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->toplevel, active);

    if (active) {
        struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
            xdg_tl_content_ptr->server_ptr->wlr_seat_ptr);
        if (NULL != wlr_keyboard_ptr) {
            wlr_seat_keyboard_notify_enter(
                xdg_tl_content_ptr->server_ptr->wlr_seat_ptr,
                xdg_tl_content_ptr->wlr_xdg_surface_ptr->surface,
                wlr_keyboard_ptr->keycodes,
                wlr_keyboard_ptr->num_keycodes,
                &wlr_keyboard_ptr->modifiers);
        }
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal.
 *
 * Issued when the XDG toplevel is fully configured and ready to be shown.
 * Will add it to the current workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_toplevel_content_t, surface_map_listener);

    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        wlmaker_server_get_current_workspace(xdg_tl_content_ptr->server_ptr));

    wlmtk_workspace_map_window(
        wlmtk_workspace_ptr,
        xdg_tl_content_ptr->super_content.window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal. Removes it from the workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_toplevel_content_t, surface_unmap_listener);

    wlmtk_window_t *window_ptr = xdg_tl_content_ptr->super_content.window_ptr;
    wlmtk_workspace_unmap_window(
        wlmtk_workspace_from_container(
            wlmtk_window_element(window_ptr)->parent_container_ptr),
        window_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
