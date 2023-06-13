/* ========================================================================= */
/**
 * @file layer_surface.h
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
#ifndef __LAYER_SURFACE_H__
#define __LAYER_SURFACE_H__

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_layer_shell_v1.h>
#undef WLR_USE_UNSTABLE

/** Handler for a layer surface. */
typedef struct _wlmaker_layer_surface_t wlmaker_layer_surface_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a handler for the layer surface.
 *
 * @param wlr_layer_surface_v1_ptr
 * @param server_ptr
 *
 * @return The handler for the layer surface or NULL on error.
 */
wlmaker_layer_surface_t *wlmaker_layer_surface_create(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the handler for the layer surface.
 *
 * @param layer_surface_ptr
 */
void wlmaker_layer_surface_destroy(wlmaker_layer_surface_t *layer_surface_ptr);

/**
 * @param layer_surface_ptr
 *
 * @return Whether this layer surface is exclusive.
 */
bool wlmaker_layer_surface_is_exclusive(
    wlmaker_layer_surface_t *layer_surface_ptr);

/**
 * Configures the layer surface, position its scene node in accordance to its
 * current state, and update the remaining usable area.
 *
 * @param layer_surface_ptr
 * @param full_area_ptr
 * @param usable_area_ptr
 */
void wlmaker_layer_surface_configure(
    wlmaker_layer_surface_t *layer_surface_ptr,
    const struct wlr_box *full_area_ptr,
    struct wlr_box *usable_area_ptr);

/**
 * Accessor: Gets the double-linked-list node from the layer.
 *
 * @param layer_surface_ptr
 *
 * @return Pointer to the |dlnode| element.
 */
bs_dllist_node_t *wlmaker_dlnode_from_layer_surface(
    wlmaker_layer_surface_t *layer_surface_ptr);

/**
 * Type cast: Gets the `wlmaker_layer_surface_t` holding |dlnode_ptr|.
 *
 * @param dlnode_ptr
 *
 * @return Pointer to the `wlmaker_layer_surface_t`.
 */
wlmaker_layer_surface_t *wlmaker_layer_surface_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LAYER_SURFACE_H__ */
/* == End of layer_surface.h =============================================== */
