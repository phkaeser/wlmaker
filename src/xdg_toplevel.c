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
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
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

    /** Injected method for wlr_xdg_toplevel_set_maximized(). */
    uint32_t (*_set_maximized)(struct wlr_xdg_toplevel *, bool);
    /** Injected method for wlr_xdg_toplevel_set_fullscreen(). */
    uint32_t (*_set_fullscreen)(struct wlr_xdg_toplevel *, bool);
    /** Injected method for wlr_xdg_toplevel_set_size(). */
    uint32_t (*_set_size)(struct wlr_xdg_toplevel *, int32_t, int32_t);
    /** Injected method for wlr_xdg_toplevel_set_activated(). */
    uint32_t (*_set_activated)(struct wlr_xdg_toplevel *, bool);

    /** Serial of the most recent commit() call. */
    uint32_t                  committed_serial;
    /** Serial of the most recent set_size() call. */
    uint32_t                  set_size_serial;

    /** Whether this toplevel is configured to be server-side decorated. */
    bool                      server_side_decorated;

    /** Properties that are pending to be configured for the toplevel. */
    struct {
        enum {
            /** Maximization. */
            WXT_PROP_MAXIMIZED = 1 << 0,
            /** Fullscreen. */
            WXT_PROP_FULLSCREEN = 1 << 1,
            /** Size. */
            WXT_PROP_SIZE = 1 << 2,
            /** Activation. */
            WXT_PROP_ACTIVATED =  1 << 3
        }  properties;
        /** Maximization status. */
        bool                  maximized;
        /** Fullscreen status. */
        bool                  fullscreen;
        /** Width and height. */
        int32_t               width, height;
        /** Activated. */
        bool                  activated;
    } pending;
};

struct wlmaker_xdg_toplevel *_wlmaker_xdg_toplevel_create_injected(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr,
    uint32_t (*_set_maximized)(struct wlr_xdg_toplevel *, bool),
    uint32_t (*_set_fullscreen)(struct wlr_xdg_toplevel *, bool),
    uint32_t (*_set_size)(struct wlr_xdg_toplevel *, int32_t, int32_t),
    uint32_t (*_set_activated)(struct wlr_xdg_toplevel *, bool));
static void _wlmaker_xdg_toplevel_flush_properties(
    struct wlmaker_xdg_toplevel *wxt_ptr);

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
    return _wlmaker_xdg_toplevel_create_injected(
        wlr_xdg_toplevel_ptr,
        server_ptr,
        wlr_xdg_toplevel_set_maximized,
        wlr_xdg_toplevel_set_fullscreen,
        wlr_xdg_toplevel_set_size,
        wlr_xdg_toplevel_set_activated);
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
/** Creates the toplevel, permitting injected calls to set properties. */
struct wlmaker_xdg_toplevel *_wlmaker_xdg_toplevel_create_injected(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr,
    uint32_t (*_set_maximized)(struct wlr_xdg_toplevel *, bool),
    uint32_t (*_set_fullscreen)(struct wlr_xdg_toplevel *, bool),
    uint32_t (*_set_size)(struct wlr_xdg_toplevel *, int32_t, int32_t),
    uint32_t (*_set_activated)(struct wlr_xdg_toplevel *, bool))
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
    wlmaker_xdg_toplevel_ptr->_set_maximized = _set_maximized;
    wlmaker_xdg_toplevel_ptr->_set_fullscreen = _set_fullscreen;
    wlmaker_xdg_toplevel_ptr->_set_size = _set_size;
    wlmaker_xdg_toplevel_ptr->_set_activated = _set_activated;

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
    wlmtk_util_client_t client = {};
    if (NULL != wlr_xdg_toplevel_ptr->resource) {
        wl_client_get_credentials(
            wlr_xdg_toplevel_ptr->resource->client,
            &client.pid, &client.uid, &client.gid);
    }
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
/**
 * Sends pending properties as configure() to the client, after 1st commit.
 *
 * If the first commit hasn't happened yet, the properties will not be sent.
 * A later call to @ref _wlmaker_xdg_toplevel_flush_properties will then flush
 * them. See @ref wlmaker_xdg_toplevel::pending.
 *
 * @param wxt_ptr
 */
