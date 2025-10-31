/* ========================================================================= */
/**
 * @file xdg_popup.c
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

#include "xdg_popup.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-protocol.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of toolkit popup. */
struct _wlmaker_xdg_popup_t {
    /** Base element for the popup. Surface is the content. */
    wlmtk_base_t              base;

    /** Listener for the `reposition` signal of `wlr_xdg_popup::events` */
    struct wl_listener        reposition_listener;
    /** Listener for the `destroy` signal of `wlr_xdg_surface::events`. */
    struct wl_listener        destroy_listener;
    /** Listener for the `new_popup` signal of `wlr_xdg_surface::events`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The WLR popup. */
    struct wlr_xdg_popup      *wlr_xdg_popup_ptr;
};

static void handle_reposition(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_xdg_popup_element_destroy(
    wlmtk_element_t *element_ptr);

/* == Data ================================================================= */

/** Virtual method table of the parent's @ref wlmtk_element_t. */
static const wlmtk_element_vmt_t _wlmaker_xdg_popup_element_vmt = {
    .destroy = _wlmaker_xdg_popup_element_destroy
};

static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    struct wlr_seat *wlr_seat_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_popup_t));
    if (NULL == wlmaker_xdg_popup_ptr) return NULL;
    wlmaker_xdg_popup_ptr->wlr_seat_ptr = wlr_seat_ptr;
    wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr = wlr_xdg_popup_ptr;

    if (!wlmtk_base_init(&wlmaker_xdg_popup_ptr->base, NULL)) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmaker_xdg_popup_element(wlmaker_xdg_popup_ptr), true);

    wlmtk_surface_t *surface_ptr = wlmtk_surface_create(
        wlr_xdg_popup_ptr->base->surface,
        wlr_seat_ptr);
    if (NULL == surface_ptr) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }
    wlmtk_base_set_content_element(
        &wlmaker_xdg_popup_ptr->base,
        wlmtk_surface_element(surface_ptr));
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->surface->events.commit,
        &wlmaker_xdg_popup_ptr->surface_commit_listener,
        handle_surface_commit);

    wlmtk_element_extend(
        wlmtk_base_element(&wlmaker_xdg_popup_ptr->base),
        &_wlmaker_xdg_popup_element_vmt);
    wlmtk_element_set_position(
        wlmtk_base_element(&wlmaker_xdg_popup_ptr->base),
        wlr_xdg_popup_ptr->scheduled.geometry.x,
        wlr_xdg_popup_ptr->scheduled.geometry.y);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->events.reposition,
        &wlmaker_xdg_popup_ptr->reposition_listener,
        handle_reposition);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->events.destroy,
        &wlmaker_xdg_popup_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.new_popup,
        &wlmaker_xdg_popup_ptr->new_popup_listener,
        handle_new_popup);

    bs_log(BS_INFO, "Created XDG popup %p", wlmaker_xdg_popup_ptr);
    return wlmaker_xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr)
{
    bs_log(BS_INFO, "Destroying XDG popup %p", wlmaker_xdg_popup_ptr);

    // For XDG popups, we don't keep reference when adding to a container. So
    // we need to remove from a potential parent container.
    wlmtk_element_t *e = wlmtk_base_element(&wlmaker_xdg_popup_ptr->base);
    if (NULL != e->parent_container_ptr) {
        wlmtk_container_remove_element(e->parent_container_ptr, e);
    }

    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->new_popup_listener);
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->destroy_listener);
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->reposition_listener);
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->surface_commit_listener);

    wlmtk_base_fini(&wlmaker_xdg_popup_ptr->base);
    free(wlmaker_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmaker_xdg_popup_element(
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr)
{
    return wlmtk_base_element(&wlmaker_xdg_popup_ptr->base);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles repositioning. */
void handle_reposition(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, reposition_listener);

    wlmtk_element_set_position(
        wlmtk_base_element(&wlmaker_xdg_popup_ptr->base),
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->scheduled.geometry.x,
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->scheduled.geometry.y);
}

/* ------------------------------------------------------------------------- */
/** Handles popup destruction: Removes from parent, and destroy. */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, destroy_listener);

    wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles further popups. Creates them and adds them to parent's content. */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *new_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr,
        wlmaker_xdg_popup_ptr->wlr_seat_ptr);
    if (NULL == new_popup_ptr) {
        wl_resource_post_error(
            wlr_xdg_popup_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmtk_xdg_popup_create.");
        return;
    }

    wlmtk_base_push_element(
        &wlmaker_xdg_popup_ptr->base,
        wlmaker_xdg_popup_element(new_popup_ptr));

    bs_log(BS_INFO, "XDG popup %p: New popup %p",
           wlmaker_xdg_popup_ptr, wlr_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles `commit` for the popup's surface. */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, surface_commit_listener);

    if (wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->base->initial_commit) {
        // Initial commit: Ensure a configure is responded with.
        wlr_xdg_surface_schedule_configure(
            wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->base);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_element_vmt_t::destroy. Virtual dtor.
 *
 * @param element_ptr
 */
void _wlmaker_xdg_popup_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_xdg_popup_t,
        base.super_container.super_element);

    wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
}

/* == End of xdg_popup.c ==================================================== */
