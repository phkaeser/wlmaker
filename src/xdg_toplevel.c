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

#include "xdg_shell.h"

#include "xdg_popup.h"

/* == Declarations ========================================================= */

/** State of the content for an XDG toplevel surface. */
typedef struct {
    /** Super class. */
    wlmtk_content_t           super_content;

    /** The toplevel's surface. */
    wlmtk_surface_t           *surface_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

    /** The corresponding wlroots XDG surface. */
    struct wlr_xdg_surface    *wlr_xdg_surface_ptr;

    /** Listener for the `destroy` signal of the `wlr_xdg_surface::events`. */
    struct wl_listener        destroy_listener;
    /** Listener for the `new_popup` signal of the `wlr_xdg_surface`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `map` signal of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;
    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Listener for `request_maximize` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_maximize_listener;
    /** Listener for `request_fullscreen` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_fullscreen_listener;
    /** Listener for `request_minimize` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_minimize_listener;
    /** Listener for `request_move` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_move_listener;
    /** Listener for `request_resize` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_resize_listener;
    /** Listener for `show_window_menu` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_request_show_window_menu_listener;
    /** Listener for `set_parent` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_set_parent_listener;
    /** Listener for `set_title` of the `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_set_title_listener;
    /** Listener for `set_app_id` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        toplevel_set_app_id_listener;
} xdg_toplevel_surface_t;

static xdg_toplevel_surface_t *xdg_toplevel_surface_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr);
static void xdg_toplevel_surface_destroy(
    xdg_toplevel_surface_t *xdg_tl_surface_ptr);

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_maximize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_minimize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_move(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_toplevel_request_show_window_menu(
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

static uint32_t content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized);
static uint32_t content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen);
static uint32_t content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void content_request_close(
    wlmtk_content_t *content_ptr);
static void content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/* == Data ================================================================= */

/** Virtual methods for XDG toplevel surface, for the Content superclass. */
const wlmtk_content_vmt_t     _xdg_toplevel_content_vmt = {
    .request_maximized = content_request_maximized,
    .request_fullscreen = content_request_fullscreen,
    .request_size = content_request_size,
    .request_close = content_request_close,
    .set_activated = content_set_activated,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create_from_xdg_toplevel(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    xdg_toplevel_surface_t *surface_ptr = xdg_toplevel_surface_create(
        wlr_xdg_surface_ptr, server_ptr);
    if (NULL == surface_ptr) return NULL;

    wlmtk_window_t *wlmtk_window_ptr = wlmtk_window_create(
        &surface_ptr->super_content, server_ptr->env_ptr);
    if (NULL == wlmtk_window_ptr) {
        xdg_toplevel_surface_destroy(surface_ptr);
        return NULL;
    }
    wl_signal_emit(&server_ptr->window_created_event, wlmtk_window_ptr);

    bs_log(BS_INFO, "Created window %p for wlmtk XDG toplevel surface %p",
           wlmtk_window_ptr, surface_ptr);

    return wlmtk_window_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Creates a @ref xdg_toplevel_surface_t. */
xdg_toplevel_surface_t *xdg_toplevel_surface_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = logged_calloc(
        1, sizeof(xdg_toplevel_surface_t));
    if (NULL == xdg_tl_surface_ptr) return NULL;

    // Note: Content needs the committed size before the surface triggers a
    // layout update. This is... hacky.
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->surface->events.commit,
        &xdg_tl_surface_ptr->surface_commit_listener,
        handle_surface_commit);

    xdg_tl_surface_ptr->surface_ptr = wlmtk_surface_create(
        wlr_xdg_surface_ptr->surface,
        server_ptr->env_ptr);
    if (NULL == xdg_tl_surface_ptr->surface_ptr) {
        xdg_toplevel_surface_destroy(xdg_tl_surface_ptr);
        return NULL;
    }
    xdg_tl_surface_ptr->wlr_xdg_surface_ptr = wlr_xdg_surface_ptr;
    xdg_tl_surface_ptr->server_ptr = server_ptr;

    if (!wlmtk_content_init(
            &xdg_tl_surface_ptr->super_content,
            xdg_tl_surface_ptr->surface_ptr,
            server_ptr->env_ptr)) {
        xdg_toplevel_surface_destroy(xdg_tl_surface_ptr);
        return NULL;
    }
    wlmtk_content_extend(
            &xdg_tl_surface_ptr->super_content,
            &_xdg_toplevel_content_vmt);

    memset(&xdg_tl_surface_ptr->super_content.client, 0,
           sizeof(wlmtk_util_client_t));
    wl_client_get_credentials(
        xdg_tl_surface_ptr->surface_ptr->wlr_surface_ptr->resource->client,
        &xdg_tl_surface_ptr->super_content.client.pid,
        &xdg_tl_surface_ptr->super_content.client.uid,
        &xdg_tl_surface_ptr->super_content.client.gid);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->events.destroy,
        &xdg_tl_surface_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->events.new_popup,
        &xdg_tl_surface_ptr->new_popup_listener,
        handle_new_popup);

    wlmtk_surface_connect_map_listener_signal(
        xdg_tl_surface_ptr->surface_ptr,
        &xdg_tl_surface_ptr->surface_map_listener,
        handle_surface_map);
    wlmtk_surface_connect_unmap_listener_signal(
        xdg_tl_surface_ptr->surface_ptr,
        &xdg_tl_surface_ptr->surface_unmap_listener,
        handle_surface_unmap);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_maximize,
        &xdg_tl_surface_ptr->toplevel_request_maximize_listener,
        handle_toplevel_request_maximize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_fullscreen,
        &xdg_tl_surface_ptr->toplevel_request_fullscreen_listener,
        handle_toplevel_request_fullscreen);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_minimize,
        &xdg_tl_surface_ptr->toplevel_request_minimize_listener,
        handle_toplevel_request_minimize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_move,
        &xdg_tl_surface_ptr->toplevel_request_move_listener,
        handle_toplevel_request_move);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_resize,
        &xdg_tl_surface_ptr->toplevel_request_resize_listener,
        handle_toplevel_request_resize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.request_show_window_menu,
        &xdg_tl_surface_ptr->toplevel_request_show_window_menu_listener,
        handle_toplevel_request_show_window_menu);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_parent,
        &xdg_tl_surface_ptr->toplevel_set_parent_listener,
        handle_toplevel_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_title,
        &xdg_tl_surface_ptr->toplevel_set_title_listener,
        handle_toplevel_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_surface_ptr->toplevel->events.set_app_id,
        &xdg_tl_surface_ptr->toplevel_set_app_id_listener,
        handle_toplevel_set_app_id);

    xdg_tl_surface_ptr->wlr_xdg_surface_ptr->data =
        &xdg_tl_surface_ptr->super_content;

    return xdg_tl_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the @ref xdg_toplevel_surface_t. */
