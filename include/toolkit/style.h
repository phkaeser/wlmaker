/* ========================================================================= */
/**
 * @file style.h
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
#ifndef __WLMTK_STYLE_H__
#define __WLMTK_STYLE_H__

#include <cairo.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Specifies the type of coloring to use for the fill. */
typedef enum {
    /** Horizontal color gradient. */
    WLMTK_STYLE_COLOR_SOLID,
    /** Horizontal color gradient. */
    WLMTK_STYLE_COLOR_HGRADIENT,
    /** Vertical color gradient. */
    WLMTK_STYLE_COLOR_VGRADIENT,
    /**
     * Diagonal color gradient, Cairo style.
     *
     * Colors are interpolated from top-left to bottom-right corner. Areas of
     * equal color value are arranged perpendicular to that diagonal.
     * This produces a smooth color flow across all rectangle edges.
     */
    WLMTK_STYLE_COLOR_DGRADIENT,
    /**
     * Alternative diagonal color gradient, Window Maker style.
     *
     * Colors are interpolated from top-left to bottom-right corner. Areas of
     * equal color value are aligned with the other diagonal -- from top-right
     * to bottom-left.
     * This may produce a steep gradient along the thin axis of long & thin
     * rectangles, but is similar to what Window Maker uses.
     */
    WLMTK_STYLE_COLOR_ADGRADIENT,
} wlmtk_style_fill_type_t;

/** Specifies the color for a solid fill. */
typedef struct {
    /** Color to start from, as ARGB 8888. Left, for the HGRADIENT type. */
    uint32_t                  color;
} wlmtk_style_color_solid_data_t;

/** Specifies the two colors to span a gradient between. */
typedef struct {
    /** Color to start from, as ARGB 8888. Left, for the HGRADIENT type. */
    uint32_t                  from;
    /** Color to end with, as ARGB 8888. Right, for the HGRADIENT type. */
    uint32_t                  to;
} wlmtk_style_color_gradient_data_t;

/** Specification for the fill of the titlebar. */
typedef struct {
    /** The type of fill to apply. */
    wlmtk_style_fill_type_t type;
    /** Parameters for the fill. */
    union {
        /** Solid color. */
        wlmtk_style_color_solid_data_t solid;
        /** Horizontal color gradient. */
        wlmtk_style_color_gradient_data_t hgradient;
        /** Vertical color gradient. */
        wlmtk_style_color_gradient_data_t vgradient;
        /** Diagonal color gradient. */
        wlmtk_style_color_gradient_data_t dgradient;
        /** Alternative diagonal color gradient. */
        wlmtk_style_color_gradient_data_t adgradient;
    } param;
} wlmtk_style_fill_t;

/** Hardcoded max length of the font face name. */
#define WLMTK_STYLE_FONT_FACE_LENGTH 128

/** Weight of a font. */
typedef enum {
    WLMTK_FONT_WEIGHT_NORMAL,
    WLMTK_FONT_WEIGHT_BOLD,
} wlmtk_style_font_weight_t;

/** Style of font. */
typedef struct {
    /** Font face family name. */
    char                      face[WLMTK_STYLE_FONT_FACE_LENGTH];
    /** Weight. */
    wlmtk_style_font_weight_t weight;
    /** Size of the font, in pixels per "em" square side. */
    uint64_t                  size;
} wlmtk_style_font_t;


/** Specifies color and width of a margin. */
typedef struct {
    /** Width of the margin. */
    uint64_t                  width;
    /** Color of the margin. */
    uint32_t                  color;
} wlmtk_margin_style_t;

/** Styling information for the dock. */
typedef struct {
    /** The margin's style of the dock's box. */
    wlmtk_margin_style_t      margin;
} wlmtk_dock_style_t;

/** Menu item style. */
typedef struct {
    /** Fill style. */
    wlmtk_style_fill_t        fill;
    /** Fill style when disabled. */
    wlmtk_style_fill_t        highlighted_fill;
    /** Style of the font used in the menu item. */
    wlmtk_style_font_t        font;
    /** Height of the menu item, in pixels. */
    uint64_t                  height;
    /** Width of the bezel, in pixels. */
    uint64_t                  bezel_width;
    /** Text color. */
    uint32_t                  enabled_text_color;
    /** Text color when highlighted. */
    uint32_t                  highlighted_text_color;
    /** Text color when disabled. */
    uint32_t                  disabled_text_color;
    /** Width of the item. */
    uint64_t                  width;
} wlmtk_menu_item_style_t;

/** Style options for the resizebar. */
typedef struct {
    /** Fill style for the complete resizebar. */
    wlmtk_style_fill_t        fill;
    /** Height of the resize bar. */
    uint64_t                  height;
    /** Width of the corners. */
    uint64_t                  corner_width;
    /** Width of the bezel. */
    uint64_t                  bezel_width;
} wlmtk_resizebar_style_t;

/** Style options for the titlebar. */
typedef struct {
    /** Fill style for when the titlebar is focussed (activated). */
    wlmtk_style_fill_t        focussed_fill;
    /** Fill style for when the titlebar is blurred (not activated). */
    wlmtk_style_fill_t        blurred_fill;
    /** Color of the title text when focussed. */
    uint32_t                  focussed_text_color;
    /** Color of the title text when blurred. */
    uint32_t                  blurred_text_color;
    /** Height of the title bar, in pixels. */
    uint64_t                  height;
    /** Width of the bezel. */
    uint64_t                  bezel_width;
    /** Style of the margin within the resizebar. */
    wlmtk_margin_style_t      margin;
    /** Font style for the titlebar's title. */
    wlmtk_style_font_t       font;
} wlmtk_titlebar_style_t;

/** Style options for the window. */
typedef struct {
    /** The titlebar's style. */
    wlmtk_titlebar_style_t    titlebar;
    /** The resizebar's style. */
    wlmtk_resizebar_style_t    resizebar;
    /** Style of the window border. */
    wlmtk_margin_style_t       border;
    /** Style of the margins between titlebar, window and resizebar. */
    wlmtk_margin_style_t       margin;
} wlmtk_window_style_t;

/** Translates the font weight from toolkit into cairo enum. */
cairo_font_weight_t wlmtk_style_font_weight_cairo_from_wlmtk(
    wlmtk_style_font_weight_t weight);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_STYLE_H__ */
/* == End of style.h ================================================== */
