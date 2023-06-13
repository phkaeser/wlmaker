/* ========================================================================= */
/**
 * @file tile.h
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
#ifndef __TILE_H__
#define __TILE_H__

#include "cursor.h"
#include "interactive.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a tile interactive.
 *
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the tile.
 * @param cursor_ptr
 * @param tile_callback       Will be called back when the tile is clicked.
 * @param tile_callback_arg   Argument to provide to |tile_callback|
 * @param tile_released_ptr   WLR buffer, texture in nominal state.
 *
 * @return A pointer to the interactive.
 */
wlmaker_interactive_t *wlmaker_tile_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t tile_callback,
    void *tile_callback_arg,
    struct wlr_buffer *tile_released_ptr);

/**
 * Updates the texture for the tile. This will replace and release any earlier
 * texture.
 *
 * @param interactive_ptr
 * @param tile_buffer_ptr     WLR buffer, new texture to use.
 */
void wlmaker_tile_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *tile_buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TILE_H__ */
/* == End of tile.h ======================================================== */
