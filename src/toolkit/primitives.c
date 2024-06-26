/* ========================================================================= */
/**
 * @file primitives.c
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

#include "primitives.h"

#include <libbase/libbase.h>

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_cairo_fill(
    cairo_t *cairo_ptr,
    const wlmtk_style_fill_t *fill_ptr)
{
    cairo_surface_t *surface_ptr = cairo_get_target(cairo_ptr);
    uint32_t width = cairo_image_surface_get_width(surface_ptr);
    uint32_t height = cairo_image_surface_get_height(surface_ptr);
    wlmaker_primitives_cairo_fill_at(cairo_ptr, 0, 0, width, height, fill_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_cairo_fill_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    const wlmtk_style_fill_t *fill_ptr)
{
    cairo_pattern_t *cairo_pattern_ptr;
    float r, g, b, alpha;
    switch (fill_ptr->type) {
    case WLMTK_STYLE_COLOR_SOLID:
        bs_gfxbuf_argb8888_to_floats(
            fill_ptr->param.solid.color, &r, &g, &b, &alpha);
        cairo_pattern_ptr = cairo_pattern_create_rgba(r, g, b, alpha);
        break;

    case WLMTK_STYLE_COLOR_HGRADIENT:
        cairo_pattern_ptr = cairo_pattern_create_linear(0, 0, width, 0);
        bs_gfxbuf_argb8888_to_floats(
            fill_ptr->param.hgradient.from, &r, &g, &b, &alpha);
        cairo_pattern_add_color_stop_rgba(
            cairo_pattern_ptr, 0, r, g, b, alpha);
        bs_gfxbuf_argb8888_to_floats(
            fill_ptr->param.hgradient.to, &r, &g, &b, &alpha);
        cairo_pattern_add_color_stop_rgba(
            cairo_pattern_ptr, 1, r, g, b, alpha);
        break;

    case WLMTK_STYLE_COLOR_DGRADIENT:
        cairo_pattern_ptr = cairo_pattern_create_linear(0, 0, width, height);
        bs_gfxbuf_argb8888_to_floats(
            fill_ptr->param.dgradient.from, &r, &g, &b, &alpha);
        cairo_pattern_add_color_stop_rgba(
            cairo_pattern_ptr, 0, r, g, b, alpha);
        bs_gfxbuf_argb8888_to_floats(
            fill_ptr->param.dgradient.to, &r, &g, &b, &alpha);
        cairo_pattern_add_color_stop_rgba(
            cairo_pattern_ptr, 1, r, g, b, alpha);
        break;

    default:
        bs_log(BS_FATAL, "Unsupported fill_type %d", fill_ptr->type);
        BS_ABORT();
        return;
    }

    cairo_save(cairo_ptr);
    cairo_set_source(cairo_ptr, cairo_pattern_ptr);
    cairo_pattern_destroy(cairo_pattern_ptr);
    cairo_rectangle(cairo_ptr, x, y, width, height);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);
    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_set_bezel_color(
    cairo_t *cairo_ptr,
    bool illuminated)
{
    if (illuminated) {
        cairo_set_source_rgba(cairo_ptr, 1.0, 1.0, 1.0, 0.6);
    } else {
        cairo_set_source_rgba(cairo_ptr, 0.0, 0.0, 0.0, 0.4);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_draw_bezel(
    cairo_t *cairo_ptr,
    double bezel_width,
    bool raised)
{
    int width = cairo_image_surface_get_width(cairo_get_target(cairo_ptr));
    int height = cairo_image_surface_get_height(cairo_get_target(cairo_ptr));
    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, height, bezel_width, raised);
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_draw_bezel_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    double bezel_width,
    bool raised)
{
    cairo_save(cairo_ptr);
    cairo_set_line_width(cairo_ptr, 0);

    // Northwestern corner is illuminted when raised.
    wlmaker_primitives_set_bezel_color(cairo_ptr, raised);
    cairo_move_to(cairo_ptr, x, y);
    cairo_line_to(cairo_ptr, x + width, y + 0);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + 0, y + height);
    cairo_line_to(cairo_ptr, x + 0, y + 0);
    cairo_fill(cairo_ptr);

    // Southeastern corner is illuminated when sunken.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !raised);
    cairo_move_to(cairo_ptr, x + width, y + height);
    cairo_line_to(cairo_ptr, x + 0, y + height);
    cairo_line_to(cairo_ptr, x + bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + width, y + 0);
    cairo_line_to(cairo_ptr, x + width, y + height);
    cairo_fill(cairo_ptr);

    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_draw_minimize_icon(
    cairo_t *cairo_ptr,
    int size,
    uint32_t color)
{
    double scale = size / 22.0;
    cairo_save(cairo_ptr);
    cairo_set_line_width(cairo_ptr, 0);
    cairo_set_source_argb8888(cairo_ptr, color);

    cairo_set_fill_rule(cairo_ptr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_rectangle(cairo_ptr, 6 * scale, 6 * scale, 10 * scale, 10 * scale);
    cairo_rectangle(cairo_ptr,
                    (6+ 1) * scale, (6 + 3) * scale, 8 * scale, 6 * scale);
    cairo_fill(cairo_ptr);

    cairo_stroke(cairo_ptr);
    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_draw_close_icon(
    cairo_t *cairo_ptr,
    int size,
    uint32_t color)
{
    double scale = size / 22.0;
    cairo_save(cairo_ptr);

    cairo_set_line_width(cairo_ptr, 2.5 * scale);
    cairo_set_line_cap(cairo_ptr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source_argb8888(cairo_ptr, color);

    cairo_move_to(cairo_ptr, 7 * scale, 7 * scale);
    cairo_line_to(cairo_ptr, (7 + 8) * scale, (7 + 8) * scale);
    cairo_move_to(cairo_ptr, (7 + 8) * scale, 7 * scale);
    cairo_line_to(cairo_ptr, 7 * scale, (7 + 8) * scale);
    cairo_stroke(cairo_ptr);

    cairo_stroke(cairo_ptr);
    cairo_restore(cairo_ptr);

}

/* ------------------------------------------------------------------------- */
void wlmaker_primitives_draw_window_title(
    cairo_t *cairo_ptr,
    const char *title_ptr,
    uint32_t color)
{
    cairo_save(cairo_ptr);
    cairo_select_font_face(
        cairo_ptr, "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 15.0);
    cairo_set_source_argb8888(cairo_ptr, color);

    cairo_move_to(cairo_ptr, 6, 17);
    cairo_show_text(cairo_ptr, title_ptr ? title_ptr : "Unnamed");
    cairo_restore(cairo_ptr);
}

/* == Local (static) methods =============================================== */

static void test_fill(bs_test_t *test_ptr);
static void test_close(bs_test_t *test_ptr);
static void test_close_large(bs_test_t *test_ptr);
static void test_minimize(bs_test_t *test_ptr);
static void test_minimize_large(bs_test_t *test_ptr);
static void test_window_title(bs_test_t *test_ptr);

/** Unit tests. */
const bs_test_case_t   wlmaker_primitives_test_cases[] = {
    { 1, "fill", test_fill },
    { 1, "close", test_close },
    { 1, "close_large", test_close_large },
    { 1, "minimize", test_minimize },
    { 1, "minimize_large", test_minimize_large },
    { 1, "window_title", test_window_title },
    { 0, NULL, NULL }
};

/** Verifies the fill styles */
void test_fill(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(16, 8);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(16, 8)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);

    // Solid fill.
    wlmtk_style_fill_t fill_solid = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xff4080c0} }
    };
    wlmaker_primitives_cairo_fill(cairo_ptr, &fill_solid);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_fill_solid.png");

    // Horizontal fill.
    wlmtk_style_fill_t fill_hgradient = {
        .type = WLMTK_STYLE_COLOR_HGRADIENT,
        .param = { .hgradient = { .from = 0xff102040, .to = 0xff4080ff }}
    };
    wlmaker_primitives_cairo_fill(cairo_ptr, &fill_hgradient);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_fill_hgradient.png");

    // Diagonal fill.
    wlmtk_style_fill_t fill_dgradient = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .dgradient = { .from = 0xff102040, .to = 0xff4080ff }}
    };
    wlmaker_primitives_cairo_fill(cairo_ptr, &fill_dgradient);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_fill_dgradient.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the looks of the "close" icon. */
