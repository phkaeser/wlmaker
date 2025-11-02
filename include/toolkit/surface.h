/* ========================================================================= */
/**
 * @file surface.h
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
#ifndef __WLMTK_SURFACE_H__
#define __WLMTK_SURFACE_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <wayland-server-core.h>

struct _wlmtk_surface_t;
/** Forward declaration: State of a toolkit's WLR surface. */
typedef struct _wlmtk_surface_t wlmtk_surface_t;
/** Forward declaration: Virtual method table of the toolkit's WLR surface. */
typedef struct _wlmtk_surface_vmt_t wlmtk_surface_vmt_t;

#include "element.h"

/** Forward declaration. */
struct wlr_seat;
struct wlr_surface;
struct wlr_xdg_surface;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a toolkit surface from the `wlr_surface_ptr`.
 *
 * @param wlr_surface_ptr
 * @param wlr_seat_ptr
 *
 * @return A pointer to the @ref wlmtk_surface_t. Must be destroyed by calling
 *     @ref wlmtk_surface_destroy.
 */
wlmtk_surface_t *wlmtk_surface_create(
    struct wlr_surface *wlr_surface_ptr,
    struct wlr_seat *wlr_seat_ptr);

/**
 * Creates a toolkit surface from the XDG surface `wlr_xdg_surface_ptr`.
 *
 * TODO(kaeser@gubbe.ch): Merge with @ref wlmtk_surface_create.
 *
 * @param wlr_xdg_surface_ptr
 * @param wlr_seat_ptr
 *
 * @return A pointer to the @ref wlmtk_surface_t. Must be destroyed by calling
 *     @ref wlmtk_surface_destroy.
 */
wlmtk_surface_t *wlmtk_xdg_surface_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    struct wlr_seat *wlr_seat_ptr);

/**
 * Destroys the toolkit surface.
 *
 * @param surface_ptr
 */
void wlmtk_surface_destroy(wlmtk_surface_t *surface_ptr);


/**
 * Returns a pointer to the surface's element superclass instance.
 *
 * @param surface_ptr
 *
 * @return Pointer to the corresponding @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_surface_element(wlmtk_surface_t *surface_ptr);

/**
 * Returns committed size of the surface.
 *
 * @param surface_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmtk_surface_get_size(
    wlmtk_surface_t *surface_ptr,
    int *width_ptr,
    int *height_ptr);

/**
 * Activates the surface.
 *
 * @param surface_ptr
 * @param activated
 */
void wlmtk_surface_set_activated(
    wlmtk_surface_t *surface_ptr,
    bool activated);

/** @return Whether the surface is activated. */
bool wlmtk_surface_is_activated(wlmtk_surface_t *surface_ptr);

/** Connects a listener and handler to the `map` signal of `wlr_surface`. */
void wlmtk_surface_connect_map_listener_signal(
    wlmtk_surface_t *surface_ptr,
    struct wl_listener *listener_ptr,
    wl_notify_func_t handler);
/** Connects a listener and handler to the `unmap` signal of `wlr_surface`. */
void wlmtk_surface_connect_unmap_listener_signal(
    wlmtk_surface_t *surface_ptr,
    struct wl_listener *listener_ptr,
    wl_notify_func_t handler);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_surface_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SURFACE_H__ */
/* == End of surface.h ===================================================== */
