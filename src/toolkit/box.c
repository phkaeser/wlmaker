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

#include <toolkit/box.h>
#include <toolkit/rectangle.h>

/* == Declarations ========================================================= */

static void _wlmtk_box_container_update_layout(
    wlmtk_container_t *container_ptr);
static bs_dllist_node_t *create_margin(wlmtk_box_t *box_ptr);

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
    wlmtk_box_orientation_t orientation,
    const wlmtk_margin_style_t *style_ptr)
{
    BS_ASSERT(NULL != box_ptr);
    memset(box_ptr, 0, sizeof(wlmtk_box_t));
    if (!wlmtk_container_init(&box_ptr->super_container, env_ptr)) {
        return false;
    }
    box_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &box_ptr->super_container, &box_container_vmt);
    box_ptr->env_ptr = env_ptr;
    memcpy(&box_ptr->style, style_ptr, sizeof(wlmtk_margin_style_t));

    if (!wlmtk_container_init(&box_ptr->element_container, env_ptr)) {
        wlmtk_box_fini(box_ptr);
        return false;
    }
    wlmtk_element_set_visible(&box_ptr->element_container.super_element, true);
    wlmtk_container_add_element(&box_ptr->super_container,
                                &box_ptr->element_container.super_element);
    if (!wlmtk_container_init(&box_ptr->margin_container, env_ptr)) {
        wlmtk_box_fini(box_ptr);
        return false;
    }
    wlmtk_element_set_visible(&box_ptr->margin_container.super_element, true);
    // Keep margins behind the box's elements.
    wlmtk_container_add_element_atop(
        &box_ptr->super_container,
        NULL,
        &box_ptr->margin_container.super_element);

    box_ptr->orientation = orientation;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_fini(wlmtk_box_t *box_ptr)
{
    if (NULL != box_ptr->element_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &box_ptr->super_container,
            &box_ptr->element_container.super_element);
        wlmtk_container_fini(&box_ptr->element_container);
    }
    if (NULL != box_ptr->margin_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &box_ptr->super_container,
            &box_ptr->margin_container.super_element);
        wlmtk_container_fini(&box_ptr->margin_container);
    }

    wlmtk_container_fini(&box_ptr->super_container);
    memset(box_ptr, 0, sizeof(wlmtk_box_t));
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_add_element_front(
    wlmtk_box_t *box_ptr,
    wlmtk_element_t *element_ptr)
{
    wlmtk_container_add_element(&box_ptr->element_container, element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_add_element_back(
    wlmtk_box_t *box_ptr,
    wlmtk_element_t *element_ptr)
{
    wlmtk_container_add_element_atop(
        &box_ptr->element_container, NULL, element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_remove_element(wlmtk_box_t *box_ptr, wlmtk_element_t *element_ptr)
{
    wlmtk_container_remove_element(&box_ptr->element_container, element_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_box_element(wlmtk_box_t *box_ptr)
{
    return &box_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Updates the layout of the box.
 *
 * Steps through all visible elements, and sets their position to be
 * left-to-right. Also updates and repositions all margin elements.
 *
 * @param container_ptr
 */
void _wlmtk_box_container_update_layout(
    wlmtk_container_t *container_ptr)
{
    wlmtk_box_t *box_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_box_t, super_container);
    wlmtk_element_t *margin_element_ptr = NULL;

    int margin_x = 0;
    int margin_y = 0;
    int margin_width = box_ptr->style.width;
    int margin_height = box_ptr->style.width;

    size_t visible_elements = 0;
    for (bs_dllist_node_t *dlnode_ptr = box_ptr->element_container.elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        if (element_ptr->visible) visible_elements++;
    }

    int position = 0;
    bs_dllist_node_t *margin_dlnode_ptr = box_ptr->margin_container.elements.head_ptr;
    for (bs_dllist_node_t *dlnode_ptr = box_ptr->element_container.elements.head_ptr;
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
            margin_x = position + right - left;
            margin_height = bottom - top;
            position = margin_x + box_ptr->style.width;
            break;

        case WLMTK_BOX_VERTICAL:
            y = position - top;
            margin_y = position + bottom - top;
            margin_width = right - left;
            position = margin_y + box_ptr->style.width;
            break;

        default:
            bs_log(BS_FATAL, "Weird orientation %d.", box_ptr->orientation);
        }
        wlmtk_element_set_position(element_ptr, x, y);
        visible_elements--;

        // Early exit: No margin needed, if there's no next element.
        if (NULL == dlnode_ptr->next_ptr || 0 >= visible_elements) break;

        // If required: Create new margin, then position the margin element.
        if (NULL == margin_dlnode_ptr) {
            margin_dlnode_ptr = create_margin(box_ptr);
        }
        margin_element_ptr = wlmtk_element_from_dlnode(margin_dlnode_ptr);
        wlmtk_element_set_position(margin_element_ptr, margin_x, margin_y);
        wlmtk_rectangle_set_size(
            wlmtk_rectangle_from_element(margin_element_ptr),
            margin_width, margin_height);

        margin_dlnode_ptr = margin_dlnode_ptr->next_ptr;
    }

    // Remove excess margin nodes.
    while (NULL != margin_dlnode_ptr) {
        margin_element_ptr = wlmtk_element_from_dlnode(margin_dlnode_ptr);
        margin_dlnode_ptr = margin_dlnode_ptr->next_ptr;
        wlmtk_container_remove_element(
            &box_ptr->margin_container, margin_element_ptr);
        wlmtk_element_destroy(margin_element_ptr);
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

/* ------------------------------------------------------------------------- */
/** Creates a new margin element, and returns the dlnode. */
bs_dllist_node_t *create_margin(wlmtk_box_t *box_ptr)
{
    wlmtk_rectangle_t *rect_ptr = wlmtk_rectangle_create(
        box_ptr->env_ptr, 0, 0, box_ptr->style.color);
    BS_ASSERT(NULL != rect_ptr);

    wlmtk_container_add_element_atop(
        &box_ptr->margin_container,
        NULL,
        wlmtk_rectangle_element(rect_ptr));
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(rect_ptr), true);

    return wlmtk_dlnode_from_element(wlmtk_rectangle_element(rect_ptr));
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

/** Style used for tests. */
static const wlmtk_margin_style_t test_style = {
    .width = 2,
    .color = 0xff000000
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_box_init(
                            &box, NULL, WLMTK_BOX_HORIZONTAL, &test_style));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &box.super_container.super_element,
        wlmtk_box_element(&box));

    wlmtk_box_fini(&box);
}

/* ------------------------------------------------------------------------- */
/** Tests layouting horizontally */
void test_layout_horizontal(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    wlmtk_box_init(&box, NULL, WLMTK_BOX_HORIZONTAL, &test_style);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e1_ptr->element, true);
    e1_ptr->dimensions.width = 10;
    e1_ptr->dimensions.height = 1;
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e2_ptr->element, false);
    e2_ptr->dimensions.width = 20;
    e1_ptr->dimensions.height = 2;
    wlmtk_fake_element_t *e3_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e3_ptr->element, true);
    e3_ptr->dimensions.width = 40;
    e3_ptr->dimensions.height = 4;

    // Note: Elements are added "in front" == left.
    wlmtk_box_add_element_front(&box, &e1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(&box.margin_container.elements));
    wlmtk_box_add_element_front(&box, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(&box.margin_container.elements));
    wlmtk_box_add_element_front(&box, &e3_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&box.margin_container.elements));

    // Layout: e3 | e1 (e2 is invisible).
    BS_TEST_VERIFY_EQ(test_ptr, 42, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.y);

    // Make e2 visible, now we should have: e3 | e2 | e1.
    wlmtk_element_set_visible(&e2_ptr->element, true);
    BS_TEST_VERIFY_EQ(test_ptr, 64, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 42, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&box.margin_container.elements));

    wlmtk_element_set_visible(&e1_ptr->element, false);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&box.margin_container.elements));
    wlmtk_element_set_visible(&e1_ptr->element, true);

    // Remove elements. Must update each.
    wlmtk_box_remove_element(&box, &e3_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 22, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&box.margin_container.elements));
    wlmtk_box_remove_element(&box, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(&box.margin_container.elements));
    wlmtk_box_remove_element(&box, &e1_ptr->element);

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
    wlmtk_box_init(&box, NULL, WLMTK_BOX_VERTICAL, &test_style);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e1_ptr->element, true);
    e1_ptr->dimensions.width = 100;
    e1_ptr->dimensions.height = 10;
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e2_ptr->element, true);
    e2_ptr->dimensions.width = 200;
    e2_ptr->dimensions.height = 20;

    // Note: Elements are added "in front" == left.
    wlmtk_box_add_element_front(&box, &e1_ptr->element);
    wlmtk_box_add_element_front(&box, &e2_ptr->element);

    // Layout: e2 | e1.
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 22, e1_ptr->element.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.y);

    // Remove elements. Must update each.
    wlmtk_box_remove_element(&box, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e1_ptr->element.y);
    wlmtk_box_remove_element(&box, &e1_ptr->element);

    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    wlmtk_box_fini(&box);
}

/* == End of box.c ========================================================= */
