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

static void container_destroy(wlmtk_container_t *container_ptr);
static void container_update_layout(wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_container_impl_t container_impl = {
    .destroy = container_destroy,
    .update_layout = container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_box_init(
    wlmtk_box_t *box_ptr,
    const wlmtk_box_impl_t *box_impl_ptr)
{
    BS_ASSERT(NULL != box_ptr);
    BS_ASSERT(NULL != box_impl_ptr);
    BS_ASSERT(NULL != box_impl_ptr->destroy);
    if (!wlmtk_container_init(&box_ptr->super_container, &container_impl)) {
        return false;
    }
    memcpy(&box_ptr->impl, box_impl_ptr, sizeof(wlmtk_box_impl_t));

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_fini(__UNUSED__ wlmtk_box_t *box_ptr)
{
    // nothing to do.
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_box_t *box_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_box_t, super_container);
    wlmtk_box_fini(box_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the layout of the box.
 *
 * Steps through all visible elements, and sets their position to be
 * left-to-right.
 *
 * @param container_ptr
 */
void container_update_layout(wlmtk_container_t *container_ptr)
{
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

        x = position - left;
        wlmtk_element_set_position(element_ptr, position - left, y);
        position += right - left;
    }

    // configure parent container.
    wlmtk_container_update_layout(
        container_ptr->super_element.parent_container_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_layout(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_box_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "layout", test_layout },
    { 0, NULL, NULL }
};

/** dtor for the testcase. */
static void test_box_destroy(wlmtk_box_t *box_ptr) {
    wlmtk_box_fini(box_ptr);
}
/** A testcase box implementation. */
static const wlmtk_box_impl_t test_box_impl = {
    .destroy = test_box_destroy,
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    wlmtk_box_init(&box, &test_box_impl);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, box.impl.destroy);
    box.impl.destroy(&box);
}

/* ------------------------------------------------------------------------- */
/** Tests layouting. */
void test_layout(bs_test_t *test_ptr)
{
    wlmtk_box_t box;
    wlmtk_box_init(&box, &test_box_impl);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e1_ptr->element, true);
    e1_ptr->width = 10;
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e2_ptr->element, false);
    e2_ptr->width = 20;
    wlmtk_fake_element_t *e3_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&e3_ptr->element, true);
    e3_ptr->width = 40;

    // Note: Elements are added "in front" == left.
    wlmtk_container_add_element(&box.super_container, &e1_ptr->element);
    wlmtk_container_add_element(&box.super_container, &e2_ptr->element);
    wlmtk_container_add_element(&box.super_container, &e3_ptr->element);

    // Layout: e3 | e1 (e2 is invisible).
    BS_TEST_VERIFY_EQ(test_ptr, 40, e1_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_ptr->element.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, e3_ptr->element.x);

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
    BS_TEST_VERIFY_EQ(test_ptr, 00, e1_ptr->element.x);
    wlmtk_container_remove_element(&box.super_container, &e1_ptr->element);

    wlmtk_element_destroy(&e3_ptr->element);
    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    box.impl.destroy(&box);
}

/* == End of box.c ========================================================= */
