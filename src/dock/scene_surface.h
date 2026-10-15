/* ========================================================================= */
/**
 * @file scene_surface.h
 *
 * Functionality to bind a `struct wlr_scene` onto a `wl_surface`, for
 * compositor rendering.
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
#ifndef __WLMAKER_DOCK_SCENE_SURFACE_H__
#define __WLMAKER_DOCK_SCENE_SURFACE_H__

#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
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
struct wlr_output_layout;
struct wlr_renderer;
struct wlr_scene;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmdock_scene_surface;

/**
 * Creates a scene surface.
 *
 * @param wlr_output_layout_ptr
 * @param wlr_backend_ptr
 * @param wlr_allocator_ptr
 * @param wlr_renderer_ptr
 * @param wlr_scene_ptr
 * @param wl_surface_ptr
 *
 * @return The scene surface, or NULL on error.
 */
struct wlmdock_scene_surface *wlmdock_scene_surface_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wl_surface *wl_surface_ptr);

/**
 * Destroys the scene surface.
 *
 * @param scene_surface_ptr
 */
void wlmdock_scene_surface_destroy(
    struct wlmdock_scene_surface *scene_surface_ptr);

/** Configures the scene surface's size. */
void wlmdock_scene_surface_configure(
    struct wlmdock_scene_surface *scene_surface_ptr,
    int width,
    int height);
/** Commits the scene surface. */
void wlmdock_scene_surface_commit(
    struct wlmdock_scene_surface *scene_surface_ptr);
/** Sets the requested size. */
void wlmdock_scene_surface_request_size(
    struct wlmdock_scene_surface *scene_surface_ptr,
    int width,
    int height);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_DOCK_SCENE_SURFACE_H__
/* == End of scene_surface.h ================= */
