/* ========================================================================= */
/**
 * @file element.c
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

#include "toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_element_init(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_impl_t *element_impl_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    BS_ASSERT(NULL != element_impl_ptr);
    BS_ASSERT(NULL != element_impl_ptr->destroy);
    BS_ASSERT(NULL != element_impl_ptr->create_scene_node);
    memset(element_ptr, 0, sizeof(wlmtk_element_t));

    element_ptr->impl_ptr = element_impl_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr)
{
    // Verify we're no longer mapped, nor part of a container.
    BS_ASSERT(!wlmtk_element_mapped(element_ptr));
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);

    element_ptr->impl_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_element(
    wlmtk_element_t *element_ptr)
{
    return &element_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_element_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_element_t, dlnode);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr)
{
    if (element_ptr->parent_container_ptr == parent_container_ptr) return;

    if (wlmtk_element_mapped(element_ptr)) {
        wlmtk_element_unmap(element_ptr);
    }
    element_ptr->parent_container_ptr = parent_container_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_map(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr =
        wlmtk_container_wlr_scene_tree(element_ptr->parent_container_ptr);
    BS_ASSERT(NULL != parent_wlr_scene_tree_ptr);

    element_ptr->wlr_scene_node_ptr = element_ptr->impl_ptr->create_scene_node(
        element_ptr, parent_wlr_scene_tree_ptr);
    // TODO(kaeser@gubbe.ch): Separate map method into set_visible/attach.
    wlr_scene_node_set_enabled(element_ptr->wlr_scene_node_ptr, true);
    BS_ASSERT(NULL != element_ptr->wlr_scene_node_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_unmap(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr->wlr_scene_node_ptr);
    // TODO(kaeser@gubbe.ch): Separate map method into set_visible/attach.
    wlr_scene_node_set_enabled(element_ptr->wlr_scene_node_ptr, false);
    wlr_scene_node_destroy(element_ptr->wlr_scene_node_ptr);
    element_ptr->wlr_scene_node_ptr = NULL;
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_map_unmap(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "map_unmap", test_map_unmap },
    { 0, NULL, NULL }
};

/** Reports whether the fake dtor was called. */
static bool test_destroy_cb_called;
/** dtor for the element under test. */
static void test_destroy_cb(wlmtk_element_t *element_ptr)
{
    test_destroy_cb_called = true;
    wlmtk_element_fini(element_ptr);
}
/** A dummy implementation for a 'create_scene_node'. */
static struct wlr_scene_node *test_create_scene_node(
    __UNUSED__ wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, NULL);
    return &wlr_scene_buffer_ptr->node;
}
/** Method table for the element we're using as test dummy. */
static const wlmtk_element_impl_t test_impl = {
    .destroy = test_destroy_cb,
    .create_scene_node = test_create_scene_node
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element, &test_impl));

    test_destroy_cb_called = false;
    wlmtk_element_destroy(&element);
    BS_TEST_VERIFY_TRUE(test_ptr, test_destroy_cb_called);

    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.impl_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests map and unmap, and that unmapping is done on reparenting or fini. */
void test_map_unmap(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element, &test_impl));

    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    wlmtk_container_t fake_parent = {
        .wlr_scene_tree_ptr = &wlr_scene_ptr->tree
    };
    wlmtk_element_set_parent_container(&element, &fake_parent);

    // Map & unmap.
    wlmtk_element_map(&element);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_mapped(&element));
    wlmtk_element_unmap(&element);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_mapped(&element));

    // Remain mapped, if the parent container remains unchanged.
    wlmtk_element_map(&element);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_mapped(&element));
    wlmtk_element_set_parent_container(&element, &fake_parent);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_mapped(&element));

    // Changing the parent (eg. to 'None') must unmap the element.
    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_mapped(&element));
    wlmtk_element_fini(&element);
}

/* == End of toolkit.c ===================================================== */
