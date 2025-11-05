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
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/box.h>
#undef WLR_USE_UNSTABLE

#include "server.h"
#include "tl_menu.h"
#include "toolkit/toolkit.h"
#include "xdg_popup.h"

/* == Declarations ========================================================= */

/** State of an XDG toplevel in wlmaker. */
struct wlmaker_xdg_toplevel {
    /** Holds surface as content, will be the window's content. */
    wlmtk_base_t              base;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
    /** The corresponding wlroots XDG toplevel. */
    struct wlr_xdg_toplevel   *wlr_xdg_toplevel_ptr;

    /** The toplevel's toolkit surface. */
    wlmtk_surface_t           *surface_ptr;
    /** The toplevel's window, when mapped. */
    wlmtk_window_t           *window_ptr;
    /** The toplevel's window menu. */
    wlmaker_tl_menu_t         *tl_menu_ptr;

    /** Listener for the `destroy` signal of the `wlr_xdg_toplevel::events`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_maximize` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_maximize_listener;
    /** Listener for `request_fullscreen` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_fullscreen_listener;
    /** Listener for `request_minimize` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_minimize_listener;
    /** Listener for `request_move` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_move_listener;
    /** Listener for `request_resize` signal of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_resize_listener;
    /** Listener for `show_window_menu` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        request_show_window_menu_listener;
    /** Listener for `set_parent` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        set_parent_listener;
    /** Listener for `set_title` of the `wlr_xdg_toplevel::events`. */
    struct wl_listener        set_title_listener;
    /** Listener for `set_app_id` of `wlr_xdg_toplevel::events`. */
    struct wl_listener        set_app_id_listener;

    /** Listener for the `new_popup` signal of the `wlr_xdg_surface`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `map` signal of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;
    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Listener for @ref wlmtk_window_events_t::request_close. */
    struct wl_listener        window_request_close_listener;
    /** Listener for @ref wlmtk_window_events_t::set_activated. */
    struct wl_listener        window_set_activated_listener;
    /** Listener for @ref wlmtk_window_events_t::request_size. */
    struct wl_listener        window_request_size_listener;
    /** Listener for @ref wlmtk_window_events_t::request_fullscreen. */
    struct wl_listener        window_request_fullscreen_listener;
    /** Listener for @ref wlmtk_window_events_t::request_maximized. */
    struct wl_listener        window_request_maximized_listener;

    /** Whether this toplevel is configured to be server-side decorated. */
    bool                      server_side_decorated;
};

