/* ========================================================================= */
/**
 * @file tile.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __WLMTK_TILE_H__
#define __WLMTK_TILE_H__

/** Forward declaration: State of a tile. */
typedef struct _wlmtk_tile_t wlmtk_tile_t;

#include "buffer.h"
#include "container.h"
#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Style options for the tile. */
typedef struct {
    /** Fill style for the tile's background. */
    wlmtk_style_fill_t        fill;
    /** Size of the tile, in pixels. Tiles are of quadratic shape. */
    int                       size;
    /** Width of the bezel. */
    int                       bezel_width;
} wlmtk_tile_style_t;

/** State of a tile. */
struct _wlmtk_tile_t {
    /** A tile is a container. Holds a background and contents. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** The tile background is modelled as @ref wlmtk_buffer_t. */
    wlmtk_buffer_t            buffer;

    /** Style to be used for this tile. */
    wlmtk_tile_style_t        style;

    /** Holds the tile's background, used in @ref wlmtk_tile_t::buffer. */
    struct wlr_buffer         *background_wlr_buffer_ptr;
};

/**
 * Initializes the tile.
 *
 * @param tile_ptr
 * @param style_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_tile_init(
    wlmtk_tile_t *tile_ptr,
    const wlmtk_tile_style_t *style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the tile.
 *
 * @param tile_ptr
 */
void wlmtk_tile_fini(wlmtk_tile_t *tile_ptr);

/** @return the superclass' @ref wlmtk_element_t of `tile_ptr`. */
wlmtk_element_t *wlmtk_tile_element(wlmtk_tile_t *tile_ptr);

/** Unit test cases for @ref wlmtk_tile_t. */
extern const bs_test_case_t wlmtk_tile_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TILE_H__ */
/* == End of tile.h ======================================================== */
