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

#include "cairo_util.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Hardcoded: Default size of tiles. */
extern const uint32_t         wlmaker_decorations_tile_size;
/** Size of the clip button (length of the catheti) */
extern const uint32_t         wlmaker_decorations_clip_button_size;

/**
 * Creates a cairo image surface for the background of the title bar.
 *
 * @param width               Full with of the title bar. The width depends on
 *                            the window size; the height of the title bar is
 *                            hardcoded.
 * @param fill_ptr            Specification for the fill.
 *
 * @return a `cairo_surface_t` image target, filled as specificed.
 */
cairo_surface_t *wlmaker_decorations_titlebar_create_background(
    uint32_t width, const wlmaker_style_fill_t *fill_ptr);

/**
 * Creates a cairo image surface for the background of the resize bar.
 *
 * @param width               Full with of the resize bar. The width depends on
 *                            the window size; the height of the reizse bar is
 *                            hardcoded.
 * @param fill_ptr            Specification for the fill.
 *
 * @return a `cairo_surface_t` image target, filled as specificed.
 */
cairo_surface_t *wlmaker_decorations_resizebar_create_background(
    uint32_t width, const wlmaker_style_fill_t *fill_ptr);

/**
 * Draws a tile into the `cairo_t`.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param pressed
 */
void wlmaker_decorations_draw_tile(
    cairo_t *cairo_ptr,
    const wlmaker_style_fill_t *fill_ptr,
    bool pressed);

/**
 * Loads an icon and draws it onto the pre-drawn tile at |cairo_ptr|.
 *
 * @param cairo_ptr
 * @param icon_path_ptr
 *
 * @return true if the icon was loaded (and then drawn) successfully.
 */
bool wlmaker_decorations_draw_tile_icon(
    cairo_t *cairo_ptr,
    const char *icon_path_ptr);

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
    const wlmaker_style_fill_t *fill_ptr,
    uint32_t font_color,
    const char *title_ptr);

/**
 * Draws the clip's tile into |cairo_ptr|.
 *
 * This includes the tile with the diagonal bezel edges facing the triangle
 * buttons, but excludes the triangle buttons. Excludes the text.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param pressed
 *
 * @return true, iff the clip was drawn.
 */
bool wlmaker_decorations_draw_clip(
    cairo_t *cairo_ptr,
    const wlmaker_style_fill_t *fill_ptr,
    bool pressed);

/**
 * Draws the north-eastern clip button ("next") onto |cairo_ptr|.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param pressed
 *
 * @return true, iff the button was drawn.
 */
bool wlmaker_decorations_draw_clip_button_next(
    cairo_t *cairo_ptr,
    const wlmaker_style_fill_t *fill_ptr,
    bool pressed);

/**
 * Draws the south-western clip button ("prev") onto |cairo_ptr|.
 *
 * @param cairo_ptr
 * @param fill_ptr
 * @param pressed
 *
 * @returns true, iff the button was drawn.
 */
bool wlmaker_decorations_draw_clip_button_prev(
    cairo_t *cairo_ptr,
    const wlmaker_style_fill_t *fill_ptr,
    bool pressed);

/** Unit tests. */
extern const bs_test_case_t wlmaker_decorations_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __DECORATIONS_H__ */
/* == End of decorations.h ================================================= */
