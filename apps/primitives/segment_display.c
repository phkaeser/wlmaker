/* ========================================================================= */
/**
 * @file segment_display.c
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

#include "segment_display.h"

#include <libbase/libbase.h>

#include <cairo.h>
#include <stdlib.h>

/* == Declarations ========================================================= */

static void draw_segment(
    cairo_t *cairo_ptr,
    const bs_vector_2f_t origin,
    const bs_vector_2f_t longitudinal,
    const bs_vector_2f_t lateral,
    const wlm_cairo_7segment_param_t *param_ptr,
    const double length);

/**
 * Encoding bits to indicate coloring of segments for each digit.
 *
 * The sequence follows https://en.wikipedia.org/wiki/Seven-segment_display,
 * as follows:
```
 <- 0 ->
^       ^
5       1
v       v
 <- 6 ->
^       ^
4       2
v       v
 <- 3 ->
```
 */
static const uint8_t          seven_segment_encoding[10] = {
    // 6543210 <-- segment.
    0b00111111,  // 0
    0b00000110,  // 1
    0b01011011,  // 2
    0b01001111,  // 3
    0b01100110,  // 4
    0b01101101,  // 5
    0b01111101,  // 6
    0b00000111,  // 7
    0b01111111,  // 8
    0b01101111   // 9
};

const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_6x8 = {
    .offset = 0.6,
    .width = 1.0,
    .hlength = 4.0,
    .vlength = 3.0
};

const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_7x10 = {
    .offset = 0.6,
    .width = 1.0,
    .hlength = 5.0,
    .vlength = 4.0
};

const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_8x12 = {
    .offset = 0.8,
    .width = 1.0,
    .hlength = 6,
    .vlength = 5
};

