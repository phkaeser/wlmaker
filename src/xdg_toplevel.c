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

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/box.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

#include "config.h"
#include "server.h"
#include "tl_menu.h"
#include "toolkit/toolkit.h"
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

    /** The corresponding wlroots XDG toplevel. */
    struct wlr_xdg_toplevel   *wlr_xdg_toplevel_ptr;

    /** The toplevel's window menu. */
    wlmaker_tl_menu_t         *tl_menu_ptr;

    /** Listener for the `destroy` signal of the `wlr_xdg_toplevel::events`. */
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
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
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
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr)
{
    xdg_toplevel_surface_t *surface_ptr = xdg_toplevel_surface_create(
        wlr_xdg_toplevel_ptr, server_ptr);
    if (NULL == surface_ptr) return NULL;

    wlmtk_window_t *wlmtk_window_ptr = wlmtk_window_create(
        &surface_ptr->super_content,
        &server_ptr->style.window,
        &server_ptr->style.menu,
        server_ptr->wlr_seat_ptr,
        server_ptr->env_ptr);
    if (NULL == wlmtk_window_ptr) {
        xdg_toplevel_surface_destroy(surface_ptr);
        return NULL;
    }

    surface_ptr->tl_menu_ptr = wlmaker_tl_menu_create(
        wlmtk_window_ptr, server_ptr);
    if (NULL == surface_ptr->tl_menu_ptr) {
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
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = logged_calloc(
        1, sizeof(xdg_toplevel_surface_t));
    if (NULL == xdg_tl_surface_ptr) return NULL;
    if (NULL == wlr_xdg_toplevel_ptr->base) return NULL;

    // Note: Content needs the committed size before the surface triggers a
    // layout update. This is... hacky.
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->base->surface->events.commit,
        &xdg_tl_surface_ptr->surface_commit_listener,
        handle_surface_commit);

    xdg_tl_surface_ptr->surface_ptr = wlmtk_surface_create(
        wlr_xdg_toplevel_ptr->base->surface,
        server_ptr->env_ptr);
    if (NULL == xdg_tl_surface_ptr->surface_ptr) {
        xdg_toplevel_surface_destroy(xdg_tl_surface_ptr);
        return NULL;
    }
    xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr = wlr_xdg_toplevel_ptr;
    xdg_tl_surface_ptr->server_ptr = server_ptr;

    if (!wlmtk_content_init(
            &xdg_tl_surface_ptr->super_content,
            wlmtk_surface_element(xdg_tl_surface_ptr->surface_ptr),
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
#if WLR_VERSION_NUM >= (18 << 8)
        &wlr_xdg_toplevel_ptr->events.destroy,
#else // WLR_VERSION_NUM >= (18 << 8)
        &wlr_xdg_toplevel_ptr->base->events.destroy,
#endif // WLR_VERSION_NUM >= (18 << 8)
        &xdg_tl_surface_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->base->events.new_popup,
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
        &wlr_xdg_toplevel_ptr->events.request_maximize,
        &xdg_tl_surface_ptr->toplevel_request_maximize_listener,
        handle_toplevel_request_maximize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_fullscreen,
        &xdg_tl_surface_ptr->toplevel_request_fullscreen_listener,
        handle_toplevel_request_fullscreen);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_minimize,
        &xdg_tl_surface_ptr->toplevel_request_minimize_listener,
        handle_toplevel_request_minimize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_move,
        &xdg_tl_surface_ptr->toplevel_request_move_listener,
        handle_toplevel_request_move);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_resize,
        &xdg_tl_surface_ptr->toplevel_request_resize_listener,
        handle_toplevel_request_resize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_show_window_menu,
        &xdg_tl_surface_ptr->toplevel_request_show_window_menu_listener,
        handle_toplevel_request_show_window_menu);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_parent,
        &xdg_tl_surface_ptr->toplevel_set_parent_listener,
        handle_toplevel_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_title,
        &xdg_tl_surface_ptr->toplevel_set_title_listener,
        handle_toplevel_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_app_id,
        &xdg_tl_surface_ptr->toplevel_set_app_id_listener,
        handle_toplevel_set_app_id);

    xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->data =
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr, maximized);
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr, fullscreen);
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr, width, height);
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr);
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr, activated);

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

    if (NULL != xdg_tl_surface_ptr->tl_menu_ptr) {
        wlmaker_tl_menu_destroy(xdg_tl_surface_ptr->tl_menu_ptr);
        xdg_tl_surface_ptr->tl_menu_ptr = NULL;
    }

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
        wlr_xdg_popup_ptr,
        xdg_tl_surface_ptr->server_ptr->env_ptr);
    if (NULL == xdg_popup_ptr) {
        wl_resource_post_error(
            wlr_xdg_popup_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmtk_xdg_popup2_create.");
        return;
    }

    wlmtk_element_set_visible(
        wlmtk_popup_element(&xdg_popup_ptr->super_popup), true);
    wlmtk_content_add_wlmtk_popup(
        &xdg_tl_surface_ptr->super_content,
        &xdg_popup_ptr->super_popup);

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

    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(
            xdg_tl_surface_ptr->server_ptr->root_ptr);

    wlmtk_workspace_map_window(
        workspace_ptr,
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

    if (xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->initial_commit) {
        // Initial commit: Ensure a configure is responded with.
        wlr_xdg_surface_schedule_configure(
            xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base);
    }

    if (NULL == xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr) return;
    BS_ASSERT(xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->role ==
              WLR_XDG_SURFACE_ROLE_TOPLEVEL);

    wlmtk_content_commit(
        &xdg_tl_surface_ptr->super_content,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->current.geometry.width,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->current.geometry.height,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base->current.configure_serial);

    wlmtk_window_commit_maximized(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->current.maximized);
    wlmtk_window_commit_fullscreen(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->current.fullscreen);
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->requested.maximized);

    // Protocol expects an `ack_configure`. Depending on current state, that
    // may not have been sent throught @ref wlmtk_window_request_maximized,
    // hence adding an explicit `ack_configure` here.
    wlr_xdg_surface_schedule_configure(
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base);
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
        toplevel_request_fullscreen_listener);

    // Sets the requested output. Or NULL, if no preference indicated.
    wlmtk_window_set_output(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen_output);

    wlmtk_window_request_fullscreen(
        xdg_tl_surface_ptr->super_content.window_ptr,
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen);

    // Protocol expects an `ack_configure`. Depending on current state, that
    // may not have been sent throught @ref wlmtk_window_request_maximized,
    // hence adding an explicit `ack_configure` here.
    wlr_xdg_surface_schedule_configure(
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->base);
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
 * @param data_ptr            struct wlr_xdg_toplevel_show_window_menu_event.
 */
void handle_toplevel_request_show_window_menu(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    xdg_toplevel_surface_t *xdg_tl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr,
        xdg_toplevel_surface_t,
        toplevel_request_show_window_menu_listener);

    wlmtk_window_menu_set_enabled(
        xdg_tl_surface_ptr->super_content.window_ptr,
        true);

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
    bs_log(BS_WARNING, "Unimplemented: set_parent for XDG toplevel %p",
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
        xdg_tl_surface_ptr->wlr_xdg_toplevel_ptr->title);
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
    bs_log(BS_WARNING, "Unimplemented: set_app_id for XDG toplevel %p",
           xdg_tl_surface_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
