/* ========================================================================= */
/**
 * @file xwl_window.c
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

#include "xwl_window.h"

#include <libbase/libbase.h>

#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/xwayland.h>
#include <wlr/types/wlr_compositor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the XWayland window content. */
struct _wlmaker_xwl_window_t {
    /** Corresponding wlroots XWayland surface. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    wlmtk_env_t               *env_ptr;
    wlmaker_server_t          *server_ptr;


    struct wlr_scene_surface  *wlr_scene_surface_ptr;

    /** Toolkit content state. */
    wlmtk_content_t           content;
    wlmtk_surface_t           surface;
    wlmtk_window_t            *window_ptr;

    /** Listener for the `destroy` signal of `wlr_xwayland_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_configure` signal of `wlr_xwayland_surface`. */
    struct wl_listener        request_configure_listener;
    /** Listener for the `associate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        associate_listener;
    /** Listener for the `dissociate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        dissociate_listener;

    /** FIXME: listener for surface commit. */
    struct wl_listener        surface_commit_listener;
    struct wl_listener        surface_map_listener;
    struct wl_listener        surface_unmap_listener;
};

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_associate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_dissociate(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void surface_element_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *surface_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/* == Data ================================================================= */

const wlmtk_element_vmt_t     _xwl_surface_element_vmt = {
    .destroy = surface_element_destroy,
    .create_scene_node = surface_element_create_scene_node,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_window_t *wlmaker_xwl_window_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_window_t));
    if (NULL == xwl_window_ptr) return NULL;
    xwl_window_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;
    xwl_window_ptr->server_ptr = server_ptr;
    xwl_window_ptr->env_ptr = server_ptr->env_ptr;

    if (!wlmtk_content_init(&xwl_window_ptr->content,
                            NULL,
                            xwl_window_ptr->env_ptr)) {
        wlmaker_xwl_window_destroy(xwl_window_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.destroy,
        &xwl_window_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.request_configure,
        &xwl_window_ptr->request_configure_listener,
        handle_request_configure);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.associate,
        &xwl_window_ptr->associate_listener,
        handle_associate);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.dissociate,
        &xwl_window_ptr->dissociate_listener,
        handle_dissociate);

    bs_log(BS_INFO, "Created XWL window %p for wlr_xwayland_surface %p",
           xwl_window_ptr, wlr_xwayland_surface_ptr);

    return xwl_window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_window_destroy(wlmaker_xwl_window_t *xwl_window_ptr)
{
    bs_log(BS_INFO, "Destroy XWL window %p", xwl_window_ptr);

    wl_list_remove(&xwl_window_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_window_ptr->associate_listener.link);
    wl_list_remove(&xwl_window_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_window_ptr->destroy_listener.link);

    wlmtk_content_fini(&xwl_window_ptr->content);

    free(xwl_window_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, destroy_listener);
    wlmaker_xwl_window_destroy(xwl_window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_configure` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, request_configure_listener);
    struct wlr_xwayland_surface_configure_event *cfg_event_ptr = data_ptr;

    bs_log(BS_INFO, "Reqeust configure for %p: "
           "%"PRId16" x %"PRId16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_window_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);
    bs_log(BS_INFO, "wlr_surface %p",
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    // FIXME:
    // -> if we have content/surface: check what that means, with respect to
    //    the surface::commit handler.
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `associate` event of `struct wlr_xwayland_surface`.
 *
 * The `associate` event is triggered once an X11 window becomes associated
 * with the surface. Understanding this is a moment the surface can be mapped.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, associate_listener);
    bs_log(BS_INFO, "Associate %p with wlr_surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    // Here: Create the surface using wlr_scene_surface_create on
    // wlr_xwayland_surface_ptr->surface  (a wlr_surface)

    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.commit,
        &xwl_window_ptr->surface_commit_listener,
        handle_surface_commit);
    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.map,
        &xwl_window_ptr->surface_map_listener,
        handle_surface_map);
    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.unmap,
        &xwl_window_ptr->surface_unmap_listener,
        handle_surface_unmap);

    if (!wlmtk_surface_init(
            &xwl_window_ptr->surface,
            xwl_window_ptr->wlr_xwayland_surface_ptr->surface,
            xwl_window_ptr->env_ptr)) {
        bs_log(BS_ERROR, "FIXME: Error.");
    }
    wlmtk_element_extend(
        &xwl_window_ptr->surface.super_element,
        &_xwl_surface_element_vmt);

    wlmtk_content_set_surface(
        &xwl_window_ptr->content,
        &xwl_window_ptr->surface);
    xwl_window_ptr->window_ptr = wlmtk_window_create(
        &xwl_window_ptr->content,
        xwl_window_ptr->env_ptr);
    if (NULL == xwl_window_ptr->window_ptr) {
        bs_log(BS_ERROR, "FIXME: Error.");
    }
    wlmtk_window_set_server_side_decorated(xwl_window_ptr->window_ptr, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_dissociate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, dissociate_listener);

    wl_list_remove(&xwl_window_ptr->surface_unmap_listener.link);
    wl_list_remove(&xwl_window_ptr->surface_map_listener.link);
    wl_list_remove(&xwl_window_ptr->surface_commit_listener.link);

    if (NULL != xwl_window_ptr->window_ptr) {
        wlmtk_window_destroy(xwl_window_ptr->window_ptr);
        xwl_window_ptr->window_ptr = NULL;
    }

    wlmtk_surface_fini(&xwl_window_ptr->surface);

    bs_log(BS_INFO, "Dissociate %p from wlr_surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    // Here: Destroy the actual surface.
}

/* ------------------------------------------------------------------------- */
/** Temporary: Surface commit handler. */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_commit_listener);

    bs_log(BS_INFO, "XWL window %p commit surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    bs_log(BS_INFO, "current width x height: %d x %d",
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.width,
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.height);
    bs_log(BS_INFO, "pending width x height: %d x %d",
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->pending.width,
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->pending.height);

    wlmtk_surface_commit_size(
        &xwl_window_ptr->surface, 0,
        xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.width,
        xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.height);
}

/* ------------------------------------------------------------------------- */
/** Temporary: Surface map handler. */
void handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_map_listener);

    bs_log(BS_INFO, "XWL window %p map surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        xwl_window_ptr->server_ptr);

    wlmtk_workspace_map_window(
        wlmaker_workspace_wlmtk(workspace_ptr),
        xwl_window_ptr->window_ptr);
    wlmtk_window_set_server_side_decorated(xwl_window_ptr->window_ptr, true);
    wlmtk_window_set_position(xwl_window_ptr->window_ptr, 40, 30);
    // FIXME
}

/* ------------------------------------------------------------------------- */
/** Temporary: Surface unmap handler. */
void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_unmap_listener);

    bs_log(BS_INFO, "XWL window %p unmap surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

}

void surface_element_destroy(wlmtk_element_t *element_ptr)
{
    element_ptr = element_ptr;
}

struct wlr_scene_node *surface_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_xwl_window_t, surface.super_element);


    xwl_window_ptr->wlr_scene_surface_ptr = wlr_scene_surface_create(
        wlr_scene_tree_ptr,
        xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    return &xwl_window_ptr->wlr_scene_surface_ptr->buffer->node;
}


/* == End of xwl_window.c ================================================== */
