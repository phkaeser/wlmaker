/* ========================================================================= */
/**
 * @file tilebox.h
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
#ifndef __WLMAKER_DOCK_TILEBOX_H__
#define __WLMAKER_DOCK_TILEBOX_H__

#include <toolkit/toolkit.h>

struct wlr_scene_tree;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the tile box. */
typedef struct _wlmdock_tilebox wlmdock_tilebox_t;

/**
 * Creates a tile box.
 *
 * @param wlr_scene_tree_ptr
 * @param orientation
 * @param style_ptr
 *
 * @return Handle to the tile box' state, or NULL on error.
 */
wlmdock_tilebox_t *wlmdock_tilebox_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmtk_box_orientation_t orientation,
    const struct wlmtk_dock_style *style_ptr);

/**
 * Destroys the tile box.
 *
 * @param tilebox_ptr
 */
void wlmdock_tilebox_destroy(wlmdock_tilebox_t *tilebox_ptr);

/**
 * Returns the tilebox' root @ref wlmtk_element_t.
 *
 * @param tilebox_ptr
 *
 * @return Pointer to the super element of @ref wlmdock_tilebox_t::container.
 */
wlmtk_element_t *wlmdock_tilebox_element(wlmdock_tilebox_t *tilebox_ptr);

/**
 * Adds the tile to the tile box.
 *
 * @param tilebox_ptr
 * @param tile_ptr
 */
void wlmdock_tilebox_add_tile(
    wlmdock_tilebox_t *tilebox_ptr,
    wlmtk_tile_t *tile_ptr);

/**
 * Removes the tile from the tilebox.
 *
 * @param tilebox_ptr
 * @param tile_ptr            Tile to remove. Must have been added to the tile
 *                            box before.
 */
void wlmdock_tilebox_remove_tile(
    wlmdock_tilebox_t *tilebox_ptr,
    wlmtk_tile_t *tile_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_DOCK_TILEBOX_H__
/* == End of tilebox.h ===================================================== */
