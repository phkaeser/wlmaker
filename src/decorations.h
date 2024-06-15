/* ========================================================================= */
/**
 * @file decorations.h
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
 * Provides methods for drawing decorations on a cairo drawable.
 */
#ifndef __DECORATIONS_H__
#define __DECORATIONS_H__

#include <cairo.h>

#include <stdint.h>
#include <libbase/libbase.h>

#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Hardcoded: Default size of tiles. */
extern const uint32_t         wlmaker_decorations_tile_size;
/** Size of the clip button (length of the catheti) */
extern const uint32_t         wlmaker_decorations_clip_button_size;

/**
 * Draws a tile into the `cairo_t`.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param pressed
 */
void wlmaker_decorations_draw_tile(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    bool pressed);

/**
 * Draws the title of an iconified on to `cairo_ptr`.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param font_color
 * @param title_ptr
 */
void wlmaker_decorations_draw_iconified(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    uint32_t font_color,
    const char *title_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmaker_decorations_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __DECORATIONS_H__ */
/* == End of decorations.h ================================================= */
