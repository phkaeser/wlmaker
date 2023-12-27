/* ========================================================================= */
/**
 * @file xdg_decoration.c
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

#include "xdg_decoration.h"

#include <libbase/libbase.h>

#include "config.h"
#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_decoration_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the XDG decoration manager. */
struct _wlmaker_xdg_decoration_manager_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
    /** The wlroots XDG decoration manager. */
    struct wlr_xdg_decoration_manager_v1* wlr_xdg_decoration_manager_v1_ptr;

    /** Listener for `new_toplevel_decoration`. */
    struct wl_listener        new_toplevel_decoration_listener;
    /** Listener for `destroy` of `wlr_xdg_decoration_manager_v1`. */
    struct wl_listener        destroy_listener;
};

/** A decoration handle. */
typedef struct {
    /** Points to the wlroots `wlr_xdg_toplevel_decoration_v1`. */
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_toplevel_decoration_v1_ptr;

    /** Listener for `request_mode` of `wlr_xdg_toplevel_decoration_v1`. */
    struct wl_listener        request_mode_listener;
    /** Listener for `destroy` of `wlr_xdg_toplevel_decoration_v1.` */
    struct wl_listener        destroy_listener;
} wlmaker_xdg_decoration_t;

static void handle_new_toplevel_decoration(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_xdg_decoration_t *wlmaker_xdg_decoration_create(
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_toplevel_decoration_v1_ptr);
static void wlmaker_xdg_decoration_destroy(
    wlmaker_xdg_decoration_t *decoration_ptr);

static void handle_decoration_request_mode(
    struct wl_listener *listener_ptr,
     void *data_ptr);
static void handle_decoration_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_decoration_manager_t *wlmaker_xdg_decoration_manager_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_xdg_decoration_manager_t *decoration_manager_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_decoration_manager_t));
    if (NULL == decoration_manager_ptr) return NULL;
    decoration_manager_ptr->server_ptr = server_ptr;

    decoration_manager_ptr->wlr_xdg_decoration_manager_v1_ptr =
        wlr_xdg_decoration_manager_v1_create(server_ptr->wl_display_ptr);
    if (NULL == decoration_manager_ptr->wlr_xdg_decoration_manager_v1_ptr) {
        wlmaker_xdg_decoration_manager_destroy(decoration_manager_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &decoration_manager_ptr->wlr_xdg_decoration_manager_v1_ptr->
        events.new_toplevel_decoration,
        &decoration_manager_ptr->new_toplevel_decoration_listener,
        handle_new_toplevel_decoration);
    wlmtk_util_connect_listener_signal(
        &decoration_manager_ptr->wlr_xdg_decoration_manager_v1_ptr->
        events.destroy,
        &decoration_manager_ptr->destroy_listener,
        handle_destroy);

    return decoration_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_decoration_manager_destroy(
    wlmaker_xdg_decoration_manager_t *decoration_manager_ptr)
{
    wl_list_remove(
        &decoration_manager_ptr->new_toplevel_decoration_listener.link);
    wl_list_remove(
        &decoration_manager_ptr->destroy_listener.link);

    free(decoration_manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_toplevel_decoration` signal of
 * `wlr_xdg_decoration_manager_v1`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to `wlr_xdg_toplevel_decoration_v1`.
 */
void handle_new_toplevel_decoration(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xdg_decoration_manager_t *decoration_manager_ptr = wl_container_of(
        listener_ptr, decoration_manager_ptr, new_toplevel_decoration_listener);
    struct wlr_xdg_toplevel_decoration_v1
        *wlr_xdg_toplevel_decoration_v1_ptr = data_ptr;

    wlmaker_xdg_decoration_t *decoration_ptr = wlmaker_xdg_decoration_create(
        wlr_xdg_toplevel_decoration_v1_ptr);
    if (NULL == decoration_ptr) return;

    handle_decoration_request_mode(
        &decoration_ptr->request_mode_listener, NULL);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_xdg_decoration_manager_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
static void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_decoration_manager_t *decoration_manager_ptr = wl_container_of(
        listener_ptr, decoration_manager_ptr, destroy_listener);

    free(decoration_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a decoration handle.
 *
 * @param wlr_xdg_toplevel_decoration_v1_ptr
 *
 * @returns The decoration handle or NULL on error.
 */
wlmaker_xdg_decoration_t *wlmaker_xdg_decoration_create(
    struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_toplevel_decoration_v1_ptr)
{
    wlmaker_xdg_decoration_t *decoration_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_decoration_t));
    if (NULL == decoration_ptr) return NULL;
    decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr =
        wlr_xdg_toplevel_decoration_v1_ptr;

    wlmtk_util_connect_listener_signal(
        &decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->events.destroy,
        &decoration_ptr->destroy_listener,
        handle_decoration_destroy);
    wlmtk_util_connect_listener_signal(
        &decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->events.request_mode,
        &decoration_ptr->request_mode_listener,
        handle_decoration_request_mode);

    return decoration_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the decoration handle.
 *
 * @param decoration_ptr
 */
void wlmaker_xdg_decoration_destroy(wlmaker_xdg_decoration_t *decoration_ptr)
{
    wl_list_remove(&decoration_ptr->destroy_listener.link);
    wl_list_remove(&decoration_ptr->request_mode_listener.link);
    free(decoration_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_mode` signal of `wlr_xdg_toplevel_decoration_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_decoration_request_mode(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_decoration_t *decoration_ptr = wl_container_of(
        listener_ptr, decoration_ptr, request_mode_listener);

    struct wlr_scene_tree *wlr_scene_tree_ptr = (struct wlr_scene_tree*)
        decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->toplevel->base->data;
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)wlr_scene_tree_ptr->node.data;

    wlmtk_content_t *content_ptr = (wlmtk_content_t*)
        decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->toplevel->base->data;

    enum wlr_xdg_toplevel_decoration_v1_mode mode =
        decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->requested_mode;
    switch (config_decoration) {
    case WLMAKER_CONFIG_DECORATION_SUGGEST_CLIENT:
        if (WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE == mode) {
            mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        }
        break;

    case WLMAKER_CONFIG_DECORATION_SUGGEST_SERVER:
        if (WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE == mode) {
            mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        }
        break;

    case WLMAKER_CONFIG_DECORATION_ENFORCE_CLIENT:
        mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
        break;

    case WLMAKER_CONFIG_DECORATION_ENFORCE_SERVER:
        mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        break;

    default:
        bs_log(BS_FATAL, "Unhandled config_decoration value %d",
               config_decoration);
        BS_ABORT();
    }

    wlr_xdg_toplevel_decoration_v1_set_mode(
        decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr, mode);

    if (NULL != content_ptr &&
        content_ptr->identifier_ptr == wlmtk_content_identifier_ptr) {

        bs_log(BS_INFO, "XDG decoration request_mode for XDG surface %p, "
               "content %p: Current %d, pending %d, scheduled %d, "
               "requested %d. Set: %d",
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->toplevel->base->surface,
               content_ptr,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->current.mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->pending.mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->scheduled_mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->requested_mode,
               mode);

        wlmtk_window_set_server_side_decorated(
            content_ptr->window_ptr,
            mode != WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);

    } else {

        bs_log(BS_INFO, "XDG decoration request_mode for XDG surface %p, view %p: "
               "Current %d, pending %d, scheduled %d, requested %d. Set: %d",
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->toplevel->base->surface,
               view_ptr,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->current.mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->pending.mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->scheduled_mode,
               decoration_ptr->wlr_xdg_toplevel_decoration_v1_ptr->requested_mode,
               mode);

        wlmaker_view_set_server_side_decoration(
            view_ptr,
            mode != WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_xdg_toplevel_decoration_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_decoration_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_decoration_t *decoration_ptr = wl_container_of(
        listener_ptr, decoration_ptr, destroy_listener);
    wlmaker_xdg_decoration_destroy(decoration_ptr);
}

/* == End of xdg_decoration.c ============================================== */
