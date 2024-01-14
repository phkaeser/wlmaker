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

    /** Toolkit content state. */
    wlmtk_content_t           content;

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

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_window_t *wlmaker_xwl_window_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_window_t));
    if (NULL == xwl_window_ptr) return NULL;
    xwl_window_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;

    if (!wlmtk_content_init(&xwl_window_ptr->content, NULL, NULL)) {
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

    wl_list_remove(&xwl_window_ptr->surface_commit_listener.link);

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
}

/* == End of xwl_window.c ================================================== */
