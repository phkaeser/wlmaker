/* ========================================================================= */
/**
 * @file resizebar.c
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

#include "resizebar.h"

#include "box.h"
#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"
#include "resizebar_area.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the title bar. */
struct _wlmtk_resizebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Current width of the resize bar. */
    unsigned                  width;
    /** Style of the resize bar. */
    wlmtk_resizebar_style_t   style;

    /** Background. */
    bs_gfxbuf_t               *gfxbuf_ptr;

    /** Left element of the resizebar. */
    wlmtk_resizebar_area_t    *left_area_ptr;
    /** Center element of the resizebar. */
    wlmtk_resizebar_area_t    *center_area_ptr;
    /** Right element of the resizebar. */
    wlmtk_resizebar_area_t    *right_area_ptr;
};

static void resizebar_box_destroy(wlmtk_box_t *box_ptr);
static bool redraw_buffers(wlmtk_resizebar_t *resizebar_ptr, unsigned width);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t resizebar_box_impl = {
    .destroy = resizebar_box_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_resizebar_t *wlmtk_resizebar_create(
    wlmtk_window_t *window_ptr,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr)
{
    wlmtk_resizebar_t *resizebar_ptr = logged_calloc(
        1, sizeof(wlmtk_resizebar_t));
    if (NULL == resizebar_ptr) return NULL;
    memcpy(&resizebar_ptr->style, style_ptr, sizeof(wlmtk_resizebar_style_t));

    if (!wlmtk_box_init(&resizebar_ptr->super_box,
                        &resizebar_box_impl,
                        WLMTK_BOX_HORIZONTAL)) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }

    if (!redraw_buffers(resizebar_ptr, width)) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }

    resizebar_ptr->left_area_ptr = wlmtk_resizebar_area_create(
        window_ptr, WLR_EDGE_LEFT | WLR_EDGE_BOTTOM);
    if (NULL == resizebar_ptr->left_area_ptr) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &resizebar_ptr->super_box.super_container,
        wlmtk_resizebar_area_element(resizebar_ptr->left_area_ptr));

    resizebar_ptr->center_area_ptr = wlmtk_resizebar_area_create(
        window_ptr, WLR_EDGE_BOTTOM);
    if (NULL == resizebar_ptr->center_area_ptr) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element_before(
        &resizebar_ptr->super_box.super_container,
        NULL,
        wlmtk_resizebar_area_element(resizebar_ptr->center_area_ptr));

    resizebar_ptr->right_area_ptr = wlmtk_resizebar_area_create(
        window_ptr, WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);
    if (NULL == resizebar_ptr->right_area_ptr) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element_before(
        &resizebar_ptr->super_box.super_container,
        NULL,
        wlmtk_resizebar_area_element(resizebar_ptr->right_area_ptr));

    if (!wlmtk_resizebar_set_width(resizebar_ptr, width)) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }

    return resizebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_destroy(wlmtk_resizebar_t *resizebar_ptr)
{
    if (NULL != resizebar_ptr->right_area_ptr) {
        wlmtk_container_remove_element(
            &resizebar_ptr->super_box.super_container,
            wlmtk_resizebar_area_element(resizebar_ptr->right_area_ptr));
        wlmtk_resizebar_area_destroy(resizebar_ptr->right_area_ptr);
        resizebar_ptr->right_area_ptr = NULL;
    }
    if (NULL != resizebar_ptr->center_area_ptr) {
        wlmtk_container_remove_element(
            &resizebar_ptr->super_box.super_container,
            wlmtk_resizebar_area_element(resizebar_ptr->center_area_ptr));
        wlmtk_resizebar_area_destroy(resizebar_ptr->center_area_ptr);
        resizebar_ptr->center_area_ptr = NULL;
    }
    if (NULL != resizebar_ptr->left_area_ptr) {
        wlmtk_container_remove_element(
            &resizebar_ptr->super_box.super_container,
            wlmtk_resizebar_area_element(resizebar_ptr->left_area_ptr));

        wlmtk_resizebar_area_destroy(resizebar_ptr->left_area_ptr);
        resizebar_ptr->left_area_ptr = NULL;
    }

    if (NULL != resizebar_ptr->gfxbuf_ptr) {
        bs_gfxbuf_destroy(resizebar_ptr->gfxbuf_ptr);
        resizebar_ptr->gfxbuf_ptr = NULL;
    }

    wlmtk_box_fini(&resizebar_ptr->super_box);
    free(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_set_cursor(
    wlmtk_resizebar_t *resizebar_ptr,
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr)
{
    wlmtk_resizebar_area_set_cursor(
        resizebar_ptr->left_area_ptr,
        wlr_cursor_ptr,
        wlr_xcursor_manager_ptr);
    wlmtk_resizebar_area_set_cursor(
        resizebar_ptr->center_area_ptr,
        wlr_cursor_ptr,
        wlr_xcursor_manager_ptr);
    wlmtk_resizebar_area_set_cursor(
        resizebar_ptr->right_area_ptr,
        wlr_cursor_ptr,
        wlr_xcursor_manager_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_resizebar_set_width(
    wlmtk_resizebar_t *resizebar_ptr,
    unsigned width)
{
    if (resizebar_ptr->width == width) return true;
    if (!redraw_buffers(resizebar_ptr, width)) return false;
    BS_ASSERT(width == resizebar_ptr->width);
    BS_ASSERT(width == resizebar_ptr->gfxbuf_ptr->width);

    int right_corner_width = BS_MIN(
        (int)width, (int)resizebar_ptr->style.corner_width);
    int left_corner_width = BS_MAX(
        0, BS_MIN((int)width - right_corner_width,
                  (int)resizebar_ptr->style.corner_width));
    int center_width = BS_MAX(
        0, (int)width - right_corner_width - left_corner_width);

    wlmtk_element_set_visible(
        wlmtk_resizebar_area_element(resizebar_ptr->left_area_ptr),
        0 < left_corner_width);
    wlmtk_element_set_visible(
        wlmtk_resizebar_area_element(resizebar_ptr->center_area_ptr),
        0 < center_width);
    wlmtk_element_set_visible(
        wlmtk_resizebar_area_element(resizebar_ptr->right_area_ptr),
        0 < right_corner_width);

    if (!wlmtk_resizebar_area_redraw(
            resizebar_ptr->left_area_ptr,
            resizebar_ptr->gfxbuf_ptr,
            0, left_corner_width,
            &resizebar_ptr->style)) {
        return false;
    }
    if (!wlmtk_resizebar_area_redraw(
            resizebar_ptr->center_area_ptr,
            resizebar_ptr->gfxbuf_ptr,
            left_corner_width, center_width,
            &resizebar_ptr->style)) {
        return false;
    }
    if (!wlmtk_resizebar_area_redraw(
            resizebar_ptr->right_area_ptr,
            resizebar_ptr->gfxbuf_ptr,
            left_corner_width + center_width, right_corner_width,
            &resizebar_ptr->style)) {
        return false;
    }

    wlmtk_container_update_layout(&resizebar_ptr->super_box.super_container);
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_resizebar_element(wlmtk_resizebar_t *resizebar_ptr)
{
    return &resizebar_ptr->super_box.super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from box. Wraps to our dtor. */
void resizebar_box_destroy(wlmtk_box_t *box_ptr)
{
    wlmtk_resizebar_t *resizebar_ptr = BS_CONTAINER_OF(
        box_ptr, wlmtk_resizebar_t, super_box);
    wlmtk_resizebar_destroy(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
/** Redraws the resizebar's background in appropriate size. */
bool redraw_buffers(wlmtk_resizebar_t *resizebar_ptr, unsigned width)
{
    cairo_t *cairo_ptr;

    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(
        width, resizebar_ptr->style.height);
    if (NULL == gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(cairo_ptr, &resizebar_ptr->style.fill);
    cairo_destroy(cairo_ptr);

    if (NULL != resizebar_ptr->gfxbuf_ptr) {
        bs_gfxbuf_destroy(resizebar_ptr->gfxbuf_ptr);
    }
    resizebar_ptr->gfxbuf_ptr = gfxbuf_ptr;
    resizebar_ptr->width = width;
    return true;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_variable_width(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_resizebar_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "variable_width", test_variable_width },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises @ref wlmtk_resizebar_create and @ref wlmtk_resizebar_destroy. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_resizebar_style_t style = {};
    wlmtk_resizebar_t *resizebar_ptr = wlmtk_resizebar_create(
        NULL, 120, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, resizebar_ptr);

    wlmtk_element_destroy(wlmtk_resizebar_element(resizebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Performs resizing and verifies the elements are shown as expected. */
void test_variable_width(bs_test_t *test_ptr)
{
    wlmtk_resizebar_style_t style = { .height = 7, .corner_width = 16 };
    wlmtk_resizebar_t *resizebar_ptr = wlmtk_resizebar_create(
        NULL, 0, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, resizebar_ptr);

    wlmtk_element_t *left_elem_ptr = wlmtk_resizebar_area_element(
        resizebar_ptr->left_area_ptr);
    wlmtk_element_t *center_elem_ptr = wlmtk_resizebar_area_element(
        resizebar_ptr->center_area_ptr);
    wlmtk_element_t *right_elem_ptr = wlmtk_resizebar_area_element(
        resizebar_ptr->right_area_ptr);

    // Zero width. Zero visibility.
    BS_TEST_VERIFY_FALSE(test_ptr, left_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, center_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, right_elem_ptr->visible);

    // Sufficient space for all the elements.
    BS_TEST_VERIFY_TRUE(
        test_ptr, wlmtk_resizebar_set_width(resizebar_ptr, 33));
    BS_TEST_VERIFY_TRUE(test_ptr, left_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, center_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, right_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 16, center_elem_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 17, right_elem_ptr->x);

    // Not enough space for the center element.
    BS_TEST_VERIFY_TRUE(
        test_ptr, wlmtk_resizebar_set_width(resizebar_ptr, 32));
    BS_TEST_VERIFY_TRUE(test_ptr, left_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, center_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, right_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 16, right_elem_ptr->x);

    // Not enough space for center and left element.
    BS_TEST_VERIFY_TRUE(
        test_ptr, wlmtk_resizebar_set_width(resizebar_ptr, 16));
    BS_TEST_VERIFY_FALSE(test_ptr, left_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, center_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, right_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, right_elem_ptr->x);

    wlmtk_element_destroy(wlmtk_resizebar_element(resizebar_ptr));
}

/* == End of resizebar.c =================================================== */
