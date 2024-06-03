/* ========================================================================= */
/**
 * @file dock.h
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
#ifndef __WLMTK_DOCK_H__
#define __WLMTK_DOCK_H__

#include <wlr/util/edges.h>

/** Forward declaration: Dock handle. */
typedef struct _wlmtk_dock_t wlmtk_dock_t;

#include "env.h"
#include "panel.h"
#include "tile.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Positioning options for the dock. */
typedef struct {
    /** Principal edge the dock aligns to. Must not be WLR_EDGE_NONE. */
    enum wlr_edges            edge;
    /** Dock anchor, along @ref wlmtk_dock_positioning_t::edge. */
    enum wlr_edges            anchor;

    /** Size of the tile. */
    int                       tile_size;
} wlmtk_dock_positioning_t;

/**
 * Creates a dock. A dock contains icons, launchers and the likes.
 *
 * The dock is an implementation of a @ref wlmtk_panel_t.
 *
 * @param dock_positioning_ptr
 * @param env_ptr
 *
 * @return The dock handle, or NULL on error. Must be destroyed calling
 *     @ref wlmtk_dock_destroy.
 */
wlmtk_dock_t *wlmtk_dock_create(
    const wlmtk_dock_positioning_t *dock_positioning_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the dock.
 *
 * @param dock_ptr
 */
void wlmtk_dock_destroy(wlmtk_dock_t *dock_ptr);

/**
 * Adds the tile to the dock.
 *
 * @param dock_ptr
 * @param tile_ptr
 */
void wlmtk_dock_add_tile(
    wlmtk_dock_t *dock_ptr,
    wlmtk_tile_t *tile_ptr);

/**
 * Removes the tile from the dock.
 *
 * @param dock_ptr
 * @param tile_ptr
 */
void wlmtk_dock_remove_tile(
    wlmtk_dock_t *dock_ptr,
    wlmtk_tile_t *tile_ptr);

/** @return Pointer to the superclass @ref wlmtk_panel_t of `dock_ptr`. */
wlmtk_panel_t *wlmtk_dock_panel(wlmtk_dock_t *dock_ptr);

/** @return Pointer to the superclass @ref wlmtk_element_t of `dock_ptr`. */
wlmtk_element_t *wlmtk_dock_element(wlmtk_dock_t *dock_ptr);

/** Unit tests for @ref wlmtk_dock_t. */
extern const bs_test_case_t wlmtk_dock_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_DOCK_H__ */
/* == End of dock.h ======================================================== */
