/* ========================================================================= */
/**
 * @file decorations.c
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

#include "decorations.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <libbase/libbase.h>

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Hardcoded: Default size of tiles. */
const uint32_t                wlmaker_decorations_tile_size = 64;
/** Hardcoded: Margin of the tile, defining the width of the bezel. */
const uint32_t                wlmaker_decorations_tile_margin = 2;
/** Size of the clip button (length of the catheti) */
const uint32_t                wlmaker_decorations_clip_button_size = 22;

static cairo_surface_t *create_background(
    unsigned width,
    unsigned height,
    const wlmtk_style_fill_t *fill_ptr);

/** Lookup paths for icons. */
static const char *lookup_paths[] = {
    "/usr/share/icons/wlmaker",
    "/usr/local/share/icons/wlmaker",
#if defined(WLMAKER_SOURCE_DIR)
    WLMAKER_SOURCE_DIR "/icons",
#endif  // WLMAKER_SOURCE_DIR
#if defined(WLMAKER_ICON_DATA_DIR)
    WLMAKER_ICON_DATA_DIR,
#endif  // WLMAKER_ICON_DATA_DIR
    NULL
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_draw_tile(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    bool pressed)
{
    wlmaker_primitives_cairo_fill(cairo_ptr, fill_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_tile_margin, !pressed);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_decorations_draw_tile_icon(
    cairo_t *cairo_ptr,
    const char *icon_path_ptr)
{
    // Just double-check that the cairo is of appropriate size...
    BS_ASSERT((int)wlmaker_decorations_tile_size ==
              cairo_image_surface_get_width(cairo_get_target(cairo_ptr)));
    BS_ASSERT((int)wlmaker_decorations_tile_size ==
              cairo_image_surface_get_height(cairo_get_target(cairo_ptr)));

    char full_path[PATH_MAX];
    char *path_ptr = bs_file_resolve_and_lookup_from_paths(
        icon_path_ptr, lookup_paths, 0, full_path);
    if (NULL == path_ptr) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed bs_file_resolve_and_lookup_from_paths(%s, ...).",
               icon_path_ptr);
        return false;
    }

    cairo_surface_t *icon_surface_ptr = cairo_image_surface_create_from_png(
        path_ptr);
    if (NULL == icon_surface_ptr) {
        bs_log(BS_ERROR, "Failed cairo_image_surface_create_from_png(%s).",
               path_ptr);
        return false;
    }

    // Find top-left, and cap the icon to max the tile size.
    int width = BS_MIN(
        (int)wlmaker_decorations_tile_size,
        cairo_image_surface_get_width(icon_surface_ptr));
    int height = BS_MIN(
        (int)wlmaker_decorations_tile_size,
        cairo_image_surface_get_height(icon_surface_ptr));
    int x = (wlmaker_decorations_tile_size - width) / 2;
    int y = (wlmaker_decorations_tile_size - height) / 2;

    cairo_save(cairo_ptr);
    cairo_set_source_surface(cairo_ptr, icon_surface_ptr, x, y);
    cairo_rectangle(cairo_ptr, x, y, width, height);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);
    cairo_restore(cairo_ptr);

    cairo_surface_destroy(icon_surface_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_draw_iconified(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    uint32_t font_color,
    const char *title_ptr)
{
    uint32_t width = cairo_image_surface_get_width(
        cairo_get_target(cairo_ptr));
    uint32_t height = BS_MIN(
        16, cairo_image_surface_get_height(cairo_get_target(cairo_ptr)));

    cairo_surface_t *background_surface_ptr = create_background(
        width, height, fill_ptr);
    cairo_set_source_surface(cairo_ptr, background_surface_ptr, 0, 0);
    cairo_rectangle(cairo_ptr, 0, 0, width, height);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);
    cairo_surface_destroy(background_surface_ptr);

    cairo_save(cairo_ptr);
    cairo_select_font_face(cairo_ptr, "Helvetica",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo_ptr, 10.0);
    cairo_set_source_argb8888(cairo_ptr, font_color);
    cairo_move_to(cairo_ptr, 4, 12);
    cairo_show_text(cairo_ptr, title_ptr);

    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_decorations_draw_clip(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    bool pressed)
{
    // For readability.
    double tsize = wlmaker_decorations_tile_size;
    double bsize = wlmaker_decorations_clip_button_size;
    double margin = wlmaker_decorations_tile_margin;

    // Create tile background, but draw only the core parts.
    cairo_surface_t *background_surface_ptr = create_background(
        wlmaker_decorations_tile_size,
        wlmaker_decorations_tile_size,
        fill_ptr);
    if (NULL == background_surface_ptr) {
        bs_log(BS_ERROR, "Failed create_background().");
        return false;
    }
    cairo_set_source_surface(cairo_ptr, background_surface_ptr, 0, 0);
    cairo_surface_destroy(background_surface_ptr);

    cairo_save(cairo_ptr);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize, tsize);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);
    cairo_restore(cairo_ptr);

    // Load icon into the very center...
    if (!wlmaker_decorations_draw_tile_icon(cairo_ptr, "clip-48x48.png")) {
        return false;
    }

    // Northwestern corner, illuminated when raised. Clock-wise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !pressed);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize - bsize, margin);
    cairo_line_to(cairo_ptr, margin, margin);
    cairo_line_to(cairo_ptr, margin, tsize - bsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);

    // Southeastern corner, illuminted when pressed. Also clock-wise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, pressed);
    cairo_move_to(cairo_ptr, tsize, tsize);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, bsize, tsize - margin);
    cairo_line_to(cairo_ptr, tsize - margin, tsize - margin);
    cairo_line_to(cairo_ptr, tsize - margin, bsize);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize, tsize);
    cairo_fill(cairo_ptr);

    // Diagonal at the north-eastern corner. Drawn clockwise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !pressed);
    cairo_move_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize - margin, bsize);
    cairo_line_to(cairo_ptr, tsize - bsize, margin);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_fill(cairo_ptr);

    // Diagonal at south-western corner. Drawn clockwise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, pressed);
    cairo_move_to(cairo_ptr, 0, tsize - bsize);
    cairo_line_to(cairo_ptr, margin, tsize - bsize);
    cairo_line_to(cairo_ptr, bsize, tsize - margin);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize);
    cairo_fill(cairo_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmaker_decorations_draw_clip_button_next(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    bool pressed)
{
    BS_ASSERT((int)wlmaker_decorations_clip_button_size ==
              cairo_image_surface_get_width(cairo_get_target(cairo_ptr)));
    BS_ASSERT((int)wlmaker_decorations_clip_button_size ==
              cairo_image_surface_get_height(cairo_get_target(cairo_ptr)));

    // For readability.
    double tile_size = wlmaker_decorations_tile_size;
    double bsize = wlmaker_decorations_clip_button_size;
    double margin = wlmaker_decorations_tile_margin;

    // Create tile background, but draw only the core parts.
    cairo_surface_t *background_surface_ptr =  create_background(
        wlmaker_decorations_tile_size,
        wlmaker_decorations_tile_size,
        fill_ptr);
    if (NULL == background_surface_ptr) {
        bs_log(BS_ERROR, "Failed create_background().");
        return false;
    }
    cairo_save(cairo_ptr);
    cairo_set_source_surface(
        cairo_ptr, background_surface_ptr, - tile_size  + bsize, 0);
    cairo_surface_destroy(background_surface_ptr);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, bsize, 0);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);
    cairo_restore(cairo_ptr);

    // Northern edge, illuminated when raised
    wlmaker_primitives_set_bezel_color(cairo_ptr, !pressed);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, bsize, 0);
    cairo_line_to(cairo_ptr, bsize - margin, margin);
    cairo_line_to(cairo_ptr, 2 * margin, margin);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);

    // Eastern edge, illuminated when pressed
    wlmaker_primitives_set_bezel_color(cairo_ptr, pressed);
    cairo_move_to(cairo_ptr, bsize, 0);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, bsize - margin, bsize - 2 * margin);
    cairo_line_to(cairo_ptr, bsize - margin,  margin);
    cairo_line_to(cairo_ptr, bsize, 0);
    cairo_fill(cairo_ptr);

    // Diagonal, illuminated when pressed.
    wlmaker_primitives_set_bezel_color(cairo_ptr, pressed);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, 2 * margin, margin);
    cairo_line_to(cairo_ptr, bsize - margin, bsize - 2 *margin);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);

    // The black triangle. Use relative sizes.
    double tpad = bsize * 5.0 / 22.0;
    double tsize = bsize * 7.0 / 22.0;
    double tmargin = bsize * 1.0 / 22.0;
    cairo_set_source_rgba(cairo_ptr, 0, 0, 0, 1.0);
    cairo_move_to(cairo_ptr, bsize - tpad, tpad);
    cairo_line_to(cairo_ptr, bsize - tpad, tsize + tpad);
    cairo_line_to(cairo_ptr, bsize - tpad - tsize, tpad);
    cairo_line_to(cairo_ptr, bsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    // Northern edge of triangle, not illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, bsize - tpad, tpad);
    cairo_line_to(cairo_ptr, bsize - tpad - tsize, tpad);
    cairo_line_to(cairo_ptr, bsize - tpad - tsize - tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, bsize - tpad + tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, bsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    // Eastern side of triangle, illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, bsize - tpad, tpad);
    cairo_line_to(cairo_ptr, bsize - tpad + tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, bsize - tpad + tmargin, tpad + tsize + tmargin);
    cairo_line_to(cairo_ptr, bsize - tpad, tpad + tsize);
    cairo_line_to(cairo_ptr, bsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmaker_decorations_draw_clip_button_prev(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr,
    bool pressed)
{
    BS_ASSERT((int)wlmaker_decorations_clip_button_size ==
              cairo_image_surface_get_width(cairo_get_target(cairo_ptr)));
    BS_ASSERT((int)wlmaker_decorations_clip_button_size ==
              cairo_image_surface_get_height(cairo_get_target(cairo_ptr)));

    // For readability.
    double tile_size = wlmaker_decorations_tile_size;
    double bsize = wlmaker_decorations_clip_button_size;
    double margin = wlmaker_decorations_tile_margin;

    // Create tile background, but draw only the core parts.
    cairo_surface_t *background_surface_ptr =  create_background(
        wlmaker_decorations_tile_size,
        wlmaker_decorations_tile_size,
        fill_ptr);
    if (NULL == background_surface_ptr) {
        bs_log(BS_ERROR, "Failed create_background().");
        return false;
    }
    cairo_save(cairo_ptr);
    cairo_set_source_surface(
        cairo_ptr, background_surface_ptr, 0, - tile_size  + bsize);
    cairo_surface_destroy(background_surface_ptr);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, 0, bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);
    cairo_restore(cairo_ptr);

    // Southern edge, illuminated when pressed.
    wlmaker_primitives_set_bezel_color(cairo_ptr, pressed);
    cairo_move_to(cairo_ptr, 0, bsize);
    cairo_line_to(cairo_ptr, margin, bsize - margin);
    cairo_line_to(cairo_ptr, bsize - 2 * margin, bsize - margin);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, 0, bsize);
    cairo_fill(cairo_ptr);

    // Western edge, illuminated when raised.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !pressed);
    cairo_move_to(cairo_ptr, 0, bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, margin, 2 * margin);
    cairo_line_to(cairo_ptr, margin, bsize - margin);
    cairo_line_to(cairo_ptr, 0, bsize);
    cairo_fill(cairo_ptr);

    // Diagonal, illuminated when raised.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !pressed);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, bsize, bsize);
    cairo_line_to(cairo_ptr, bsize - 2 * margin, bsize - margin);
    cairo_line_to(cairo_ptr, margin, 2 * margin);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);

    // The black triangle. Use relative sizes.
    double tpad = bsize * 5.0 / 22.0;
    double tsize = bsize * 7.0 / 22.0;
    double tmargin = bsize * 1.0 / 22.0;
    cairo_set_source_rgba(cairo_ptr, 0, 0, 0, 1.0);
    cairo_move_to(cairo_ptr, tpad, bsize - tpad);
    cairo_line_to(cairo_ptr, tpad, bsize - tsize - tpad);
    cairo_line_to(cairo_ptr, tpad + tsize, bsize - tpad);
    cairo_line_to(cairo_ptr, tpad, bsize - tpad);
    cairo_fill(cairo_ptr);

    // Southern edge of triangle, illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, tpad, bsize - tpad);
    cairo_line_to(cairo_ptr, tpad + tsize, bsize - tpad);
    cairo_line_to(cairo_ptr, tpad + tsize + tmargin, bsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad - tmargin, bsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad, bsize - tpad);
    cairo_fill(cairo_ptr);

    // Eastern side of triangle, not illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, tpad, bsize - tpad);
    cairo_line_to(cairo_ptr, tpad - tmargin, bsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad - tmargin, bsize - tpad - tsize - tmargin);
    cairo_line_to(cairo_ptr, tpad, bsize - tpad - tsize);
    cairo_line_to(cairo_ptr, tpad, bsize - tpad);
    cairo_fill(cairo_ptr);

    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates background cairo surface with given |width| x |height| and |fill|.
 *
 * @param width
 * @param height
 * @param fill_ptr
 *
 * @return Cairo surface.
 */
static cairo_surface_t *create_background(
    unsigned width,
    unsigned height,
    const wlmtk_style_fill_t *fill_ptr)
{
    cairo_surface_t *surface_ptr = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    if (NULL == surface_ptr) {
        bs_log(BS_ERROR, "Failed cairo_image_surface_create("
               "CAIRO_FORMAT_ARGB32, %u, %u)", width, height);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create(surface_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create(%p)", cairo_ptr);
        cairo_surface_destroy(surface_ptr);
        return NULL;
    }

    wlmaker_primitives_cairo_fill_at(cairo_ptr, 0, 0, width, height, fill_ptr);

    cairo_destroy(cairo_ptr);
    return surface_ptr;
}

/* == Unit tests =========================================================== */

static void test_tile(bs_test_t *test_ptr);
static void test_iconified(bs_test_t *test_ptr);
static void test_clip(bs_test_t *test_ptr);
static void test_clip_button_next(bs_test_t *test_ptr);
static void test_clip_button_prev(bs_test_t *test_ptr);

const bs_test_case_t          wlmaker_decorations_test_cases[] = {
    { 1, "tile", test_tile },
    { 1, "iconified", test_iconified },
    { 1, "clip", test_clip },
    { 1, "clip_button_next", test_clip_button_next },
    { 1, "clip_button_prev", test_clip_button_prev },
    { 0, NULL, NULL }
};

/** Verifies the title text is drawn as expected. */
void test_tile(bs_test_t *test_ptr) {
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(64, 64);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(64, 64)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    wlmtk_style_fill_t fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    };
    wlmaker_decorations_draw_tile(cairo_ptr, &fill, false);
    cairo_destroy(cairo_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "decorations_tile.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the title text is drawn as expected. */
void test_iconified(bs_test_t *test_ptr) {
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(64, 64);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(64, 64)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    wlmtk_style_fill_t fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xff808080 }}
    };
    wlmaker_decorations_draw_iconified(cairo_ptr, &fill, 0xffffffff, "Title");
    cairo_destroy(cairo_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "decorations_iconified.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the clip tile (excluding the buttons) is drawn as expected. */
void test_clip(bs_test_t *test_ptr) {
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(64, 64);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(64, 64)");
        return;
    }
   cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    wlmtk_style_fill_t fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    };
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    bool drawn = wlmaker_decorations_draw_clip(cairo_ptr, &fill, false);
    BS_TEST_VERIFY_TRUE(test_ptr, drawn);
    cairo_destroy(cairo_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "decorations_clip.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the clip's "next" button is drawn as expected. */
void test_clip_button_next(bs_test_t *test_ptr) {
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr,
                     "Failed bs_gfxbuf_create(%"PRIu32", %"PRIu32")",
                     wlmaker_decorations_clip_button_size,
                     wlmaker_decorations_clip_button_size);
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    wlmtk_style_fill_t fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    };
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    bool drawn = wlmaker_decorations_draw_clip_button_next(
        cairo_ptr, &fill, false);
    BS_TEST_VERIFY_TRUE(test_ptr, drawn);
    cairo_destroy(cairo_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "decorations_clip_button_next.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);

}

/** Verifies the clip's "prev" button is drawn as expected. */
void test_clip_button_prev(bs_test_t *test_ptr) {
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr,
                     "Failed bs_gfxbuf_create(%"PRIu32", %"PRIu32")",
                     wlmaker_decorations_clip_button_size,
                     wlmaker_decorations_clip_button_size);
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    wlmtk_style_fill_t fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    };
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    bool drawn = wlmaker_decorations_draw_clip_button_prev(
        cairo_ptr, &fill, false);
    BS_TEST_VERIFY_TRUE(test_ptr, drawn);
    cairo_destroy(cairo_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "decorations_clip_button_prev.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/* == End of decorations.c ================================================= */