const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_16x24 = {
    .offset = 1.2,
    .width = 2.0,
    .hlength = 12.0,
    .vlength = 10.0
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlm_cairo_7segment_display_digit(
    cairo_t *cairo_ptr,
    const wlm_cairo_7segment_param_t *pptr,
    uint32_t x,
    uint32_t y,
    uint32_t color_on,
    uint32_t color_off,
    uint8_t digit)
{
    BS_ASSERT(digit < UINT8_C(10));
    uint8_t segments = seven_segment_encoding[digit];

    // Vectors spanning up a segment.
    bs_vector_2f_t longitudinal = { .x = 1.0, .y = 0.0 };
    bs_vector_2f_t lateral = { .x = 0.0, .y = 1.0 };

    bs_vector_2f_t origin = BS_VECTOR_2F(
        x + pptr->width / 2.0, y - 2 * pptr->vlength - pptr->width / 2.0);
    bs_vector_2f_t pos;

    cairo_save(cairo_ptr);

    // Segment 0.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 0) ? color_on : color_off);
    pos = origin;
    draw_segment(cairo_ptr, pos, longitudinal, lateral, pptr, pptr->hlength);

    // Segment 1.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 1) ? color_on : color_off);
    pos = bs_vec_add_2f(origin, BS_VECTOR_2F(pptr->hlength, 0));
    draw_segment(cairo_ptr, pos, lateral, longitudinal, pptr, pptr->vlength);

    // Segment 2.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 2) ? color_on : color_off);
    pos = bs_vec_add_2f(origin, BS_VECTOR_2F(pptr->hlength, pptr->vlength));
    draw_segment(cairo_ptr, pos, lateral, longitudinal, pptr, pptr->vlength);

    // Segment 3.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 3) ? color_on : color_off);
    pos = bs_vec_add_2f(origin, BS_VECTOR_2F(0, 2 * pptr->vlength));
    draw_segment(cairo_ptr, pos, longitudinal, lateral, pptr, pptr->hlength);

    // Segment 4.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 4) ? color_on : color_off);
    pos = bs_vec_add_2f(origin, BS_VECTOR_2F(0, pptr->vlength));
    draw_segment(cairo_ptr, pos, lateral, longitudinal, pptr, pptr->vlength);

    // Segment 5.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 5) ? color_on : color_off);
    pos = origin;
    draw_segment(cairo_ptr, pos, lateral, longitudinal, pptr, pptr->vlength);

    // Segment 6.
    cairo_set_source_argb8888(
        cairo_ptr, segments & (0x01 << 6) ? color_on : color_off);
    pos = bs_vec_add_2f(origin, BS_VECTOR_2F(0, pptr->vlength));
    draw_segment(cairo_ptr, pos, longitudinal, lateral, pptr, pptr->hlength);

    cairo_restore(cairo_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Draws a segment, from `origin` along `longitudinal`/`lateral` direction.
 *
 * A segment spans from the point `origin` along the `longitudinal` vector,
 * and expands by (width/2) along `lateral` direction. It will use the current
 * cairo source color to fill the segment.
 *
```
     +---------------------+
    /                       \
+--+ +                     + +--+  ^
    \                       /      | width/2
     +---------------------+       v

<----> offset              <----> offset
   <-> width/2             <->    width/2
<-------------------------------> length
```
 *
 * @param cairo_ptr
 * @param origin
 * @param longitudinal
 * @param lateral
 * @param param_ptr
 * @param length
 */
void draw_segment(
    cairo_t *cairo_ptr,
    const bs_vector_2f_t origin,
    const bs_vector_2f_t longitudinal,
    const bs_vector_2f_t lateral,
    const wlm_cairo_7segment_param_t *param_ptr,
    const double length)
{
    bs_vector_2f_t rel;

    rel = bs_vec_mul_2f(param_ptr->offset - param_ptr->width/2, longitudinal);
    cairo_move_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_add_2f(
        bs_vec_mul_2f(param_ptr->offset, longitudinal),
        bs_vec_mul_2f(param_ptr->width / 2, lateral));
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_add_2f(
        bs_vec_mul_2f(length - param_ptr->offset, longitudinal),
        bs_vec_mul_2f(param_ptr->width / 2, lateral));
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_mul_2f(length - param_ptr->offset + param_ptr->width/2,
                        longitudinal);
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_add_2f(
        bs_vec_mul_2f(length - param_ptr->offset, longitudinal),
        bs_vec_mul_2f(-param_ptr->width / 2, lateral));
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_add_2f(
        bs_vec_mul_2f(param_ptr->offset, longitudinal),
        bs_vec_mul_2f(-param_ptr->width / 2, lateral));
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    rel = bs_vec_mul_2f(param_ptr->offset - param_ptr->width/2, longitudinal);
    cairo_line_to(cairo_ptr, origin.x + rel.x, origin.y + rel.y);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);
}

/* == Unit tests =========================================================== */

static void test_6x8(bs_test_t *test_ptr);
static void test_7x10(bs_test_t *test_ptr);
static void test_16x24(bs_test_t *test_ptr);

const bs_test_case_t          wlm_cairo_segment_display_test_cases[] = {
    { 1, "6x8", test_6x8 },
    { 1, "7x10", test_7x10 },
    { 1, "16x24", test_16x24 },
    { 0, NULL, NULL }  // sentinel.
};

/* ------------------------------------------------------------------------- */
/** Test for the 6x8-sized digits. */
void test_6x8(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(60, 8);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(60, 8)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed cairo_create_from_bs_gfxbuf.");
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return;
    }
    for (int i = 0; i < 10; i++) {
        wlm_cairo_7segment_display_digit(
            cairo_ptr, &wlm_cairo_7segment_param_6x8, i * 6, 8,
            0xffc0c0ff, 0xff202040, i);
    }
    cairo_destroy(cairo_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "segment_display_6x8.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/** Test for the 7x10-sized digits. */
void test_7x10(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(70, 10);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(70, 10)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed cairo_create_from_bs_gfxbuf.");
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return;
    }
    for (int i = 0; i < 10; i++) {
        wlm_cairo_7segment_display_digit(
            cairo_ptr, &wlm_cairo_7segment_param_7x10, i * 7, 10,
            0xffc0c0ff, 0xff202040, i);
    }
    cairo_destroy(cairo_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "segment_display_7x10.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/** Test for the 16x24-sized digits. */
void test_16x24(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(160, 24);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(160, 24)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed cairo_create_from_bs_gfxbuf.");
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return;
    }
    for (int i = 0; i < 10; i++) {
        wlm_cairo_7segment_display_digit(
            cairo_ptr, &wlm_cairo_7segment_param_16x24, i * 16, 24,
            0xffc0c0ff, 0xff202040, i);
    }
    cairo_destroy(cairo_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "segment_display_16x24.png");
    bs_gfxbuf_destroy(gfxbuf_ptr);
}

/* == End of segment_display.c ============================================= */
