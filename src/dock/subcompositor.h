/* ========================================================================= */
/**
 * @file subcompositor.h
 *
 * Interface for a subcompositor based off a wl_surface.
 * TODO(kaeser@gubbe.ch): Give this module a better name. An output? Scene?
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
#ifndef __WLMAKER_DOCK_SUBCOMPOSITOR_H__
#define __WLMAKER_DOCK_SUBCOMPOSITOR_H__

#include <stdbool.h>
#include <toolkit/toolkit.h>

#include "wlclient/layer_surface.h"
#include "wlclient/wlclient.h"

struct wl_display;
struct wlmim_cursor_style;
struct wlr_allocator;
struct wlr_backend;
struct wlr_renderer;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Handle to the subcompositor's state. */
typedef struct _wlmdock_subcompositor_t wlmdock_subcompositor_t;

/**
 * Creates a subcompositor.
 *
 * @param wl_display_ptr
 * @param wlr_backend_ptr     Must be a `wayland` backend, not started (yet).
 * @param client_ptr
 * @param layer_surface_ptr
 * @param cursor_style_ptr
 * @param element_ptr
 *
 * @return Handle of the compositor, or NULL on error.
 */
wlmdock_subcompositor_t *wlmdock_subcompositor_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    wlmcl_client_t *client_ptr,
    wlmcl_layer_surface_t *layer_surface_ptr,
    struct wlmim_cursor_style *cursor_style_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Starts the subcompositor.
 *
 * @param subcompositor_ptr
 * @param wlr_backend_ptr     Must be a `wayland` backend, must be started.
 * @param wlr_allocator_ptr
 * @param wlr_renderer_ptr
 *
 * @return true on success.
 */
bool wlmdock_subcompositor_start(
    wlmdock_subcompositor_t *subcompositor_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr);

/**
 * Destroys the subcompositor.
 *
 * @param subcompositor_ptr
 */
void wlmdock_subcompositor_destroy(wlmdock_subcompositor_t *subcompositor_ptr);

/**
 * Returns the topmost element of the subcompositor's element tree.
 *
 * @param subcompositor_ptr
 *
 * @return Pointer to @ref wlmdock_subcompositor_t::container super element.
 */
wlmtk_element_t *wlmdock_subcompositor_element(
    wlmdock_subcompositor_t *subcompositor_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_DOCK_SUBCOMPOSITOR_H__
/* == End of subcompositor.h ================= */