void _wlmaker_xdg_toplevel_flush_properties(
    struct wlmaker_xdg_toplevel *wxt_ptr)
{
    if (!wxt_ptr->wlr_xdg_toplevel_ptr->base->initialized) return;

    if (wxt_ptr->pending.properties & WXT_PROP_MAXIMIZED) {
        wxt_ptr->_set_maximized(
            wxt_ptr->wlr_xdg_toplevel_ptr,
            wxt_ptr->pending.maximized);
    }

    if (wxt_ptr->pending.properties & WXT_PROP_FULLSCREEN) {
        wxt_ptr->_set_fullscreen(
            wxt_ptr->wlr_xdg_toplevel_ptr,
            wxt_ptr->pending.fullscreen);
    }

    if (wxt_ptr->pending.properties & WXT_PROP_SIZE) {
        wxt_ptr->set_size_serial = wxt_ptr->_set_size(
            wxt_ptr->wlr_xdg_toplevel_ptr,
            wxt_ptr->pending.width,
            wxt_ptr->pending.height);
    }

    if (wxt_ptr->pending.properties & WXT_PROP_ACTIVATED){
        wxt_ptr->_set_activated(
            wxt_ptr->wlr_xdg_toplevel_ptr,
            wxt_ptr->pending.activated);
    }
    wxt_ptr->pending.properties = 0;
}

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

    wlmtk_window_request_maximized(
        wxt_ptr->window_ptr,
        wxt_ptr->wlr_xdg_toplevel_ptr->requested.maximized);
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

    // Sets the requested output. Or NULL, if no preference indicated.
    wlmtk_window_set_wlr_output(
        wxt_ptr->window_ptr,
        wxt_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen_output);

    wlmtk_window_request_fullscreen(
        wxt_ptr->window_ptr,
        wxt_ptr->wlr_xdg_toplevel_ptr->requested.fullscreen);
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

    wlmtk_window_commit_size(
        wxt_ptr->window_ptr,
        wxt_ptr->wlr_xdg_toplevel_ptr->base->current.geometry.width,
        wxt_ptr->wlr_xdg_toplevel_ptr->base->current.geometry.height);

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

    if (0 != wxt_ptr->pending.properties) {
        _wlmaker_xdg_toplevel_flush_properties(wxt_ptr);
    } else if (wlr_xdg_surface_ptr->initial_commit) {
        wlr_xdg_surface_schedule_configure(wlr_xdg_surface_ptr);
    }

    wxt_ptr->committed_serial =
        wxt_ptr->wlr_xdg_toplevel_ptr->base->current.configure_serial;
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

    wlmaker_xdg_toplevel_ptr->pending.properties |= WXT_PROP_ACTIVATED;
    wlmaker_xdg_toplevel_ptr->pending.activated =
        wlmtk_window_is_activated(wlmaker_xdg_toplevel_ptr->window_ptr);
    _wlmaker_xdg_toplevel_flush_properties(wlmaker_xdg_toplevel_ptr);
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
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_size_listener);
    const struct wlr_box *box_ptr = data_ptr;

    // Ignore the request, if commits have not caught up yet.
    if (wxt_ptr->set_size_serial > wxt_ptr->committed_serial) return;

    wxt_ptr->pending.width = box_ptr->width;
    wxt_ptr->pending.height = box_ptr->height;
    wxt_ptr->pending.properties |= WXT_PROP_SIZE;
    _wlmaker_xdg_toplevel_flush_properties(wxt_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request for going to fullscreen. */
void _wlmaker_xdg_toplevel_handle_window_request_fullscreen(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_fullscreen_listener);
    bool *fullscreen_ptr = data_ptr;

    wxt_ptr->pending.properties |= WXT_PROP_FULLSCREEN;
    wxt_ptr->pending.fullscreen = *fullscreen_ptr;
    if (wxt_ptr->pending.fullscreen) wxt_ptr->pending.maximized = false;
    _wlmaker_xdg_toplevel_flush_properties(wxt_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the window's request for going to maximized. */
void _wlmaker_xdg_toplevel_handle_window_request_maximized(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlmaker_xdg_toplevel *wxt_ptr = BS_CONTAINER_OF(
        listener_ptr,
        struct wlmaker_xdg_toplevel,
        window_request_maximized_listener);
    bool *maximized_ptr = data_ptr;

    wxt_ptr->pending.properties |= WXT_PROP_MAXIMIZED;
    wxt_ptr->pending.maximized = *maximized_ptr;
    _wlmaker_xdg_toplevel_flush_properties(wxt_ptr);
}

/** == Unit test helpers =================================================== */

/** Data needed for using an output layout for tests. */
struct _wlmaker_test_layout {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    struct wl_display         *wl_display_ptr;
    struct wlr_output_layout  *wlr_output_layout_ptr;
    struct wlr_output         wlr_output;
#endif
};

static bool _wlmaker_test_layout_init(struct _wlmaker_test_layout *tl_ptr);
static void _wlmaker_test_layout_fini(struct _wlmaker_test_layout *tl_ptr);

/** Initializes @ref _wlmaker_test_layout */
bool _wlmaker_test_layout_init(struct _wlmaker_test_layout *tl_ptr)
{
    *tl_ptr = (struct _wlmaker_test_layout){};

    tl_ptr->wl_display_ptr = wl_display_create();
    if (NULL == tl_ptr->wl_display_ptr) return false;

    tl_ptr->wlr_output_layout_ptr = wlr_output_layout_create(
        tl_ptr->wl_display_ptr);
    if (NULL == tl_ptr->wl_display_ptr) {
        _wlmaker_test_layout_fini(tl_ptr);
        return false;
    }

    tl_ptr->wlr_output = (struct wlr_output){
        .name = "Output", .width = 1024, .height = 768, .scale = 1
    };
    wlmtk_test_wlr_output_init(&tl_ptr->wlr_output);

    return true;
}

/** Un-initializes @ref _wlmaker_test_layout */
void _wlmaker_test_layout_fini(struct _wlmaker_test_layout *tl_ptr)
{
    if (NULL != tl_ptr->wl_display_ptr) {
        wl_display_destroy(tl_ptr->wl_display_ptr);
        tl_ptr->wl_display_ptr = NULL;
    }
}

/** Data for testing an XDG toplevel. */
struct _xdg_toplevel_test_data {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    struct wlr_surface        wlr_surface;
    struct wlr_xdg_surface    wlr_xdg_surface;
    struct wlr_xdg_toplevel   wlr_xdg_toplevel;

    struct _wlmaker_test_layout test_layout;
    wlmaker_server_t          server;

    int32_t                   set_size_width;
    int32_t                   set_size_height;

    int                       set_maximized_calls;
    int                       set_fullscreen_calls;
    int                       set_size_calls;
    int                       set_activated_calls;
#endif
};

/** A fake for wlr_xdg_toplevel_set_maximized(). Records the call. */
uint32_t _wlmaker_xdg_toplevel_fake_set_maximized(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    __UNUSED__ bool maximized)
{
    struct _xdg_toplevel_test_data *td_ptr = BS_CONTAINER_OF(
        wlr_xdg_toplevel_ptr, struct _xdg_toplevel_test_data, wlr_xdg_toplevel);

    td_ptr->set_maximized_calls++;
    return 0;
}

/** A fake for wlr_xdg_toplevel_set_fullscreen(). Records the call. */
uint32_t _wlmaker_xdg_toplevel_fake_set_fullscreen(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    __UNUSED__ bool fullscreen)
{
    struct _xdg_toplevel_test_data *td_ptr = BS_CONTAINER_OF(
        wlr_xdg_toplevel_ptr, struct _xdg_toplevel_test_data, wlr_xdg_toplevel);

    td_ptr->set_fullscreen_calls++;
    return 0;
}

/** A fake for wlr_xdg_toplevel_set_size(). Recods the call. */
uint32_t _wlmaker_xdg_toplevel_fake_set_size(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    int32_t width,
    int32_t height)
{
    struct _xdg_toplevel_test_data *td_ptr = BS_CONTAINER_OF(
        wlr_xdg_toplevel_ptr, struct _xdg_toplevel_test_data, wlr_xdg_toplevel);

    td_ptr->set_size_calls++;
    td_ptr->set_size_width = width;
    td_ptr->set_size_height = height;
    return 0;
}

/** A fake for wlr_xdg_toplevel_set_activated(). Recods the call. */
uint32_t _wlmaker_xdg_toplevel_fake_set_activated(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    __UNUSED__ bool activated)
{
    struct _xdg_toplevel_test_data *td_ptr = BS_CONTAINER_OF(
        wlr_xdg_toplevel_ptr, struct _xdg_toplevel_test_data, wlr_xdg_toplevel);

    td_ptr->set_activated_calls++;
    return 0;
}

/* == Unit tests =========================================================== */

static void *_wlmaker_xdg_toplevel_test_setup(void);
static void _wlmaker_xdg_toplevel_test_teardown(void *test_context_ptr);

static void _wlmaker_xdg_toplevel_test_maximize(bs_test_t *test_ptr);
static void _wlmaker_xdg_toplevel_test_fullscreen(bs_test_t *test_ptr);
static void _wlmaker_xdg_toplevel_test_size(bs_test_t *test_ptr);
static void _wlmaker_xdg_toplevel_test_activated(bs_test_t *test_ptr);

/** Unit test cases. */
const bs_test_case_t _test_cases[] = {
    { true, "maximize", _wlmaker_xdg_toplevel_test_maximize },
    { true, "fullscreen", _wlmaker_xdg_toplevel_test_fullscreen },
    { true, "size", _wlmaker_xdg_toplevel_test_size },
    { true, "activated", _wlmaker_xdg_toplevel_test_activated },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_xdg_toplevel_test_set = BS_TEST_SET_CONTEXT(
    true,
    "xdg_toplevel",
    _test_cases,
    _wlmaker_xdg_toplevel_test_setup,
    _wlmaker_xdg_toplevel_test_teardown);

/* ------------------------------------------------------------------------- */
/** Creates a @ref _xdg_toplevel_test_data in the test context. */
void *_wlmaker_xdg_toplevel_test_setup(void)
{
    struct _xdg_toplevel_test_data *td_ptr = logged_calloc(
        1, sizeof(struct _xdg_toplevel_test_data));
    if (NULL == td_ptr) return NULL;
    td_ptr->wlr_xdg_surface.surface = &td_ptr->wlr_surface;
    td_ptr->wlr_xdg_toplevel.base = &td_ptr->wlr_xdg_surface;

    if (!_wlmaker_test_layout_init(&td_ptr->test_layout)) {
        _wlmaker_xdg_toplevel_test_teardown(td_ptr);
        return NULL;
    }
    wlr_output_layout_add_auto(
        td_ptr->test_layout.wlr_output_layout_ptr,
        &td_ptr->test_layout.wlr_output);

    td_ptr->server.wlr_scene_ptr = wlr_scene_create();
    if (NULL == td_ptr->server.wlr_scene_ptr) {
        _wlmaker_xdg_toplevel_test_teardown(td_ptr);
        return NULL;
    }

    td_ptr->server.root_ptr = wlmtk_root_create(
        td_ptr->server.wlr_scene_ptr,
        td_ptr->test_layout.wlr_output_layout_ptr);
    if (NULL == td_ptr->server.root_ptr) {
        _wlmaker_xdg_toplevel_test_teardown(td_ptr);
        return NULL;
    }

    wl_signal_init(&td_ptr->server.window_created_event);
    wl_signal_init(&td_ptr->server.window_destroyed_event);

    wl_signal_init(&td_ptr->wlr_surface.events.commit);
    wl_signal_init(&td_ptr->wlr_surface.events.map);
    wl_signal_init(&td_ptr->wlr_surface.events.unmap);

    wl_signal_init(&td_ptr->wlr_xdg_surface.events.destroy);
    wl_signal_init(&td_ptr->wlr_xdg_surface.events.ping_timeout);
    wl_signal_init(&td_ptr->wlr_xdg_surface.events.new_popup);
    wl_signal_init(&td_ptr->wlr_xdg_surface.events.configure);
    wl_signal_init(&td_ptr->wlr_xdg_surface.events.ack_configure);

    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.destroy);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_maximize);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_fullscreen);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_minimize);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_move);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_resize);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.request_show_window_menu);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.set_parent);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.set_title);
    wl_signal_init(&td_ptr->wlr_xdg_toplevel.events.set_app_id);

    return td_ptr;
}

