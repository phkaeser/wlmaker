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
 *
 * @return State of the layer surface or NULL on error.
 */
wlmcl_layer_surface_t *wlmcl_layer_surface_create(
    wlmcl_client_t *wlclient_ptr,
    uint32_t layer,
    const char *namespace_ptr);

/**
 * Destroys the layer surface.
 *
 * @param layer_surface_ptr
 */
void wlmcl_layer_surface_destroy(wlmcl_layer_surface_t *layer_surface_ptr);

/** @return @ref wlmcl_layer_surface_t::wl_surface_ptr. */
struct wl_surface *wlmcl_layer_surface_wl_surface(
    wlmcl_layer_surface_t *layer_surface_ptr);

/** @return @ref wlmcl_layer_surface_t::layer_surface_ptr. */
struct zwlr_layer_surface_v1 *wlmcl_layer_surface_wlr_layer_surface(
    wlmcl_layer_surface_t *layer_surface_ptr);

/**
 * Registers the callback to notify when the layer surface size/layout is
 * determined or updated.
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