static void _wlmaker_xdg_toplevel_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_maximize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_minimize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_move(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_request_show_window_menu(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_set_parent(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_set_title(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_set_app_id(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_xdg_toplevel_handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_xdg_toplevel_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_xdg_toplevel_handle_window_request_close(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_window_set_activated(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_window_request_size(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_window_request_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_xdg_toplevel_handle_window_request_maximized(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_create(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr)
{
    // Guard clause: Must have a base. */
    if (NULL == wlr_xdg_toplevel_ptr->base) {
        bs_log(BS_ERROR, "Missing base for wlr_xdg_toplevel at %p",
               wlr_xdg_toplevel_ptr);
        return NULL;
    }

    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = logged_calloc(
        1, sizeof(struct wlmaker_xdg_toplevel));
    if (NULL == wlmaker_xdg_toplevel_ptr) return NULL;
    wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr = wlr_xdg_toplevel_ptr;
    wlmaker_xdg_toplevel_ptr->server_ptr = server_ptr;

    if (!wlmtk_base_init(&wlmaker_xdg_toplevel_ptr->base, NULL)) goto error;

    wlmaker_xdg_toplevel_ptr->surface_ptr = wlmtk_xdg_surface_create(
        wlr_xdg_toplevel_ptr->base,
        server_ptr->wlr_seat_ptr);
    if (NULL == wlmaker_xdg_toplevel_ptr->surface_ptr) goto error;
    wlmtk_base_set_content_element(
        &wlmaker_xdg_toplevel_ptr->base,
        wlmtk_surface_element(wlmaker_xdg_toplevel_ptr->surface_ptr));

    wlmaker_xdg_toplevel_ptr->window_ptr = wlmtk_window_create(
        wlmtk_base_element(&wlmaker_xdg_toplevel_ptr->base),
        &wlmaker_xdg_toplevel_ptr->server_ptr->style.window,
        &wlmaker_xdg_toplevel_ptr->server_ptr->style.menu);
    if (NULL == wlmaker_xdg_toplevel_ptr->window_ptr) goto error;
    wlmtk_window_set_properties(
        wlmaker_xdg_toplevel_ptr->window_ptr,
        WLMTK_WINDOW_PROPERTY_RESIZABLE |
        WLMTK_WINDOW_PROPERTY_ICONIFIABLE |
        WLMTK_WINDOW_PROPERTY_CLOSABLE);
    wlmtk_util_client_t client;
    wl_client_get_credentials(
        wlr_xdg_toplevel_ptr->resource->client,
        &client.pid, &client.uid, &client.gid);
    wlmtk_window_set_client(wlmaker_xdg_toplevel_ptr->window_ptr, &client);

    wlmaker_xdg_toplevel_ptr->tl_menu_ptr = wlmaker_tl_menu_create(
        wlmaker_xdg_toplevel_ptr->window_ptr, server_ptr);
    if (NULL == wlmaker_xdg_toplevel_ptr->tl_menu_ptr) goto error;

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.destroy,
        &wlmaker_xdg_toplevel_ptr->destroy_listener,
        _wlmaker_xdg_toplevel_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_maximize,
        &wlmaker_xdg_toplevel_ptr->request_maximize_listener,
        _wlmaker_xdg_toplevel_handle_request_maximize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_fullscreen,
        &wlmaker_xdg_toplevel_ptr->request_fullscreen_listener,
        _wlmaker_xdg_toplevel_handle_request_fullscreen);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_minimize,
        &wlmaker_xdg_toplevel_ptr->request_minimize_listener,
        _wlmaker_xdg_toplevel_handle_request_minimize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_move,
        &wlmaker_xdg_toplevel_ptr->request_move_listener,
        _wlmaker_xdg_toplevel_handle_request_move);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_resize,
        &wlmaker_xdg_toplevel_ptr->request_resize_listener,
        _wlmaker_xdg_toplevel_handle_request_resize);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.request_show_window_menu,
        &wlmaker_xdg_toplevel_ptr->request_show_window_menu_listener,
        _wlmaker_xdg_toplevel_handle_request_show_window_menu);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_parent,
        &wlmaker_xdg_toplevel_ptr->set_parent_listener,
        _wlmaker_xdg_toplevel_handle_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_title,
        &wlmaker_xdg_toplevel_ptr->set_title_listener,
        _wlmaker_xdg_toplevel_handle_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->events.set_app_id,
        &wlmaker_xdg_toplevel_ptr->set_app_id_listener,
        _wlmaker_xdg_toplevel_handle_set_app_id);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_toplevel_ptr->base->events.new_popup,
        &wlmaker_xdg_toplevel_ptr->new_popup_listener,
        _wlmaker_xdg_toplevel_handle_new_popup);

    wlmtk_surface_connect_map_listener_signal(
        wlmaker_xdg_toplevel_ptr->surface_ptr,
        &wlmaker_xdg_toplevel_ptr->surface_map_listener,
        _wlmaker_xdg_toplevel_handle_surface_map);
    wlmtk_surface_connect_unmap_listener_signal(
        wlmaker_xdg_toplevel_ptr->surface_ptr,
        &wlmaker_xdg_toplevel_ptr->surface_unmap_listener,
        _wlmaker_xdg_toplevel_handle_surface_unmap);

    wlmtk_util_connect_listener_signal(
        &wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr->base->surface->events.commit,
        &wlmaker_xdg_toplevel_ptr->surface_commit_listener,
        _wlmaker_xdg_toplevel_handle_surface_commit);

    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(wlmaker_xdg_toplevel_ptr->window_ptr)->request_close,
        &wlmaker_xdg_toplevel_ptr->window_request_close_listener,
        _wlmaker_xdg_toplevel_handle_window_request_close);
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(wlmaker_xdg_toplevel_ptr->window_ptr)->set_activated,
        &wlmaker_xdg_toplevel_ptr->window_set_activated_listener,
        _wlmaker_xdg_toplevel_handle_window_set_activated);
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(wlmaker_xdg_toplevel_ptr->window_ptr)->request_size,
        &wlmaker_xdg_toplevel_ptr->window_request_size_listener,
        _wlmaker_xdg_toplevel_handle_window_request_size);
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(wlmaker_xdg_toplevel_ptr->window_ptr)->request_fullscreen,
        &wlmaker_xdg_toplevel_ptr->window_request_fullscreen_listener,
        _wlmaker_xdg_toplevel_handle_window_request_fullscreen);
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(wlmaker_xdg_toplevel_ptr->window_ptr)->request_maximized,
        &wlmaker_xdg_toplevel_ptr->window_request_maximized_listener,
        _wlmaker_xdg_toplevel_handle_window_request_maximized);

    wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr->base->data =
        wlmaker_xdg_toplevel_ptr;

    wl_signal_emit(
        &server_ptr->window_created_event,
        wlmaker_xdg_toplevel_ptr->window_ptr);

    bs_log(BS_INFO, "Created XDG toplevel %p", wlmaker_xdg_toplevel_ptr);
    return wlmaker_xdg_toplevel_ptr;

