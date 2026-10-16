/* ========================================================================= */
/**
 * @file container_surface.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "container_surface.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <toolkit/toolkit.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

struct wl_surface;
struct wlr_allocator;
struct wlr_backend;
struct wlr_renderer;

/* == Declarations ========================================================= */

/** State of the container surface. */
struct wlmdock_container_surface {
    /* -- Owned objects objects ------------------------- */
    /** Root container for what will be rendered to the surface. */
    wlmtk_container_t         container;

    /** The local scene graph, to represent all from @ref container. */
    struct wlr_scene          *wlr_scene_ptr;
    /** For `events.frame` of @ref wlr_output_ptr. */
    struct wl_listener        output_frame_listener;

    /* -- References to parent objects ------------------ */
    /** Output layout for this surface's compositor. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** wlroots output for this surface's compositor. */
    struct wlr_output         *wlr_output_ptr;
    /** Scene-graph output for the scene on @ref wlr_output_ptr. */
    struct wlr_scene_output   *wlr_scene_output_ptr;
};

static void _wlmdock_container_surface_handle_output_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmdock_container_surface *wlmdock_container_surface_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    wlmtk_container_t *container_ptr,
    struct wl_surface *wl_surface_ptr)
{
    struct wlmdock_container_surface *container_surface_ptr = logged_calloc(
        1, sizeof(*container_surface_ptr));
    if (NULL == container_surface_ptr) return NULL;
    container_surface_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    container_surface_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == container_surface_ptr->wlr_scene_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_create()");
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }

    if (!wlmtk_container_init_attached(
            &container_surface_ptr->container,
            &container_surface_ptr->wlr_scene_ptr->tree)) {
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &container_surface_ptr->container.super_element,
        true);
    wlmtk_container_add_element(
        &container_surface_ptr->container,
        &container_ptr->super_element);

    if (NULL == wlr_scene_attach_output_layout(
            container_surface_ptr->wlr_scene_ptr,
            container_surface_ptr->wlr_output_layout_ptr)) {
        // Note: The `struct wlr_scene_output_layout` is destroyed when the
        // scene or output layout gets destroyed.
        bs_log(BS_ERROR, "Failed wlr_scene_attach_output_layout(%p, %p)",
               container_surface_ptr->wlr_scene_ptr,
               container_surface_ptr->wlr_output_layout_ptr);
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }

    container_surface_ptr->wlr_output_ptr = wlr_wl_output_create_from_surface(
        wlr_backend_ptr,
        wl_surface_ptr);
    if (NULL == container_surface_ptr->wlr_output_ptr) {
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }

    if (!wlr_output_init_render(
            container_surface_ptr->wlr_output_ptr,
            wlr_allocator_ptr,
            wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer(%p, %p, %p)",
               container_surface_ptr->wlr_output_ptr,
               wlr_allocator_ptr,
               wlr_renderer_ptr);
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }

    container_surface_ptr->wlr_scene_output_ptr = wlr_scene_output_create(
        container_surface_ptr->wlr_scene_ptr,
        container_surface_ptr->wlr_output_ptr);
    if (NULL == container_surface_ptr->wlr_scene_output_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_output_create(%p, %p)",
               container_surface_ptr->wlr_scene_ptr,
               container_surface_ptr->wlr_output_ptr);
        wlmdock_container_surface_destroy(container_surface_ptr);
        return NULL;
    }

    wlr_output_layout_add_auto(
        container_surface_ptr->wlr_output_layout_ptr,
        container_surface_ptr->wlr_output_ptr);
    wlmtk_util_connect_listener_signal(
        &container_surface_ptr->wlr_output_ptr->events.frame,
        &container_surface_ptr->output_frame_listener,
        _wlmdock_container_surface_handle_output_frame);

    return container_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_container_surface_destroy(
    struct wlmdock_container_surface *container_surface_ptr)
{
    wlmtk_util_disconnect_listener(
        &container_surface_ptr->output_frame_listener);

    if (NULL != container_surface_ptr->wlr_scene_output_ptr) {
        wlr_scene_output_destroy(container_surface_ptr->wlr_scene_output_ptr);
        container_surface_ptr->wlr_scene_output_ptr = NULL;
    }

    if (NULL != container_surface_ptr->wlr_output_ptr) {
        wlr_output_layout_remove(
            container_surface_ptr->wlr_output_layout_ptr,
            container_surface_ptr->wlr_output_ptr);
        wlr_output_destroy(container_surface_ptr->wlr_output_ptr);
        container_surface_ptr->wlr_output_ptr = NULL;
    }

    wlmtk_container_remove_element(
        &container_surface_ptr->container,
        wlmtk_element_from_dlnode(
            container_surface_ptr->container.elements.head_ptr));
    wlmtk_container_fini(&container_surface_ptr->container);

    if (NULL != container_surface_ptr->wlr_scene_ptr) {
        wlr_scene_node_destroy(
            &container_surface_ptr->wlr_scene_ptr->tree.node);
        container_surface_ptr->wlr_scene_ptr = NULL;
    }

    free(container_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmdock_container_surface_commit_size(
    struct wlmdock_container_surface *container_surface_ptr,
    int width,
    int height)
{
    if (0 >= width || 0 >= height) return;

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, width > 0 && height > 0);
    wlr_output_state_set_custom_mode(&state, width, height, 0);
    if (!wlr_output_commit_state(
            container_surface_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_WARNING, "Failed wlr_output_commit_state(%p, %p) for %dx%d",
               container_surface_ptr->wlr_output_ptr, &state, width, height);
    }
    wlr_output_state_finish(&state);

    if (!wlr_scene_output_commit(
            container_surface_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "Failed wlr_scene_output_commit(%p, NULL)",
               container_surface_ptr->wlr_scene_output_ptr);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Commits the frame. */
void _wlmdock_container_surface_handle_output_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmdock_container_surface *container_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmdock_container_surface, output_frame_listener);

    if (!wlr_scene_output_commit(
            container_surface_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "wlr_scene_output_commit(%p, NULL) failed.",
               container_surface_ptr->wlr_scene_output_ptr);
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(
        container_surface_ptr->wlr_scene_output_ptr, &now);
}

/* == End of container_surface.c =========================================== */
