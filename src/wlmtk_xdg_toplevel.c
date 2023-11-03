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
    /** Whether this surface is currently activated. */
    bool                      activated;

    /** Listener for the `map` signal of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;
    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Listener for `request_move` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_move_listener;
    /** Listener for `request_resize` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_resize_listener;
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
static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_move(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void content_destroy(wlmtk_content_t *content_ptr);
static struct wlr_scene_node *content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static uint32_t content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/* == Data ================================================================= */

/** Method table for the `wlmtk_content_t` virtual methods. */
const wlmtk_content_impl_t    content_impl = {
    .destroy = content_destroy,
    .create_scene_node = content_create_scene_node,
    .request_size = content_request_size,
    .set_activated = content_set_activated,
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
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.commit,
        &xdg_tl_content_ptr->surface_commit_listener,
        handle_surface_commit);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_move,
        &xdg_tl_content_ptr->toplevel_request_move_listener,
        handle_toplevel_request_move);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_resize,
        &xdg_tl_content_ptr->toplevel_request_resize_listener,
        handle_toplevel_request_resize);

    xdg_tl_content_ptr->wlr_xdg_surface_ptr->data =
        &xdg_tl_content_ptr->super_content;

    // FIXME
    xdg_tl_content_ptr->super_content.wlr_surface_ptr =
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->surface;

    return xdg_tl_content_ptr;
}

/* ------------------------------------------------------------------------- */
void xdg_toplevel_content_destroy(
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr)
{
    wl_list_remove(&xdg_tl_content_ptr->toplevel_request_resize_listener.link);
    wl_list_remove(&xdg_tl_content_ptr->toplevel_request_move_listener.link);

    wl_list_remove(&xdg_tl_content_ptr->surface_commit_listener.link);
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
 * Sets the dimensions of the element in pixels.
 *
 * @param content_ptr
 * @param width               Width of content.
 * @param height              Height of content.
 *
 * @return The serial.
 */
uint32_t content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);

    return wlr_xdg_toplevel_set_size(
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->toplevel, width, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the keyboard activation status for the surface.
 *
 * @param content_ptr
 * @param activated
 */
void content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);
    // Early return, if nothing to be done.
    if (xdg_tl_content_ptr->activated == activated) return;

    struct wlr_seat *wlr_seat_ptr =
        xdg_tl_content_ptr->server_ptr->wlr_seat_ptr;
    wlr_xdg_toplevel_set_activated(
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->toplevel, activated);

    if (activated) {
        struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
            wlr_seat_ptr);
        if (NULL != wlr_keyboard_ptr) {
            wlr_seat_keyboard_notify_enter(
                wlr_seat_ptr,
                xdg_tl_content_ptr->wlr_xdg_surface_ptr->surface,
                wlr_keyboard_ptr->keycodes,
                wlr_keyboard_ptr->num_keycodes,
                &wlr_keyboard_ptr->modifiers);
        }
    } else {
        BS_ASSERT(xdg_tl_content_ptr->activated);
        // FIXME: This clears pointer focus. But, this is keyboard focus?
        if (wlr_seat_ptr->keyboard_state.focused_surface ==
            xdg_tl_content_ptr->wlr_xdg_surface_ptr->surface) {
            wlr_seat_pointer_clear_focus(wlr_seat_ptr);
        }
    }

    xdg_tl_content_ptr->activated = activated;
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

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the const struct wlr_surface.
 */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_xdg_toplevel_content_t, surface_commit_listener);

    if (NULL == xdg_tl_content_ptr->wlr_xdg_surface_ptr) return;

    wlmtk_content_commit_size(
        &xdg_tl_content_ptr->super_content,
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->current.configure_serial,
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->current.geometry.width,
        xdg_tl_content_ptr->wlr_xdg_surface_ptr->current.geometry.height);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_move` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_request_move(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        listener_ptr,
        wlmtk_xdg_toplevel_content_t,
        toplevel_request_move_listener);
    wlmtk_window_request_move(xdg_tl_content_ptr->super_content.window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `requestp_resize` signal.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a struct wlr_xdg_toplevel_resize_event.
 */
void handle_toplevel_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        listener_ptr,
        wlmtk_xdg_toplevel_content_t,
        toplevel_request_resize_listener);
    struct wlr_xdg_toplevel_resize_event *resize_event_ptr = data_ptr;
    wlmtk_window_request_resize(
        xdg_tl_content_ptr->super_content.window_ptr,
        resize_event_ptr->edges);
}

/* == End of xdg_toplevel.c ================================================ */