error:
    wlmaker_xdg_toplevel_destroy(wlmaker_xdg_toplevel_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_toplevel_destroy(struct wlmaker_xdg_toplevel *wxt_ptr)
{
    bs_log(BS_INFO, "Destroying XDG toplevel %p", wxt_ptr);
    wxt_ptr->wlr_xdg_toplevel_ptr->base->data = NULL;

    wlmtk_util_disconnect_listener(&wxt_ptr->window_request_fullscreen_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->window_request_size_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->window_set_activated_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->window_request_close_listener);

    wlmtk_util_disconnect_listener(&wxt_ptr->surface_commit_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->surface_unmap_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->surface_map_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->new_popup_listener);

    wlmtk_util_disconnect_listener(&wxt_ptr->set_app_id_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->set_title_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->set_parent_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_show_window_menu_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_resize_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_move_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_fullscreen_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_maximize_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->request_minimize_listener);
    wlmtk_util_disconnect_listener(&wxt_ptr->destroy_listener);

    if (NULL != wxt_ptr->tl_menu_ptr) {
        wlmaker_tl_menu_destroy(wxt_ptr->tl_menu_ptr);
        wxt_ptr->tl_menu_ptr = NULL;
    }

    if (NULL != wxt_ptr->window_ptr) {
        wl_signal_emit(
            &wxt_ptr->server_ptr->window_destroyed_event,
            wxt_ptr->window_ptr);

        wlmtk_window_destroy(wxt_ptr->window_ptr);
        wxt_ptr->window_ptr = NULL;
    }

    wlmtk_base_fini(&wxt_ptr->base);

    free(wxt_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_toplevel_set_server_side_decorated(
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr,
    bool server_side_decorated)
{
    wlmaker_xdg_toplevel_ptr->server_side_decorated = server_side_decorated;
    wlmtk_window_set_server_side_decorated(
        wlmaker_xdg_toplevel_ptr->window_ptr,
        wlmaker_xdg_toplevel_ptr->server_side_decorated);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** The XDG toplevel is destroyed: Destry the wlmaker toplevel, too. */
void _wlmaker_xdg_toplevel_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, destroy_listener);
    wlmaker_xdg_toplevel_destroy(wlmaker_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel requests to be maximized. */
void _wlmaker_xdg_toplevel_handle_request_maximize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, request_maximize_listener);

    if (wxt_ptr->wlr_xdg_toplevel_ptr->requested.maximized !=
        wlmtk_window_is_maximized(wxt_ptr->window_ptr)) {
        wlmtk_window_request_maximized(
            wxt_ptr->window_ptr,
            wxt_ptr->wlr_xdg_toplevel_ptr->requested.maximized);
    }

    // Protocol expects an `configure`. Depending on current state, that may
    // not have been sent yet, hence adding an explicit `configure` here.
    if (wxt_ptr->wlr_xdg_toplevel_ptr->base->initialized) {
        // TODO(kaeser@gubbe.ch): Store state and then issue these pending
        // configures once initialized.
        wlr_xdg_surface_schedule_configure(wxt_ptr->wlr_xdg_toplevel_ptr->base);
    }
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel (client) requests to be put in fullscreen. */
void _wlmaker_xdg_toplevel_handle_request_fullscreen(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        request_fullscreen_listener);

    if (wxt_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen !=
        wlmtk_window_is_fullscreen(wxt_ptr->window_ptr)) {

        // Sets the requested output. Or NULL, if no preference indicated.
        wlmtk_window_set_wlr_output(
            wxt_ptr->window_ptr,
            wxt_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen_output);

        wlmtk_window_request_fullscreen(
            wxt_ptr->window_ptr,
            wxt_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen);
    }

    // Protocol expects an `configure`. Depending on current state, that may
    // not have been sent yet, hence adding an explicit `configure` here.
    if (wxt_ptr->wlr_xdg_toplevel_ptr->base->initialized) {
        // TODO(kaeser@gubbe.ch): Store state and then issue these pending
        // configures once initialized.
        wlr_xdg_surface_schedule_configure(wxt_ptr->wlr_xdg_toplevel_ptr->base);
    }
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel requests to be minimized. */
void _wlmaker_xdg_toplevel_handle_request_minimize(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, request_minimize_listener);

    bs_log(BS_ERROR, "TODO: Request minimize %p", wlmaker_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel requests to be moved. */
void _wlmaker_xdg_toplevel_handle_request_move(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, request_move_listener);

    wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(
        wlmaker_xdg_toplevel_ptr->window_ptr);
    if (NULL == workspace_ptr) return;

    wlmtk_workspace_begin_window_move(
        workspace_ptr,
        wlmaker_xdg_toplevel_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel requests to be resized. */
void _wlmaker_xdg_toplevel_handle_request_resize(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, request_resize_listener);
    struct wlr_xdg_toplevel_resize_event *resize_event_ptr = data_ptr;

    wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(
        wlmaker_xdg_toplevel_ptr->window_ptr);
    if (NULL == workspace_ptr) return;

    wlmtk_workspace_begin_window_resize(
        workspace_ptr,
        wlmaker_xdg_toplevel_ptr->window_ptr,
        resize_event_ptr->edges);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel requests to show the window menu. */
void _wlmaker_xdg_toplevel_handle_request_show_window_menu(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        request_show_window_menu_listener);

    wlmtk_window_menu_set_enabled(
        wlmaker_xdg_toplevel_ptr->window_ptr,
        !wlmtk_menu_is_open(wlmtk_window_menu(wlmaker_xdg_toplevel_ptr->window_ptr)));
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel sets the parent. */
void _wlmaker_xdg_toplevel_handle_set_parent(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, set_parent_listener);

    bs_log(BS_ERROR, "TODO: set_parent %p", wlmaker_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel sets the title. */
void _wlmaker_xdg_toplevel_handle_set_title(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, set_title_listener);

    wlmtk_window_set_title(
        wlmaker_xdg_toplevel_ptr->window_ptr,
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr->title);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel sets the app ID. */
void _wlmaker_xdg_toplevel_handle_set_app_id(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, set_app_id_listener);

    bs_log(BS_ERROR, "TODO: set_app_id %p", wlmaker_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** The XDG toplevel adds a new popup. */
void _wlmaker_xdg_toplevel_handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr,
        wlmaker_xdg_toplevel_ptr->server_ptr->wlr_seat_ptr);
    if (NULL == xdg_popup_ptr) {
        wl_resource_post_error(
            wlr_xdg_popup_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmtk_xdg_popup2_create");
        return;
    }

    wlmtk_base_push_element(
        &wlmaker_xdg_toplevel_ptr->base,
        wlmaker_xdg_popup_element(xdg_popup_ptr));
}

/* ------------------------------------------------------------------------- */
/** Handles the XDG's surface map event. */
void _wlmaker_xdg_toplevel_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, surface_map_listener);

    wlmtk_workspace_t *workspace_ptr = wlmtk_root_get_current_workspace(
        wlmaker_xdg_toplevel_ptr->server_ptr->root_ptr);

    wlmtk_workspace_map_window(
        workspace_ptr,
        wlmaker_xdg_toplevel_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles when the XDG surface is to be unmapped. */
void _wlmaker_xdg_toplevel_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, surface_unmap_listener);

    wlmtk_workspace_unmap_window(
        wlmtk_window_get_workspace(wlmaker_xdg_toplevel_ptr->window_ptr),
        wlmaker_xdg_toplevel_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles XDG surface commits. */
void _wlmaker_xdg_toplevel_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmaker_xdg_toplevel, surface_commit_listener);

    struct wlr_xdg_surface *wlr_xdg_surface_ptr =
        wxt_ptr->wlr_xdg_toplevel_ptr->base;
    if (wlr_xdg_surface_ptr->initial_commit) {
        // Initial commit: Ensure a configure is responded with.
        wlr_xdg_surface_schedule_configure(wlr_xdg_surface_ptr);
    }

    if (wxt_ptr->wlr_xdg_toplevel_ptr->current.fullscreen !=
        wlmtk_window_is_fullscreen(wxt_ptr->window_ptr)) {
        wlmtk_window_commit_fullscreen(
            wxt_ptr->window_ptr,
            wxt_ptr->wlr_xdg_toplevel_ptr->current.fullscreen);
    } else if (wxt_ptr->wlr_xdg_toplevel_ptr->current.maximized !=
               wlmtk_window_is_maximized(wxt_ptr->window_ptr)) {
        wlmtk_window_commit_maximized(
            wxt_ptr->window_ptr,
            wxt_ptr->wlr_xdg_toplevel_ptr->current.maximized);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request_close event: Forwards to the toplevel. */
void _wlmaker_xdg_toplevel_handle_window_request_close(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_close_listener);

    wlr_xdg_toplevel_send_close(
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the window's activation event: Forwards to the toplevel. */
void _wlmaker_xdg_toplevel_handle_window_set_activated(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_set_activated_listener);

    wlr_xdg_toplevel_set_activated(
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr,
        wlmtk_window_is_activated(wlmaker_xdg_toplevel_ptr->window_ptr));
    wlmtk_surface_set_activated(
        wlmaker_xdg_toplevel_ptr->surface_ptr,
        wlmtk_window_is_activated(wlmaker_xdg_toplevel_ptr->window_ptr));
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request for size. */
void _wlmaker_xdg_toplevel_handle_window_request_size(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_size_listener);
    const struct wlr_box *box_ptr = data_ptr;

    wlr_xdg_toplevel_set_size(
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr,
        box_ptr->width,
        box_ptr->height);
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request for going to fullscreen. */
void _wlmaker_xdg_toplevel_handle_window_request_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_fullscreen_listener);
    bool *fullscreen_ptr = data_ptr;

    wlr_xdg_toplevel_set_fullscreen(
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr,
        *fullscreen_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request for going to maximized. */
void _wlmaker_xdg_toplevel_handle_window_request_maximized(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_maximized_listener);
    bool *maximized_ptr = data_ptr;

    wlr_xdg_toplevel_set_maximized(
        wlmaker_xdg_toplevel_ptr->wlr_xdg_toplevel_ptr,
        *maximized_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
