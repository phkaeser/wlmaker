/* ========================================================================= */
/**
 * @file container.c
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

static void element_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_impl_t super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_impl_t *container_impl_ptr)
{
    BS_ASSERT(NULL != container_ptr);
    BS_ASSERT(NULL != container_impl_ptr);
    memset(container_ptr, 0, sizeof(wlmtk_container_t));

    if (!wlmtk_element_init(&container_ptr->super_element,
                            &super_element_impl)) {
        return false;
    }

    container_ptr->impl_ptr = container_impl_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_fini(wlmtk_container_t *container_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = container_ptr->elements.head_ptr)) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        wlmtk_container_remove_element(container_ptr, element_ptr);
        BS_ASSERT(container_ptr->elements.head_ptr != dlnode_ptr);

        wlmtk_element_destroy(element_ptr);
    }

    wlmtk_element_fini(&container_ptr->super_element);
    container_ptr->impl_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    bs_dllist_push_back(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    wlmtk_element_set_parent_container(element_ptr, container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_remove_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    wlmtk_element_set_parent_container(element_ptr, NULL);
    bs_dllist_remove(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_tree *wlmtk_container_wlr_scene_tree(
    wlmtk_container_t *container_ptr)
{
    return container_ptr->wlr_scene_tree_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::destroy method.
 *
 * @param element_ptr
 */
void element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    container_ptr->impl_ptr->destroy(container_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the scene graph API node.
 */
struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    BS_ASSERT(NULL == container_ptr->wlr_scene_tree_ptr);
    container_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr);
    BS_ASSERT(NULL != container_ptr->wlr_scene_tree_ptr);

    return &container_ptr->wlr_scene_tree_ptr->node;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_add_remove(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_container_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "add_remove", test_add_remove },
    { 0, NULL, NULL }
};

/** dtor for the container under test. */
static void test_destroy_cb(wlmtk_container_t *container_ptr)
{
    wlmtk_container_fini(container_ptr);
}

/** dtor for the element under test. */
static void test_element_destroy_cb(wlmtk_element_t *element_ptr)
{
    wlmtk_element_fini(element_ptr);
}

/** Method table for the container we're using for test. */
static const wlmtk_container_impl_t test_container_impl = {
    .destroy = test_destroy_cb
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(
                            &container, &test_container_impl));
    // Also expect the super element to be initialized.
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, container.super_element.impl_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, container.impl_ptr);

    wlmtk_container_destroy(&container);

    // Also expect the super element to be un-initialized.
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.super_element.impl_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.impl_ptr);
}

/* ------------------------------------------------------------------------- */
/** Exercises adding and removing elements, verifies destruction on fini. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(
                            &container, &test_container_impl));

    wlmtk_element_t element1, element2, element3;
    wlmtk_element_impl_t impl = { .destroy = test_element_destroy_cb };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element1, &impl));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element2, &impl));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element3, &impl));

    wlmtk_container_add_element(&container, &element1);
    BS_TEST_VERIFY_EQ(test_ptr, element1.parent_container_ptr, &container);
    wlmtk_container_add_element(&container, &element2);
    BS_TEST_VERIFY_EQ(test_ptr, element2.parent_container_ptr, &container);
    wlmtk_container_add_element(&container, &element3);
    BS_TEST_VERIFY_EQ(test_ptr, element3.parent_container_ptr, &container);

    wlmtk_container_remove_element(&container, &element2);
    BS_TEST_VERIFY_EQ(test_ptr, element2.parent_container_ptr, NULL);

    wlmtk_container_destroy(&container);
    BS_TEST_VERIFY_EQ(test_ptr, element1.parent_container_ptr, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, element3.parent_container_ptr, NULL);

    BS_TEST_VERIFY_EQ(test_ptr, element1.impl_ptr, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, element3.impl_ptr, NULL);
}

/* == End of container.c =================================================== */
