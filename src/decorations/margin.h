/* ========================================================================= */
/**
 * @file margin.h
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
#ifndef __MARGIN_H__
#define __MARGIN_H__

#include <stdint.h>

#include <wlr/util/edges.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward definition: Handle for margin. */
typedef struct _wlmaker_decorations_margin_t wlmaker_decorations_margin_t;

/** Creates margin. */
wlmaker_decorations_margin_t *wlmaker_decorations_margin_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    int x, int y,
    unsigned width, unsigned height,
    uint32_t edges);

/** Destroys margin. */
void wlmaker_decorations_margin_destroy(
    wlmaker_decorations_margin_t *margin_ptr);

/**
 * Sets the position of the margins.
 *
 * The given (x, y) coordinates are defining the top-left corner of the
 * decorated area, not including the margin itself.
 *
 * @param margin_ptr
 * @param x
 * @param y
 */
void wlmaker_decorations_margin_set_position(
    wlmaker_decorations_margin_t *margin_ptr,
    int x,
    int y);

/**
 * Resizes the margins.
 *
 * `width` and `height` are describing the dimensions of the decorated element,
 * excluding the added dimensions of the margin.
 *
 * @param margin_ptr
 * @param width
 * @param height
 */
void wlmaker_decorations_margin_set_size(
    wlmaker_decorations_margin_t *margin_ptr,
    unsigned width,
    unsigned height);

/**
 * (re)configures which edges to show for the margin.
 *
 * @param margin_ptr
 * @param edges
 */
void wlmaker_decorations_margin_set_edges(
    wlmaker_decorations_margin_t *margin_ptr,
    uint32_t edges);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __MARGIN_H__ */
/* == End of margin.h ====================================================== */
