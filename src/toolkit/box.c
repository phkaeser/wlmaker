/* ========================================================================= */
/**
 * @file box.c
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

#include "box.h"

/* == Declarations ========================================================= */

static void _wlmtk_box_container_update_layout(
    wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Virtual method table: @ref wlmtk_container_t at @ref wlmtk_box_t level. */
static const wlmtk_container_vmt_t box_container_vmt = {
    .update_layout = _wlmtk_box_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_box_init(
    wlmtk_box_t *box_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_box_orientation_t orientation)
{
    BS_ASSERT(NULL != box_ptr);
    memset(box_ptr, 0, sizeof(wlmtk_box_t));
    if (!wlmtk_container_init(&box_ptr->super_container, env_ptr)) {
        return false;
    }
    box_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &box_ptr->super_container, &box_container_vmt);

    box_ptr->orientation = orientation;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_fini(__UNUSED__ wlmtk_box_t *box_ptr)
{
    // nothing to do.
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Updates the layout of the box.
 *
 * Steps through all visible elements, and sets their position to be
 * left-to-right.
 *
 * @param container_ptr
 */
void _wlmtk_box_container_update_layout(
    wlmtk_container_t *container_ptr)
{
    wlmtk_box_t *box_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_box_t, super_container);

    int position = 0;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        if (!element_ptr->visible) continue;

        int left, top, right, bottom;
        wlmtk_element_get_dimensions(element_ptr, &left, &top, &right, &bottom);
        int x, y;
        wlmtk_element_get_position(element_ptr, &x, &y);

        switch (box_ptr->orientation) {
        case WLMTK_BOX_HORIZONTAL:
            x = position - left;
            position += right - left;
            break;

        case WLMTK_BOX_VERTICAL:
            y = position - top;
            position += bottom - top;
            break;

        default:
            bs_log(BS_FATAL, "Weird orientation %d.", box_ptr->orientation);
        }
        wlmtk_element_set_position(element_ptr, x, y);
    }

    // Run the base class' update layout; may update pointer focus.
    // We do this only after having updated the position of the elements.
    box_ptr->orig_super_container_vmt.update_layout(container_ptr);

    // configure parent container.
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout(
            container_ptr->super_element.parent_container_ptr);
    }
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_layout_horizontal(bs_test_t *test_ptr);
static void test_layout_vertical(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_box_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "layout_horizontal", test_layout_horizontal },
    { 1, "layout_vertical", test_layout_vertical },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_box_init(
                            &box, NULL, WLMTK_BOX_HORIZONTAL));
    wlmtk_box_fini(&box);
}

/* ------------------------------------------------------------------------- */
/** Tests layouting horizontally */
void test_layout_horizontal(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    wlmtk_box_init(&box, NULL, WLMTK_BOX_HORIZONTAL);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e1_ptr->element, true);
    e1_ptr->width = 10;
    e1_ptr->height = 1;
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e2_ptr->element, false);
    e2_ptr->width = 20;
    e1_ptr->height = 2;
    wlmtk_fake_element_t *e3_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e3_ptr->element, true);
    e3_ptr->width = 40;
    e3_ptr->height = 4;

    // Note: Elements are added "in front" == left.
    wlmtk_container_add_element(&box.super_container, &e1_ptr->element);
    wlmtk_container_add_element(&box.super_container, &e2_ptr->element);
    wlmtk_container_add_element(&box.super_container, &e3_ptr->element);

    // Layout: e3 | e1 (e2 is invisible).
    BS_TEST_VERIFY_EQ(test_ptr, 40, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.y);

    // Make e2 visible, now we should have: e3 | e2 | e1.
    wlmtk_element_set_visible(&e2_ptr->element, true);
    BS_TEST_VERIFY_EQ(test_ptr, 60, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 40, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.x);

    // Remove elements. Must update each.
    wlmtk_container_remove_element(&box.super_container, &e3_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 20, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    wlmtk_container_remove_element(&box.super_container, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.x);
    wlmtk_container_remove_element(&box.super_container, &e1_ptr->element);

    wlmtk_element_destroy(&e3_ptr->element);
    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    wlmtk_box_fini(&box);
}

/* ------------------------------------------------------------------------- */
/** Tests layouting vertically */
void test_layout_vertical(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    wlmtk_box_init(&box, NULL, WLMTK_BOX_VERTICAL);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e1_ptr->element, true);
    e1_ptr->width = 10;
    e1_ptr->height = 1;
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e2_ptr->element, true);
    e2_ptr->width = 20;
    e2_ptr->height = 2;

    // Note: Elements are added "in front" == left.
    wlmtk_container_add_element(&box.super_container, &e1_ptr->element);
    wlmtk_container_add_element(&box.super_container, &e2_ptr->element);

    // Layout: e2 | e1.
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.y);

    // Remove elements. Must update each.
    wlmtk_container_remove_element(&box.super_container, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.y);
    wlmtk_container_remove_element(&box.super_container, &e1_ptr->element);

    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    wlmtk_box_fini(&box);
}

/* == End of box.c ========================================================= */
