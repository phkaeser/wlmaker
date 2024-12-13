/* ========================================================================= */
/**
 * @file bordered.c
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

#include "bordered.h"

/* == Declarations ========================================================= */

static void _wlmtk_bordered_container_update_layout(
    wlmtk_container_t *container_ptr);

static wlmtk_rectangle_t * _wlmtk_bordered_create_border_rectangle(
    wlmtk_bordered_t *bordered_ptr,
    wlmtk_env_t *env_ptr);
static void _wlmtk_bordered_destroy_border_rectangle(
    wlmtk_bordered_t *bordered_ptr,
    wlmtk_rectangle_t **rectangle_ptr_ptr);
static void _wlmtk_bordered_set_positions(wlmtk_bordered_t *bordered_ptr);

/* == Data ================================================================= */

/** Virtual method table: @ref wlmtk_container_t at @ref wlmtk_bordered_t. */
static const wlmtk_container_vmt_t bordered_container_vmt = {
    .update_layout = _wlmtk_bordered_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_bordered_init(wlmtk_bordered_t *bordered_ptr,
                         wlmtk_env_t *env_ptr,
                         wlmtk_element_t *element_ptr,
                         const wlmtk_margin_style_t *style_ptr)
{
    BS_ASSERT(NULL != bordered_ptr);
    memset(bordered_ptr, 0, sizeof(wlmtk_bordered_t));
    if (!wlmtk_container_init(&bordered_ptr->super_container, env_ptr)) {
        return false;
    }
    bordered_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &bordered_ptr->super_container, &bordered_container_vmt);
    memcpy(&bordered_ptr->style, style_ptr, sizeof(wlmtk_margin_style_t));

    bordered_ptr->element_ptr = element_ptr;
    wlmtk_container_add_element(&bordered_ptr->super_container,
                                bordered_ptr->element_ptr);

    bordered_ptr->northern_border_rectangle_ptr =
        _wlmtk_bordered_create_border_rectangle(bordered_ptr, env_ptr);
    bordered_ptr->eastern_border_rectangle_ptr =
        _wlmtk_bordered_create_border_rectangle(bordered_ptr, env_ptr);
    bordered_ptr->southern_border_rectangle_ptr =
        _wlmtk_bordered_create_border_rectangle(bordered_ptr, env_ptr);
    bordered_ptr->western_border_rectangle_ptr =
        _wlmtk_bordered_create_border_rectangle(bordered_ptr, env_ptr);
    if (NULL == bordered_ptr->northern_border_rectangle_ptr ||
        NULL == bordered_ptr->eastern_border_rectangle_ptr ||
        NULL == bordered_ptr->southern_border_rectangle_ptr ||
        NULL == bordered_ptr->western_border_rectangle_ptr) {
        wlmtk_bordered_fini(bordered_ptr);
        return false;
    }

    _wlmtk_bordered_set_positions(bordered_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_bordered_fini(wlmtk_bordered_t *bordered_ptr)
{
    _wlmtk_bordered_destroy_border_rectangle(
        bordered_ptr, &bordered_ptr->western_border_rectangle_ptr);
    _wlmtk_bordered_destroy_border_rectangle(
        bordered_ptr, &bordered_ptr->southern_border_rectangle_ptr);
    _wlmtk_bordered_destroy_border_rectangle(
        bordered_ptr, &bordered_ptr->eastern_border_rectangle_ptr);
    _wlmtk_bordered_destroy_border_rectangle(
        bordered_ptr, &bordered_ptr->northern_border_rectangle_ptr);

    wlmtk_container_remove_element(&bordered_ptr->super_container,
                                   bordered_ptr->element_ptr);
    wlmtk_container_fini(&bordered_ptr->super_container);
    memset(bordered_ptr, 0, sizeof(wlmtk_bordered_t));
}

/* ------------------------------------------------------------------------- */
void wlmtk_bordered_set_style(wlmtk_bordered_t *bordered_ptr,
                              const wlmtk_margin_style_t *style_ptr)
{
    memcpy(&bordered_ptr->style, style_ptr, sizeof(wlmtk_margin_style_t));

    _wlmtk_bordered_container_update_layout(&bordered_ptr->super_container);

    // Guard clause. Actually, if *any* of the rectangles was not created.
    if (NULL == bordered_ptr->western_border_rectangle_ptr) return;

    wlmtk_rectangle_set_color(
        bordered_ptr->northern_border_rectangle_ptr, style_ptr->color);
    wlmtk_rectangle_set_color(
        bordered_ptr->eastern_border_rectangle_ptr, style_ptr->color);
    wlmtk_rectangle_set_color(
        bordered_ptr->southern_border_rectangle_ptr, style_ptr->color);
    wlmtk_rectangle_set_color(
        bordered_ptr->western_border_rectangle_ptr, style_ptr->color);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_bordered_element(wlmtk_bordered_t *bordered_ptr)
{
    return &bordered_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Updates the layout of the bordered element.
 *
 * @param container_ptr
 */
void _wlmtk_bordered_container_update_layout(
    wlmtk_container_t *container_ptr)
{
    wlmtk_bordered_t *bordered_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_bordered_t, super_container);

    _wlmtk_bordered_set_positions(bordered_ptr);

    bordered_ptr->orig_super_container_vmt.update_layout(container_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates a border rectangle and adds it to `bordered_ptr`. */
wlmtk_rectangle_t * _wlmtk_bordered_create_border_rectangle(
    wlmtk_bordered_t *bordered_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = wlmtk_rectangle_create(
        env_ptr, 0, 0, bordered_ptr->style.color);
    if (NULL == rectangle_ptr) return NULL;

    wlmtk_element_set_visible(wlmtk_rectangle_element(rectangle_ptr), true);
    wlmtk_container_add_element_atop(
        &bordered_ptr->super_container,
        NULL,
        wlmtk_rectangle_element(rectangle_ptr));

    return rectangle_ptr;
}

/* ------------------------------------------------------------------------- */
/** Removes the rectangle from `bordered_ptr`, destroys it and NULLs it. */
void _wlmtk_bordered_destroy_border_rectangle(
    wlmtk_bordered_t *bordered_ptr,
    wlmtk_rectangle_t **rectangle_ptr_ptr)
{
    if (NULL == *rectangle_ptr_ptr) return;

    wlmtk_container_remove_element(
        &bordered_ptr->super_container,
        wlmtk_rectangle_element(*rectangle_ptr_ptr));
    wlmtk_rectangle_destroy(*rectangle_ptr_ptr);
    *rectangle_ptr_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the position of all 4 border elements.
 *
 * Retrieves the position and dimensions of @ref wlmtk_bordered_t::element_ptr
 * and arranges the 4 border elements around it.
 *
 * @param bordered_ptr
 */
void _wlmtk_bordered_set_positions(wlmtk_bordered_t *bordered_ptr)
{
    int x1, y1, x2, y2;
    int x_pos, y_pos;

    if (NULL == bordered_ptr->western_border_rectangle_ptr) return;

    int margin = bordered_ptr->style.width;

    wlmtk_element_get_dimensions(
        bordered_ptr->element_ptr, &x1, &y1, &x2, &y2);
    x_pos = -x1 + margin;
    y_pos = -y1 + margin;
    int width = x2 - x1;
    int height = y2 - y1;
    wlmtk_element_set_position(bordered_ptr->element_ptr, x_pos, y_pos);

    wlmtk_element_set_position(
        wlmtk_rectangle_element(bordered_ptr->northern_border_rectangle_ptr),
        x_pos - margin, y_pos - margin);
    wlmtk_rectangle_set_size(
        bordered_ptr->northern_border_rectangle_ptr,
        width + 2 * margin, margin);

    wlmtk_element_set_position(
        wlmtk_rectangle_element(bordered_ptr->eastern_border_rectangle_ptr),
        x_pos + width, y_pos);
    wlmtk_rectangle_set_size(
        bordered_ptr->eastern_border_rectangle_ptr,
        margin, height);

    wlmtk_element_set_position(
        wlmtk_rectangle_element(bordered_ptr->southern_border_rectangle_ptr),
        x_pos - margin, y_pos + height);
    wlmtk_rectangle_set_size(
        bordered_ptr->southern_border_rectangle_ptr,
        width + 2 * margin, margin);

    wlmtk_element_set_position(
        wlmtk_rectangle_element(bordered_ptr->western_border_rectangle_ptr),
        x_pos - margin, y_pos);
    wlmtk_rectangle_set_size(
        bordered_ptr->western_border_rectangle_ptr,
        margin, height);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_bordered_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/** Style used for tests. */
static const wlmtk_margin_style_t test_style = {
    .width = 2,
    .color = 0xff000000
};

/** Helper: Tests that the rectangle is positioned as specified. */
void test_rectangle_pos(bs_test_t *test_ptr, wlmtk_rectangle_t *rect_ptr,
                        int x, int y, int width, int height)
{
    wlmtk_element_t *elem_ptr = wlmtk_rectangle_element(rect_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, x, elem_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, y, elem_ptr->y);

    int x1, y1, x2, y2;
    wlmtk_element_get_dimensions(elem_ptr, &x1, &y1, &x2, &y2);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x1);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y1);
    BS_TEST_VERIFY_EQ(test_ptr, width, x2 - x1);
    BS_TEST_VERIFY_EQ(test_ptr, height, y2 - y1);
}

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    fe_ptr->dimensions.width = 100;
    fe_ptr->dimensions.height = 20;
    wlmtk_element_set_position(&fe_ptr->element, -10, -4);

    wlmtk_bordered_t bordered;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_bordered_init(
                            &bordered, NULL, &fe_ptr->element, &test_style));

    // Positions of border elements.
    test_rectangle_pos(
        test_ptr, bordered.northern_border_rectangle_ptr,
        0, 0, 104, 2);
    test_rectangle_pos(
        test_ptr, bordered.eastern_border_rectangle_ptr,
        102, 2, 2, 20);
    test_rectangle_pos(
        test_ptr, bordered.southern_border_rectangle_ptr,
        0, 22, 104, 2);
    test_rectangle_pos(
        test_ptr, bordered.western_border_rectangle_ptr,
        0, 2, 2, 20);

    // Update layout, test updated positions.
    fe_ptr->dimensions.width = 200;
    fe_ptr->dimensions.height = 120;
    wlmtk_container_update_layout(&bordered.super_container);
    test_rectangle_pos(
        test_ptr, bordered.northern_border_rectangle_ptr,
        0, 0, 204, 2);
    test_rectangle_pos(
        test_ptr, bordered.eastern_border_rectangle_ptr,
        202, 2, 2, 120);
    test_rectangle_pos(
        test_ptr, bordered.southern_border_rectangle_ptr,
        0, 122, 204, 2);
    test_rectangle_pos(
        test_ptr, bordered.western_border_rectangle_ptr,
        0, 2, 2, 120);

    wlmtk_bordered_fini(&bordered);

    wlmtk_element_destroy(&fe_ptr->element);
}

/* == End of bordered.c ==================================================== */
