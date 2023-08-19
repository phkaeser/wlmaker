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
    memset(element_ptr, 0, sizeof(wlmtk_element_t));

    element_ptr->impl_ptr = element_impl_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr)
{
    // Verify we're no longer mapped, nor part of a container.
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
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
    element_ptr->parent_container_ptr = parent_container_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_map(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);

    element_ptr->wlr_scene_node_ptr = element_ptr->impl_ptr->create_scene_node(
        element_ptr,
        wlmtk_container_wlr_scene_tree(element_ptr->parent_container_ptr));
    BS_ASSERT(NULL != element_ptr->wlr_scene_node_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_unmap(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr->wlr_scene_node_ptr);
    wlr_scene_node_destroy(element_ptr->wlr_scene_node_ptr);
    element_ptr->wlr_scene_node_ptr = NULL;
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 1, "init_fini", test_init_fini },
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

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    wlmtk_element_impl_t impl = { .destroy = test_destroy_cb };

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element, &impl));

    test_destroy_cb_called = false;
    wlmtk_element_destroy(&element);
    BS_TEST_VERIFY_TRUE(test_ptr, test_destroy_cb_called);

    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.impl_ptr);
}

/* == End of toolkit.c ===================================================== */
