/* ========================================================================= */
/**
 * @file tile_container.h
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
 *
 * Interface for a @ref wlmaker_tile_container_t.
 */
#ifndef __TILE_CONTAINER_H__
#define __TILE_CONTAINER_H__

/** Forward declaration of the tile container state. */
typedef struct _wlmaker_tile_container_t wlmaker_tile_container_t;

#include "iconified.h"
#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a tile container.
 *
 * Tile containers contain... tiles.
 *
 * @param server_ptr
 * @param workspace_ptr
 */
wlmaker_tile_container_t *wlmaker_tile_container_create(
    wlmaker_server_t *server_ptr,
    wlmaker_workspace_t *workspace_ptr);

/**
 * Destroys the tile container.
 *
 * @param tile_container_ptr
 */
void wlmaker_tile_container_destroy(
    wlmaker_tile_container_t *tile_container_ptr);

/**
 * Adds the `iconified_ptr` to the tile container.
 *
 * @param tile_container_ptr
 * @param iconified_ptr
 */
void wlmaker_tile_container_add(
    wlmaker_tile_container_t *tile_container_ptr,
    wlmaker_iconified_t *iconified_ptr);

/**
 * Adds the `iconified_ptr` to the tile container.
 *
 * @param tile_container_ptr
 * @param iconified_ptr
 */
void wlmaker_tile_container_remove(
    wlmaker_tile_container_t *tile_container_ptr,
    wlmaker_iconified_t *iconified_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TILE_CONTAINER_H__ */
/* == End of tile_container.h ============================================== */