void test_close(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(22, 22);
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(22, 22)");
        return;
    }

    wlmaker_primitives_draw_close_icon(cairo_ptr, 22, 0xffffffff);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_close_icon.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the looks of the "close" icon, with non-default size. */
void test_close_large(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(50, 50);
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(50, 50)");
        return;
    }

    wlmaker_primitives_draw_close_icon(cairo_ptr, 50, 0xffffffff);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_close_icon_large.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the looks of the "minimize" icon. */
void test_minimize(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(22, 22);
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(22, 22)");
        return;
    }

    wlmaker_primitives_draw_minimize_icon(cairo_ptr, 22, 0xffffffff);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_minimize_icon.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the looks of the "minimize" icon, widht non-default size. */
void test_minimize_large(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(50, 50);
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(50, 50)");
        return;
    }

    wlmaker_primitives_draw_minimize_icon(cairo_ptr, 50, 0xffffffff);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_minimize_icon_large.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/** Verifies the looks of the window title. */
void test_window_title(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(80, 22);
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(80, 22)");
        return;
    }

    wlmaker_primitives_draw_window_title(cairo_ptr, "Title", 0xffffffff);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "toolkit/primitive_window_title.png");

    cairo_destroy(cairo_ptr);
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/* == End of primitives.c ================================================== */