/* ------------------------------------------------------------------------- */
/** Tear down XDG toplevel data. */
void _wlmaker_xdg_toplevel_test_teardown(void *test_context_ptr)
{
    struct _xdg_toplevel_test_data *td_ptr = test_context_ptr;

    if (NULL != td_ptr->server.root_ptr) {
        wlmtk_root_destroy(td_ptr->server.root_ptr);
        td_ptr->server.root_ptr = NULL;
    }

    wlr_scene_node_destroy(&td_ptr->server.wlr_scene_ptr->tree.node);

    _wlmaker_test_layout_fini(&td_ptr->test_layout);
    free(td_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests maximize requests, from client & window. */
void _wlmaker_xdg_toplevel_test_maximize(bs_test_t *test_ptr)
{
    struct _xdg_toplevel_test_data *td_ptr =
        BS_ASSERT_NOTNULL(bs_test_context(test_ptr));

    wlmtk_util_test_listener_t created, destroyed;
    wlmtk_util_connect_test_listener(
        &td_ptr->server.window_created_event, &created);
    wlmtk_util_connect_test_listener(
        &td_ptr->server.window_destroyed_event, &destroyed);

    struct wlmaker_xdg_toplevel *wxt_ptr =
        _wlmaker_xdg_toplevel_create_injected(
            &td_ptr->wlr_xdg_toplevel,
            &td_ptr->server,
            _wlmaker_xdg_toplevel_fake_set_maximized,
            _wlmaker_xdg_toplevel_fake_set_fullscreen,
            _wlmaker_xdg_toplevel_fake_set_size,
            _wlmaker_xdg_toplevel_fake_set_activated);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wxt_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, created.calls);

    // No initial commit yet. No `set_maximized`.
    wl_signal_emit(&td_ptr->wlr_xdg_toplevel.events.request_maximize, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_maximized_calls);

    // Now we have pending properties. Commit must trigger a `configure`.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_maximized_calls);

    // When initialized a `request_maximize` configures right away.
    td_ptr->set_maximized_calls = 0;
    wl_signal_emit(&td_ptr->wlr_xdg_toplevel.events.request_maximize, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_maximized_calls);

    // Issued from the window, ie. from the compositor.
    bool m = true;
    td_ptr->wlr_xdg_surface.initialized = false;  // Ready for configure().
    td_ptr->set_maximized_calls = 0;
    wl_signal_emit(
        &wlmtk_window_events(wxt_ptr->window_ptr)->request_maximized, &m);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_maximized_calls);

    // Pending properties. Next commit must trigger.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_maximized_calls);

    td_ptr->set_maximized_calls = 0;
    wl_signal_emit(
        &wlmtk_window_events(wxt_ptr->window_ptr)->request_maximized, &m);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_maximized_calls);

    wlmaker_xdg_toplevel_destroy(wxt_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, destroyed.calls);
}