void xdg_toplevel_surface_destroy(
    xdg_toplevel_surface_t *xdg_tl_surface_ptr)
{
    xdg_toplevel_surface_t *xts_ptr = xdg_tl_surface_ptr;
    wl_list_remove(&xts_ptr->toplevel_set_app_id_listener.link);
    wl_list_remove(&xts_ptr->toplevel_set_title_listener.link);
    wl_list_remove(&xts_ptr->toplevel_set_parent_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_show_window_menu_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_resize_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_move_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_fullscreen_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_maximize_listener.link);
    wl_list_remove(&xts_ptr->toplevel_request_minimize_listener.link);

    wl_list_remove(&xts_ptr->surface_map_listener.link);
    wl_list_remove(&xts_ptr->surface_unmap_listener.link);
    wl_list_remove(&xts_ptr->new_popup_listener.link);
    wl_list_remove(&xts_ptr->destroy_listener.link);

    wlmtk_content_fini(&xts_ptr->super_content);

    if (NULL != xdg_tl_surface_ptr->surface_ptr) {
        wlmtk_surface_destroy(xdg_tl_surface_ptr->surface_ptr);
        xdg_tl_surface_ptr->surface_ptr = NULL;
    }
    wl_list_remove(&xts_ptr->surface_commit_listener.link);
    free(xts_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_maximized for XDG toplevel. */
uint32_t content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        content_ptr, xdg_toplevel_surface_t, super_content);

    return wlr_xdg_toplevel_set_maximized(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel, maximized);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_fullscreen for XDG. */
uint32_t content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        content_ptr, xdg_toplevel_surface_t, super_content);

    return wlr_xdg_toplevel_set_fullscreen(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel, fullscreen);
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
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        content_ptr, xdg_toplevel_surface_t, super_content);

    return wlr_xdg_toplevel_set_size(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel, width, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Requests the content to close: Sends a 'close' message to the toplevel.
 *
 * @param content_ptr
 */
void content_request_close(wlmtk_content_t *content_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        content_ptr, xdg_toplevel_surface_t, super_content);

    wlr_xdg_toplevel_send_close(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets the keyboard activation status for the content.
 *
 * @param content_ptr
 * @param activated
 */
void content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        content_ptr, xdg_toplevel_surface_t, super_content);

    wlr_xdg_toplevel_set_activated(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel, activated);

    wlmtk_surface_set_activated(xdg_tl_surface_ptr->surface_ptr, activated);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the `wlr_xdg_surface::events`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(struct wl_listener *listener_ptr,
                    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, xdg_toplevel_surface_t, destroy_listener);

    bs_log(BS_INFO, "Destroying window %p for wlmtk XDG surface %p",
           xdg_tl_surface_ptr, xdg_tl_surface_ptr->super_content.window_ptr);

    wl_signal_emit(
        &xdg_tl_surface_ptr->server_ptr->window_destroyed_event,
        xdg_tl_surface_ptr->super_content.window_ptr);
    wlmtk_window_destroy(xdg_tl_surface_ptr->super_content.window_ptr);
    xdg_toplevel_surface_destroy(xdg_tl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_popup` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, xdg_toplevel_surface_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr, xdg_tl_surface_ptr->server_ptr->env_ptr);
    if (NULL == xdg_popup_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_xdg_popup_create(%p, %p)",
               wlr_xdg_popup_ptr, xdg_tl_surface_ptr->server_ptr->env_ptr);
        return;
    }

    wlmtk_element_set_visible(
        wlmtk_content_element(&xdg_popup_ptr->super_content), true);
    wlmtk_content_add_popup(
        &xdg_tl_surface_ptr->super_content,
        &xdg_popup_ptr->super_content);

    bs_log(BS_INFO, "XDG toplevel %p: New popup %p",
           xdg_tl_surface_ptr, xdg_popup_ptr);
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
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, xdg_toplevel_surface_t, surface_map_listener);

    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        wlmaker_server_get_current_workspace(xdg_tl_surface_ptr->server_ptr));

    wlmtk_workspace_map_window(
        wlmtk_workspace_ptr,
        xdg_tl_surface_ptr->super_content.window_ptr);
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
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, xdg_toplevel_surface_t, surface_unmap_listener);

    wlmtk_window_t *window_ptr = xdg_tl_surface_ptr->super_content.window_ptr;
    wlmtk_workspace_unmap_window(
        wlmtk_window_get_workspace(window_ptr),
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
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, xdg_toplevel_surface_t, surface_commit_listener);

    if (NULL == xdg_tl_surface_ptr->wlr_xdg_surface_ptr) return;
    BS_ASSERT(xdg_tl_surface_ptr->wlr_xdg_surface_ptr->role ==
              WLR_XDG_SURFACE_ROLE_TOPLEVEL);

    wlmtk_content_commit(
        &xdg_tl_surface_ptr->super_content,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->current.geometry.width,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->current.geometry.height,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->current.configure_serial);

    wlmtk_window_commit_maximized(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel->current.maximized);
    wlmtk_window_commit_fullscreen(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel->current.fullscreen);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_maximize` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_request_maximize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_maximize_listener);
    wlmtk_window_request_maximized(
        xdg_tl_surface_ptr->super_content.window_ptr,
        !wlmtk_window_is_maximized(
            xdg_tl_surface_ptr->super_content.window_ptr));

    // Protocol expects an `ack_configure`. Depending on current state, that
    // may not have been sent throught @ref wlmtk_window_request_maximized,
    // hence adding an explicit `ack_configure` here.
    wlr_xdg_surface_schedule_configure(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel->base);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_fullscreen` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_request_fullscreen(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_maximize_listener);

    wlmtk_window_request_fullscreen(
        xdg_tl_surface_ptr->super_content.window_ptr,
        !wlmtk_window_is_fullscreen(
            xdg_tl_surface_ptr->super_content.window_ptr));

    // Protocol expects an `ack_configure`. Depending on current state, that
    // may not have been sent throught @ref wlmtk_window_request_maximized,
    // hence adding an explicit `ack_configure` here.
    wlr_xdg_surface_schedule_configure(
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel->base);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_minimize` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_request_minimize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_minimize_listener);

    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_WARNING, "Unimplemented: request_minimize for XDG toplevel %p",
           xdg_tl_surface_ptr);
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
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_move_listener);
    wlmtk_window_request_move(xdg_tl_surface_ptr->super_content.window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_resize` signal.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a struct wlr_xdg_toplevel_resize_event.
 */
void handle_toplevel_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_resize_listener);
    struct wlr_xdg_toplevel_resize_event *resize_event_ptr = data_ptr;
    wlmtk_window_request_resize(
        xdg_tl_surface_ptr->super_content.window_ptr,
        resize_event_ptr->edges);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_show_window_menu` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_request_show_window_menu(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_show_window_menu_listener);

    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_WARNING, "Unimplemented: request_show_window_menu for XDG "
           "toplevel %p", xdg_tl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_parent` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_parent(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_set_parent_listener);

    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_WARNING, "Unimplemented: set_parent_menu for XDG toplevel %p",
           xdg_tl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_title` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_title(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_set_title_listener);

    wlmtk_window_set_title(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_surface_ptr->toplevel->title);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_app_id` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_toplevel_set_app_id(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_set_app_id_listener);

    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_WARNING, "Unimplemented: set_parent_menu for XDG toplevel %p",
           xdg_tl_surface_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
