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

#include "iconified.h"
#include "toolkit/toolkit.h"
#include "xdg_popup.h"

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of an XDG toplevel surface. */
struct _wlmaker_xdg_toplevel_t {
    /** State of the view. */
    wlmaker_view_t            view;

    /**
     * The corresponding wlroots xdg_surface.
     *
     * The `data` field of the `struct wlr_xdg_surface` will be set to point
     * to the `struct wlr_scene_tree` of the view.
     */
    struct wlr_xdg_surface    *wlr_xdg_surface_ptr;

    /** Scene graph of all surfaces & sub-surfaces of the XDG toplevel. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** ID of the last 'set_size' call. */
    uint32_t                  pending_resize_serial;

    /** Listener for the `destroy` signal of the `wlr_xdg_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for the `new_popup` signal of the `wlr_xdg_surface`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `map` signal of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;

    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Listener for the `maximize` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_maximize_listener;
    /** Listener for the `fullscreen` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_fullscreen_listener;
    /** Listener for the `minimize` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_minimize_listener;
    /** Listener for the `move` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_move_listener;
    /** Listener for the `request_resize` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_resize_listener;
    /** Listener for the `show_window_menu` signal of `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_request_show_window_menu_listener;
    /** Listener for the `set_parent` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_set_parent_listener;
    /** Listener for the `set_title` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_set_title_listener;
    /** Listener for the `set_app_id` signal of the `wlr_xdg_toplevel`. */
    struct wl_listener        toplevel_set_app_id_listener;

    /** Transitional: Content. */
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr;
};

static wlmaker_xdg_toplevel_t *wlmaker_xdg_toplevel_from_view(
    wlmaker_view_t *view_ptr);