/* ------------------------------------------------------------------------- */
/** Tests fullscreen requests, from client & window. */
void _wlmaker_xdg_toplevel_test_fullscreen(bs_test_t *test_ptr)
{
    struct _xdg_toplevel_test_data *td_ptr =
        BS_ASSERT_NOTNULL(bs_test_context(test_ptr));

    struct wlmaker_xdg_toplevel *wxt_ptr =
        _wlmaker_xdg_toplevel_create_injected(
            &td_ptr->wlr_xdg_toplevel,
            &td_ptr->server,
            _wlmaker_xdg_toplevel_fake_set_maximized,
            _wlmaker_xdg_toplevel_fake_set_fullscreen,
            _wlmaker_xdg_toplevel_fake_set_size,
            _wlmaker_xdg_toplevel_fake_set_activated);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wxt_ptr);

    // Issued from the window, ie. from the compositor.
    bool f = true;
    wl_signal_emit(
        &wlmtk_window_events(wxt_ptr->window_ptr)->request_fullscreen, &f);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_fullscreen_calls);

    // Pending properties. Next commit must trigger.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_fullscreen_calls);

    // Issue from the client. No commit yet. Must not configure().
    td_ptr->wlr_xdg_surface.initialized = false;  // Not ready for configure().
    td_ptr->set_fullscreen_calls = 0;
    wl_signal_emit(&td_ptr->wlr_xdg_toplevel.events.request_fullscreen, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_fullscreen_calls);

    // Following commit must trigger a `configure`.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_fullscreen_calls);

    wlmaker_xdg_toplevel_destroy(wxt_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests set_size requests. */
void _wlmaker_xdg_toplevel_test_size(bs_test_t *test_ptr)
{
    struct _xdg_toplevel_test_data *td_ptr =
        BS_ASSERT_NOTNULL(bs_test_context(test_ptr));

    struct wlmaker_xdg_toplevel *wxt_ptr =
        _wlmaker_xdg_toplevel_create_injected(
            &td_ptr->wlr_xdg_toplevel,
            &td_ptr->server,
            _wlmaker_xdg_toplevel_fake_set_maximized,
            _wlmaker_xdg_toplevel_fake_set_fullscreen,
            _wlmaker_xdg_toplevel_fake_set_size,
            _wlmaker_xdg_toplevel_fake_set_activated);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wxt_ptr);

    // Issued from the window, ie. from the compositor.
    struct wlr_box b = { .width = 10, .height = 20 };
    wl_signal_emit(
        &wlmtk_window_events(wxt_ptr->window_ptr)->request_size, &b);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_size_calls);

    // Pending properties. Next commit must trigger.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_size_calls);
    BS_TEST_VERIFY_EQ(test_ptr, 10, td_ptr->set_size_width);
    BS_TEST_VERIFY_EQ(test_ptr, 20, td_ptr->set_size_height);

    wlmaker_xdg_toplevel_destroy(wxt_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests set_activated requests. */
void _wlmaker_xdg_toplevel_test_activated(bs_test_t *test_ptr)
{
    struct _xdg_toplevel_test_data *td_ptr =
        BS_ASSERT_NOTNULL(bs_test_context(test_ptr));

    struct wlmaker_xdg_toplevel *wxt_ptr =
        _wlmaker_xdg_toplevel_create_injected(
            &td_ptr->wlr_xdg_toplevel,
            &td_ptr->server,
            _wlmaker_xdg_toplevel_fake_set_maximized,
            _wlmaker_xdg_toplevel_fake_set_fullscreen,
            _wlmaker_xdg_toplevel_fake_set_size,
            _wlmaker_xdg_toplevel_fake_set_activated);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wxt_ptr);

    // Issued from the window, ie. from the compositor.
    bool a = true;
    wl_signal_emit(
        &wlmtk_window_events(wxt_ptr->window_ptr)->set_activated, &a);
    BS_TEST_VERIFY_EQ(test_ptr, 0, td_ptr->set_activated_calls);

    // Pending properties. Next commit must trigger.
    td_ptr->wlr_xdg_surface.initialized = true;  // Ready for configure().
    wl_signal_emit(&td_ptr->wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 1, td_ptr->set_activated_calls);

    wlmaker_xdg_toplevel_destroy(wxt_ptr);
}

/* == End of xdg_toplevel.c ================================================ */
