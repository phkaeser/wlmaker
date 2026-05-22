/* ========================================================================= */
/**
 * @file layer_surface.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright 2026 Google LLC
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
#ifndef __WLMAKER_WLCLIENT_LAYER_SURFACE_H__
#define __WLMAKER_WLCLIENT_LAYER_SURFACE_H__

#include <stdbool.h>
#include <stdint.h>

#include "wlclient.h"  // IWYU pragma: keep
#include "wlr-layer-shell-unstable-v1-client-protocol.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of the layer surface. */
typedef struct _wlmcl_layer_surface_t wlmcl_layer_surface_t;

/**
 * Returns whether the layer shell protocol is supported on the client.
 *
 * @param wlclient_ptr
 */
bool wlmcl_layer_shell_supported(wlmcl_client_t *wlclient_ptr);

/**
 * Creates a layer surface.
 *
 * @param wlclient_ptr
 * @param layer               One of zwlr_layer_shell_v1_layer.
 * @param namespace_ptr       Namespace string.
 * @param anchor              Bitwise OR of zwlr_layer_surface_v1_anchor.
 * @param width               Initial requested width in pixels. 0 to let the
 *                            compositor decide (requires opposite anchors).
 * @param height              Initial requested height in pixels. 0 to let the
 *                            compositor decide (requires opposite anchors).
 *
 * @return State of the layer surface or NULL on error.
 */
wlmcl_layer_surface_t *wlmcl_layer_surface_create(
    wlmcl_client_t *wlclient_ptr,
    uint32_t layer,
    const char *namespace_ptr,
    uint32_t anchor,
    uint32_t width,
    uint32_t height);

/**
 * Destroys the layer surface.
 *
 * @param layer_surface_ptr
 */
void wlmcl_layer_surface_destroy(wlmcl_layer_surface_t *layer_surface_ptr);

/**
 * Requests the specified size for the layer surface.
 *
 * @param layer_surface_ptr
 * @param width               Requested width in pixels. 0 to let the
 *                            compositor decide.
 * @param height              Requested height in pixels. 0 to let the
 *                            compositor decide.
 */
void wlmcl_layer_surface_request_size(
    wlmcl_layer_surface_t *layer_surface_ptr,
    uint32_t width,
    uint32_t height);

/**
 * Sets the margins of the layer surface from its anchor points.
 *
 * @param layer_surface_ptr
 * @param top
 * @param right
 * @param bottom
 * @param left
 */
void wlmcl_layer_surface_set_margin(
    wlmcl_layer_surface_t *layer_surface_ptr,
    int32_t top,
    int32_t right,
    int32_t bottom,
    int32_t left);

/**
 * Sets the exclusive zone of the layer surface.
 *
 * @param layer_surface_ptr
 * @param pixels              The exclusive zone in pixels. Positive value
 *                            reserves space to avoid occlusion by other (e.g.
 *                            maximized) windows. 0 disables exclusive zone.
 *                            -1 requests other windows be allowed to overlap.
 */
void wlmcl_layer_surface_set_exclusive_zone(
    wlmcl_layer_surface_t *layer_surface_ptr,
    int32_t pixels);

/**
 * Returns the underlying Wayland surface of the layer surface.
 *
 * @param layer_surface_ptr
 *
 * @return The wl_surface pointer.
 */
struct wl_surface *wlmcl_layer_surface_wl_surface(
    wlmcl_layer_surface_t *layer_surface_ptr);

/**
 * Registers the callback to notify when the layer surface size/layout is determined or updated.
 *
 * @param layer_surface_ptr
 * @param callback
 * @param ud_ptr
 */
void wlmcl_layer_surface_register_configure_callback(
    wlmcl_layer_surface_t *layer_surface_ptr,
    void (*callback)(void *ud_ptr, uint32_t width, uint32_t height),
    void *ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_LAYER_SURFACE_H__ */
/* == End of layer_surface.h ================================================= */