static void wlmaker_xdg_toplevel_send_close_callback(
    wlmaker_view_t *view_ptr);

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_toplevel_maximize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_minimize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_move(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_show_window_menu(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_set_parent(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_set_title(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_set_app_id(
    struct wl_listener *listener_ptr,
    void *data_ptr);



static uint32_t wlmaker_xdg_toplevel_set_activated(
    wlmaker_view_t *view_ptr,
    bool activated);
static void wlmaker_xdg_toplevel_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr);
static void wlmaker_xdg_toplevel_set_size(
    wlmaker_view_t *view_ptr,
    int width, int height);
static void wlmaker_xdg_toplevel_set_maximized(
    wlmaker_view_t *view_ptr,
    bool maximize);
static void wlmaker_xdg_toplevel_set_fullscreen(
    wlmaker_view_t *view_ptr,
    bool fullscreen);


/* ######################################################################### */

/** State of the content for an XDG toplevel surface. */
struct _wlmtk_xdg_toplevel_content_t {
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
};

static void handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     xdg_toplevel_view_impl = {
    .set_activated = wlmaker_xdg_toplevel_set_activated,
    .get_size = wlmaker_xdg_toplevel_get_size,
    .set_size = wlmaker_xdg_toplevel_set_size,
    .set_maximized = wlmaker_xdg_toplevel_set_maximized,
    .set_fullscreen = wlmaker_xdg_toplevel_set_fullscreen
};

/* ######################################################################### */

static void xdg_tl_content_destroy(wlmtk_content_t *content_ptr);
static struct wlr_scene_node *xdg_tl_content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/** Method table for the `wlmtk_content_t` virtual methods. */
const wlmtk_content_impl_t    content_impl = {
    .destroy = xdg_tl_content_destroy,
    .create_scene_node = xdg_tl_content_create_scene_node
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_toplevel_t *wlmaker_xdg_toplevel_create(
    wlmaker_xdg_shell_t *xdg_shell_ptr,
    struct wlr_xdg_surface *wlr_xdg_surface_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_toplevel_t));
    if (NULL == xdg_toplevel_ptr) return NULL;
    xdg_toplevel_ptr->wlr_xdg_surface_ptr = wlr_xdg_surface_ptr;

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->events.destroy,
        &xdg_toplevel_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->events.new_popup,
        &xdg_toplevel_ptr->new_popup_listener,
        handle_new_popup);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.map,
        &xdg_toplevel_ptr->surface_map_listener,
        handle_map);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.unmap,
        &xdg_toplevel_ptr->surface_unmap_listener,
        handle_unmap);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.commit,
        &xdg_toplevel_ptr->surface_commit_listener,
        handle_surface_commit);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_maximize,
        &xdg_toplevel_ptr->toplevel_request_maximize_listener,
        handle_toplevel_maximize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_fullscreen,
        &xdg_toplevel_ptr->toplevel_request_fullscreen_listener,
        handle_toplevel_fullscreen);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_minimize,
        &xdg_toplevel_ptr->toplevel_request_minimize_listener,
        handle_toplevel_minimize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_move,
        &xdg_toplevel_ptr->toplevel_request_move_listener,
        handle_toplevel_move);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_resize,
        &xdg_toplevel_ptr->toplevel_request_resize_listener,
        handle_toplevel_resize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_show_window_menu,
        &xdg_toplevel_ptr->toplevel_request_show_window_menu_listener,
        handle_toplevel_show_window_menu);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_parent,
        &xdg_toplevel_ptr->toplevel_set_parent_listener,
        handle_toplevel_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_title,
        &xdg_toplevel_ptr->toplevel_set_title_listener,
        handle_toplevel_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_app_id,
        &xdg_toplevel_ptr->toplevel_set_app_id_listener,
        handle_toplevel_set_app_id);

    BS_ASSERT(wlr_xdg_surface_ptr->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

    xdg_toplevel_ptr->wlr_scene_tree_ptr = wlr_scene_xdg_surface_create(
        &xdg_shell_ptr->server_ptr->void_wlr_scene_ptr->tree,
        wlr_xdg_surface_ptr);
    if (NULL == xdg_toplevel_ptr->wlr_scene_tree_ptr) {
        wlmaker_xdg_toplevel_destroy(xdg_toplevel_ptr);
        return NULL;
    }

#if defined(ENABLE_TOOLKIT_PROTOTYPE)
    // Transitional -- enable for prototyping: Toolkit-based workspace.
    xdg_toplevel_ptr->xdg_tl_content_ptr =
        wlmtk_xdg_toplevel_content_create(
            wlr_xdg_surface_ptr, xdg_shell_ptr->server_ptr);
#endif  // defined(ENABLE_TOOLKIT_PROTOTYPE)

    wlmaker_view_init(
        &xdg_toplevel_ptr->view,
        &xdg_toplevel_view_impl,
        xdg_shell_ptr->server_ptr,
        wlr_xdg_surface_ptr->surface,
        xdg_toplevel_ptr->wlr_scene_tree_ptr,
        wlmaker_xdg_toplevel_send_close_callback);
    xdg_toplevel_ptr->wlr_xdg_surface_ptr->data =
        xdg_toplevel_ptr->wlr_scene_tree_ptr;

    wlmaker_view_set_position(&xdg_toplevel_ptr->view, 32, 40);



    return xdg_toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_toplevel_destroy(wlmaker_xdg_toplevel_t *xdg_toplevel_ptr)
{
    wlmaker_view_fini(&xdg_toplevel_ptr->view);

    if (NULL != xdg_toplevel_ptr->xdg_tl_content_ptr) {
        wlmtk_xdg_toplevel_content_destroy(xdg_toplevel_ptr->xdg_tl_content_ptr);
        xdg_toplevel_ptr->xdg_tl_content_ptr = NULL;
    }

    wlmaker_xdg_toplevel_t *tl_ptr = xdg_toplevel_ptr;  // For shorter lines.
    wl_list_remove(&tl_ptr->destroy_listener.link);
    wl_list_remove(&tl_ptr->surface_map_listener.link);
    wl_list_remove(&tl_ptr->surface_unmap_listener.link);

    wl_list_remove(&tl_ptr->surface_commit_listener.link);

    wl_list_remove(&tl_ptr->toplevel_request_maximize_listener.link);
    wl_list_remove(&tl_ptr->toplevel_request_fullscreen_listener.link);
    wl_list_remove(&tl_ptr->toplevel_request_minimize_listener.link);
    wl_list_remove(&tl_ptr->toplevel_request_move_listener.link);
    wl_list_remove(&tl_ptr->toplevel_request_resize_listener.link);
    wl_list_remove(&tl_ptr->toplevel_request_show_window_menu_listener.link);
    wl_list_remove(&tl_ptr->toplevel_set_parent_listener.link);
    wl_list_remove(&tl_ptr->toplevel_set_title_listener.link);
    wl_list_remove(&tl_ptr->toplevel_set_app_id_listener.link);

    free(xdg_toplevel_ptr);
}

/* ######################################################################### */

/* ------------------------------------------------------------------------- */
wlmtk_xdg_toplevel_content_t *wlmtk_xdg_toplevel_content_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = logged_calloc(
        1, sizeof(wlmtk_xdg_toplevel_content_t));
    if (NULL == xdg_tl_content_ptr) return NULL;

    if (!wlmtk_content_init(&xdg_tl_content_ptr->super_content,
                            &content_impl)) {
        wlmtk_xdg_toplevel_content_destroy(xdg_tl_content_ptr);
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
void wlmtk_xdg_toplevel_content_destroy(
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr)
{
    wl_list_remove(&xdg_tl_content_ptr->surface_map_listener.link);
    wl_list_remove(&xdg_tl_content_ptr->surface_unmap_listener.link);

    wlmtk_content_fini(&xdg_tl_content_ptr->super_content);
    free(xdg_tl_content_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Type conversion: Gets the wlmaker_xdg_toplevel_t from a wlmaker_view_t.
 *
 * @param view_ptr
 *
 * @return The wlmaker_xdg_toplevel_t pointer.
 */
wlmaker_xdg_toplevel_t *wlmaker_xdg_toplevel_from_view(wlmaker_view_t *view_ptr)
{
    BS_ASSERT(view_ptr->impl_ptr == &xdg_toplevel_view_impl);
    return (wlmaker_xdg_toplevel_t*)view_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the activation status of the XDG shell associated with the |view_ptr|.
 *
 * @param view_ptr
 * @param activated
 *
 * @return The associated configure serial.
 */
uint32_t wlmaker_xdg_toplevel_set_activated(
    wlmaker_view_t *view_ptr,
    bool activated)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    return wlr_xdg_toplevel_set_activated(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel, activated);
}

/* ------------------------------------------------------------------------- */
/**
 * Gets the size of the XDG toplevel.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmaker_xdg_toplevel_get_size(wlmaker_view_t *view_ptr,
                           uint32_t *width_ptr,
                           uint32_t *height_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->base, &geo_box);
    if (NULL != width_ptr) *width_ptr = geo_box.width;
    if (NULL != height_ptr) *height_ptr = geo_box.height;
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the |width| and |height| of the XDG toplevel.
 *
 * @param view_ptr
 * @param width
 * @param height
 */
void wlmaker_xdg_toplevel_set_size(wlmaker_view_t *view_ptr,
                           int width, int height)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    uint32_t serial = wlr_xdg_toplevel_set_size(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel, width, height);
    if (0 < serial) {
        xdg_toplevel_ptr->pending_resize_serial = serial;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the 'maximized' state of the XDG toplevel.
 *
 * @param view_ptr
 * @param maximize
 */
void wlmaker_xdg_toplevel_set_maximized(
    wlmaker_view_t *view_ptr,
    bool maximize)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    // This returns a serial. Should we also tackle this via commit...?
    wlr_xdg_toplevel_set_maximized(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel, maximize);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the 'fullscreen' state of the XDG toplevel.
 *
 * @param view_ptr
 * @param fullscreen
 */
void wlmaker_xdg_toplevel_set_fullscreen(
    wlmaker_view_t *view_ptr,
    bool fullscreen)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    // This returns a serial. Should we also tackle this via commit...?
    wlr_xdg_toplevel_set_fullscreen(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel, fullscreen);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback method to send a close event.
 *
 * @param view_ptr
 */
void wlmaker_xdg_toplevel_send_close_callback(wlmaker_view_t *view_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr =
        wlmaker_xdg_toplevel_from_view(view_ptr);
    wlr_xdg_toplevel_send_close(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the `wlr_xdg_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(struct wl_listener *listener_ptr,
                    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, destroy_listener);
    wlmaker_xdg_toplevel_destroy(xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_popup` signal, creates a new XDG popup for this
 * toplevel.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr, xdg_toplevel_ptr->wlr_scene_tree_ptr);
    xdg_popup_ptr = xdg_popup_ptr;  // not used further.
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
void handle_map(struct wl_listener *listener_ptr,
                __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, surface_map_listener);
    wlmaker_view_map(
        &xdg_toplevel_ptr->view,
        wlmaker_server_get_current_workspace(xdg_toplevel_ptr->view.server_ptr),
        WLMAKER_WORKSPACE_LAYER_SHELL);

    // New XDG toplevel shells will get raised & activated.
    wlmaker_workspace_raise_view(
        xdg_toplevel_ptr->view.workspace_ptr,
        &xdg_toplevel_ptr->view);
    wlmaker_workspace_activate_view(
        xdg_toplevel_ptr->view.workspace_ptr,
        &xdg_toplevel_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal. Removes it from the workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_unmap(struct wl_listener *listener_ptr,
                  __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, surface_unmap_listener);

    wlmaker_view_unmap(&xdg_toplevel_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal of the `struct wlr_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, surface_commit_listener);
    uint32_t serial = xdg_toplevel_ptr->pending_resize_serial;

    // TODO(kaeser@gubbe.ch): Should adjust the size by the geometry.
    if (serial ==
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->current.configure_serial) {
        xdg_toplevel_ptr->pending_resize_serial = 0;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `maximize` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_maximize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, toplevel_request_maximize_listener);
    wlmaker_view_set_maximized(
        &xdg_toplevel_ptr->view,
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->requested.maximized);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `fullscreen` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_fullscreen(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, toplevel_request_fullscreen_listener);

    wlmaker_view_set_fullscreen(
        &xdg_toplevel_ptr->view,
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->requested.fullscreen);


    // TODO(kaeser@gubbe.ch): Wire this up, once implemented.
    bs_log(BS_WARNING, "Unimplemented: Fullscreen request, toplevel %p.",
           xdg_toplevel_ptr);

    /**
     * From tinywl.c and xdg_shell.h: To conform to xdg-shell protocol, we must
     * send a configure. Even if fullscreen is not supported (yet).
     * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
     */
    wlr_xdg_surface_schedule_configure(
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->base);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `minimize` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_minimize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, toplevel_request_minimize_listener);
    wlmaker_view_set_iconified(&xdg_toplevel_ptr->view, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `move` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_move(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, toplevel_request_move_listener);

    wlmaker_cursor_begin_move(
        xdg_toplevel_ptr->view.server_ptr->cursor_ptr,
        &xdg_toplevel_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `resize` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr, toplevel_request_resize_listener);
    struct wlr_xdg_toplevel_resize_event *wlr_xdg_toplevel_resize_event_ptr =
        data_ptr;

    wlmaker_cursor_begin_resize(
        xdg_toplevel_ptr->view.server_ptr->cursor_ptr,
        &xdg_toplevel_ptr->view,
        wlr_xdg_toplevel_resize_event_ptr->edges);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `show_window_menu` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_show_window_menu(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = wl_container_of(
        listener_ptr, xdg_toplevel_ptr,
        toplevel_request_show_window_menu_listener);

    wlmaker_view_window_menu_show(&xdg_toplevel_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_parent` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_parent(struct wl_listener *listener_ptr,
                                __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_toplevel_t, toplevel_set_parent_listener);

    // TODO(kaeser@gubbe.ch): Wire this up, once implemented.
    bs_log(BS_WARNING, "Unimplemented: Set parent request, toplevel %p.",
           xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_title` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_title(struct wl_listener *listener_ptr,
                                   __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_toplevel_t, toplevel_set_title_listener);
    wlmaker_view_set_title(
        &xdg_toplevel_ptr->view,
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->title);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_app_id` signal of the `struct wlr_xdg_toplevel`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_app_id(struct wl_listener *listener_ptr,
                                    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_toplevel_t *xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_toplevel_t, toplevel_set_app_id_listener);
    wlmaker_view_set_app_id(
        &xdg_toplevel_ptr->view,
        xdg_toplevel_ptr->wlr_xdg_surface_ptr->toplevel->app_id);
}

/* ######################################################################### */

/* ------------------------------------------------------------------------- */
/**
 * Destructor. Wraps to @ref wlmtk_xdg_toplevel_content_destroy.
 *
 * @param content_ptr
 */
void xdg_tl_content_destroy(wlmtk_content_t *content_ptr)
{
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_xdg_toplevel_content_t, super_content);
    wlmtk_xdg_toplevel_content_destroy(xdg_tl_content_ptr);
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
struct wlr_scene_node *xdg_tl_content_create_scene_node(
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

    wlmtk_window_t *wlmtk_window_ptr = wlmtk_window_create(
        &xdg_tl_content_ptr->super_content);

    wlmtk_workspace_map_window(wlmtk_workspace_ptr, wlmtk_window_ptr);
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

    wlmtk_window_destroy(window_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
