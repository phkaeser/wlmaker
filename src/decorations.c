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

/* == Unit tests =========================================================== */

static void test_tile(bs_test_t *test_ptr);

const bs_test_case_t          wlmaker_decorations_test_cases[] = {
    { 1, "tile", test_tile },
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

/* == End of decorations.c ================================================= */
