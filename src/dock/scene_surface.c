/* ========================================================================= */
/**
 * @file scene_surface.c
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

#include "scene_surface.h"

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

/** State of the scene surface. */
struct wlmdock_scene_surface {
    /** Output layout for this surface's compositor. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** wlroots output for this surface's compositor. */
    struct wlr_output         *wlr_output_ptr;
    /** Scene-graph output for the scene on @ref wlr_output_ptr. */
    struct wlr_scene_output   *wlr_scene_output_ptr;

    /** The configured width of the layou't surface, in pixels. */
    int                       configured_width;
    /** The configured height of the layou't surface, in pixels. */
    int                       configured_height;
    /** Requested width of the layout's surface, in pixels. */
    int                       requested_width;
    /** Requested height of the layout's surface, in pixels. */
    int                       requested_height;

    /** For `events.frame` of @ref wlr_output_ptr. */
    struct wl_listener        output_frame_listener;
};

static void _wlmdock_scene_surface_handle_output_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmdock_scene_surface *wlmdock_scene_surface_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wl_surface *wl_surface_ptr)
{
    struct wlmdock_scene_surface *scene_surface_ptr = logged_calloc(
        1, sizeof(*scene_surface_ptr));
    if (NULL == scene_surface_ptr) return NULL;
    scene_surface_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    if (NULL == wlr_scene_attach_output_layout(
            wlr_scene_ptr, scene_surface_ptr->wlr_output_layout_ptr)) {
        // Note: The `struct wlr_scene_output_layout` is destroyed when the
        // scene or output layout gets destroyed.
        bs_log(BS_ERROR, "Failed wlr_scene_attach_output_layout(%p, %p)",
               wlr_scene_ptr, scene_surface_ptr->wlr_output_layout_ptr);
        wlmdock_scene_surface_destroy(scene_surface_ptr);
        return NULL;
    }

    scene_surface_ptr->wlr_output_ptr = wlr_wl_output_create_from_surface(
        wlr_backend_ptr,
        wl_surface_ptr);
    if (NULL == scene_surface_ptr->wlr_output_ptr) {
        wlmdock_scene_surface_destroy(scene_surface_ptr);
        return NULL;
    }

    if (!wlr_output_init_render(
            scene_surface_ptr->wlr_output_ptr,
            wlr_allocator_ptr,
            wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer(%p, %p, %p)",
               scene_surface_ptr->wlr_output_ptr,
               wlr_allocator_ptr,
               wlr_renderer_ptr);
        wlmdock_scene_surface_destroy(scene_surface_ptr);
        return NULL;
    }

    scene_surface_ptr->wlr_scene_output_ptr = wlr_scene_output_create(
        wlr_scene_ptr,
        scene_surface_ptr->wlr_output_ptr);
    if (NULL == scene_surface_ptr->wlr_scene_output_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_output_create(%p, %p)",
               wlr_scene_ptr,
               scene_surface_ptr->wlr_output_ptr);
        wlmdock_scene_surface_destroy(scene_surface_ptr);
        return NULL;
    }

    wlr_output_layout_add_auto(
        scene_surface_ptr->wlr_output_layout_ptr,
        scene_surface_ptr->wlr_output_ptr);
    wlmtk_util_connect_listener_signal(
        &scene_surface_ptr->wlr_output_ptr->events.frame,
        &scene_surface_ptr->output_frame_listener,
        _wlmdock_scene_surface_handle_output_frame);

    return scene_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_scene_surface_destroy(
    struct wlmdock_scene_surface *scene_surface_ptr)
{
    wlmtk_util_disconnect_listener(&scene_surface_ptr->output_frame_listener);

    if (NULL != scene_surface_ptr->wlr_scene_output_ptr) {
        wlr_scene_output_destroy(scene_surface_ptr->wlr_scene_output_ptr);
        scene_surface_ptr->wlr_scene_output_ptr = NULL;
    }

    if (NULL != scene_surface_ptr->wlr_output_ptr) {
        wlr_output_layout_remove(
            scene_surface_ptr->wlr_output_layout_ptr,
            scene_surface_ptr->wlr_output_ptr);
        wlr_output_destroy(scene_surface_ptr->wlr_output_ptr);
        scene_surface_ptr->wlr_output_ptr = NULL;
    }

    free(scene_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmdock_scene_surface_configure(
    struct wlmdock_scene_surface *scene_surface_ptr,
    int width,
    int height)
{
    scene_surface_ptr->configured_width = width;
    scene_surface_ptr->configured_height = height;
}

/* ------------------------------------------------------------------------- */
void wlmdock_scene_surface_commit(
    struct wlmdock_scene_surface *scene_surface_ptr)
{
    if (0 >= scene_surface_ptr->configured_width ||
        0 >= scene_surface_ptr->configured_height) return;

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(
        &state,
        scene_surface_ptr->configured_width != 0 &&
        scene_surface_ptr->configured_height != 0);
    wlr_output_state_set_custom_mode(
        &state,
        scene_surface_ptr->configured_width,
        scene_surface_ptr->configured_height,
        0);
    if (!wlr_output_commit_state(scene_surface_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_WARNING, "Failed wlr_output_commit_state(%p, %p) for %d x %d",
               scene_surface_ptr->wlr_output_ptr,
               &state,
               scene_surface_ptr->configured_width,
               scene_surface_ptr->configured_height);
    }
    wlr_output_state_finish(&state);

    if (!wlr_scene_output_commit(scene_surface_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "Failed wlr_scene_output_commit(%p, NULL)",
               scene_surface_ptr->wlr_scene_output_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmdock_scene_surface_request_size(
    struct wlmdock_scene_surface *scene_surface_ptr,
    int width,
    int height)
{
    if (scene_surface_ptr->requested_width == width &&
        scene_surface_ptr->requested_height == height) return;

    scene_surface_ptr->requested_width = width;
    scene_surface_ptr->requested_height = height;
    if (0 >= width || 0 >= height) {
        // Disable the output, if no more size available.
        if (NULL != scene_surface_ptr->wlr_output_ptr) {
            struct wlr_output_state state;
            wlr_output_state_init(&state);
            wlr_output_state_set_enabled(&state, false);
            if (!wlr_output_commit_state(scene_surface_ptr->wlr_output_ptr,
                                         &state)) {
                bs_log(BS_WARNING, "Failed wlr_output_commit_state(%p, %p)",
                       scene_surface_ptr->wlr_output_ptr,
                       &state);
            }
            wlr_output_state_finish(&state);
            // Unclear: Do we need to also attach a NULL buffer to the surface
            // and commit?
        }
        return;
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Commits the frame. */
void _wlmdock_scene_surface_handle_output_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmdock_scene_surface *scene_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmdock_scene_surface, output_frame_listener);

    if (!wlr_scene_output_commit(
            scene_surface_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "wlr_scene_output_commit(%p, NULL) failed.",
               scene_surface_ptr->wlr_scene_output_ptr);
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(
        scene_surface_ptr->wlr_scene_output_ptr, &now);
}

/* == Unit Tests =========================================================== */

/* == End of scene_surface.c =============================================== */
