/* ========================================================================= */
/**
 * @file primitives.h
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
#ifndef __WLMTK_PRIMITIVES_H__
#define __WLMTK_PRIMITIVES_H__

#include <cairo.h>
#include <libbase/libbase.h>

#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Completely fills the cairo with the specified style.
 *
 * @param cairo_ptr           A cairo, backed by an image surface.
 * @param fill_ptr
 */
void wlmaker_primitives_cairo_fill(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr);

/**
 * Fills the cairo with the specified style at the specified rectangle.
 *
 * @param cairo_ptr           A cairo, backed by an image surface.
 * @param x
 * @param y
 * @param width
 * @param height
 * @param fill_ptr
 */
void wlmaker_primitives_cairo_fill_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    const wlmtk_style_fill_t *fill_ptr);

/**
 * Sets the bezel color.
 *
 * Note: Window Maker draws the bezel by adding 80 (0x50) to each R, G, B of
 * the underlying title for the illuminated side; respectively by subtracting
 * 40 (0x28) on the non-illuminated side.
 * We are using cairo's overlaying with the respective "alpha" values below,
 * which leads to different results.
 *
 * @param cairo_ptr
 * @param illuminated
 */
void wlmaker_primitives_set_bezel_color(
    cairo_t *cairo_ptr,
    bool illuminated);

/**
 * Draws a bezel into the cairo.
 *
 * @param cairo_ptr           A cairo, backed by an image surface.
 * @param bezel_width
 * @param raised              Whether the bezel is to highlight a raised (true)
 *                            or pressed (false) state.
 */
void wlmaker_primitives_draw_bezel(
    cairo_t *cairo_ptr,
    double bezel_width,
    bool raised);

/**
 * Draws a bezel into the cairo, at specified position and width/height.
 *
 * @param cairo_ptr           A cairo, backed by an image surface.
 * @param x
 * @param y
 * @param width
 * @param height
 * @param bezel_width
 * @param raised              Whether the bezel is to highlight a raised (true)
 *                            or pressed (false) state.
 */
void wlmaker_primitives_draw_bezel_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    double bezel_width,
    bool raised);

/**
 * Draws the "minimize" icon, as used in the title bar.
 *
 * @param cairo_ptr
 * @param size
 * @param color
 */
void wlmaker_primitives_draw_minimize_icon(
    cairo_t *cairo_ptr,
    int size,
    uint32_t color);

/**
 * Draws the "close" icon, as used in the title bar.
 *
 * @param cairo_ptr
 * @param size
 * @param color
 */
void wlmaker_primitives_draw_close_icon(
    cairo_t *cairo_ptr,
    int size,
    uint32_t color);

/**
 * Draws the window title into the `cairo_t`.
 *
 * @param cairo_ptr
 * @param font_style_ptr      The font style to use.
 * @param title_ptr           Title string, or NULL.
 * @param color               As an ARGB 8888 value.
 */
void wlmaker_primitives_draw_window_title(
    cairo_t *cairo_ptr,
    const wlmtk_style_font_t *font_style_ptr,
    const char *title_ptr,
    uint32_t color);

/**
 * Draws the text with given parameters into the `cairo_t` at (x, y).
 *
 * @param cairo_ptr
 * @param x
 * @param y
 * @param font_style_ptr
 * @param color
 * @param text_ptr
 */
void wlmaker_primitives_draw_text(
    cairo_t *cairo_ptr,
    int x,
    int y,
    const wlmtk_style_font_t *font_style_ptr,
    uint32_t color,
    const char *text_ptr);

/** Unit tests. */
extern const bs_test_case_t   wlmaker_primitives_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_PRIMITIVES_H__ */
/* == End of primitives.h ================================================== */
