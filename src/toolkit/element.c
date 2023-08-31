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

#include "util.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void handle_wlr_scene_node_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

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
    // Verify we're no longer part of the scene graph, nor part of a container.
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
    if (element_ptr->parent_container_ptr == parent_container_ptr) return;
    element_ptr->parent_container_ptr = parent_container_ptr;
    wlmtk_element_attach_to_scene_graph(element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_attach_to_scene_graph(
    wlmtk_element_t *element_ptr)
{
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr = NULL;
    if (NULL != element_ptr->parent_container_ptr) {
        parent_wlr_scene_tree_ptr = wlmtk_container_wlr_scene_tree(
            element_ptr->parent_container_ptr);
    }

    if (NULL == parent_wlr_scene_tree_ptr) {
        if (NULL != element_ptr->wlr_scene_node_ptr) {
            wl_list_remove(&element_ptr->wlr_scene_node_destroy_listener.link);
            wlr_scene_node_destroy(element_ptr->wlr_scene_node_ptr);
            element_ptr->wlr_scene_node_ptr = NULL;
        }
        return;
    }

    if (NULL == element_ptr->wlr_scene_node_ptr) {
        element_ptr->wlr_scene_node_ptr = element_ptr->impl_ptr->create_scene_node(
            element_ptr, parent_wlr_scene_tree_ptr);
        wlmtk_util_connect_listener_signal(
            &element_ptr->wlr_scene_node_ptr->events.destroy,
            &element_ptr->wlr_scene_node_destroy_listener,
            handle_wlr_scene_node_destroy);
        wlr_scene_node_set_enabled(element_ptr->wlr_scene_node_ptr,
                                   element_ptr->visible);
        wlr_scene_node_set_position(element_ptr->wlr_scene_node_ptr,
                                    element_ptr->x,
                                    element_ptr->y);
        return;
    }

    BS_ASSERT(NULL != element_ptr->wlr_scene_node_ptr);
    if (element_ptr->wlr_scene_node_ptr->parent == parent_wlr_scene_tree_ptr) {
        // Parent does not change, nothing to do.
        return;
    }

    wlr_scene_node_reparent(element_ptr->wlr_scene_node_ptr,
                            parent_wlr_scene_tree_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_set_visible(wlmtk_element_t *element_ptr, bool visible)
{
    // Nothing to do?
    if (element_ptr->visible == visible) return;

    element_ptr->visible = visible;
    if (NULL != element_ptr->wlr_scene_node_ptr) {
        wlr_scene_node_set_enabled(element_ptr->wlr_scene_node_ptr, visible);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_get_position(
    wlmtk_element_t *element_ptr,
    int *x_ptr,
    int *y_ptr)
{
    if (NULL != x_ptr) *x_ptr = element_ptr->x;
    if (NULL != y_ptr) *y_ptr = element_ptr->y;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_set_position(
    wlmtk_element_t *element_ptr,
    int x,
    int y)
{
    element_ptr->x = x;
    element_ptr->y = y;

    if (NULL != element_ptr->wlr_scene_node_ptr) {
        wlr_scene_node_set_position(element_ptr->wlr_scene_node_ptr,
                                    element_ptr->x,
                                    element_ptr->y);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'destroy' callback of the wlr_scene_node.
 *
 * A call here indicates that teardown was not executed properly!
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_wlr_scene_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_element_t *element_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_element_t, wlr_scene_node_destroy_listener);
    bs_log(BS_FATAL, "Unexpected call into node's dtor on %p!", element_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_set_parent_container(bs_test_t *test_ptr);
static void test_set_get_position(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "set_parent_container", test_set_parent_container },
    { 1, "set_get_position", test_set_get_position },
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
/** Tests set_parent_container, and that scene graph follows. */
void test_set_parent_container(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element, &test_impl));

    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    wlmtk_container_t fake_parent = {};
    struct wlr_scene *other_wlr_scene_ptr = wlr_scene_create();
    wlmtk_container_t other_fake_parent = {
        .wlr_scene_tree_ptr = &other_wlr_scene_ptr->tree
    };

    // Setting a parent without a scene graph tree will not set a node.
    wlmtk_element_set_parent_container(&element, &fake_parent);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    // Setting a parent with a tree must create & attach the node there.
    wlmtk_element_set_visible(&element, true);
    fake_parent.wlr_scene_tree_ptr = &wlr_scene_ptr->tree;
    wlmtk_element_set_parent_container(&element, &fake_parent);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &wlr_scene_ptr->tree,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Resetting the parent must also re-attach the node.
    wlmtk_element_set_parent_container(&element, &other_fake_parent);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &other_wlr_scene_ptr->tree,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Clearing the parent most remove the node.
    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_element_fini(&element);
}

/* ------------------------------------------------------------------------- */
/** Tests get_position and set_position, and that scene graph follows. */
void test_set_get_position(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element, &test_impl));

    // Exercise, must not crash.
    wlmtk_element_get_position(&element, NULL, NULL);

    int x, y;
    wlmtk_element_get_position(&element, &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y);

    wlmtk_element_set_position(&element, 10, 20);
    wlmtk_element_get_position(&element, &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 10, x);
    BS_TEST_VERIFY_EQ(test_ptr, 20, y);

    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    wlmtk_container_t fake_parent = {
        .wlr_scene_tree_ptr = &wlr_scene_ptr->tree
    };
    wlmtk_element_set_parent_container(&element, &fake_parent);

    BS_TEST_VERIFY_EQ(test_ptr, 10, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 20, element.wlr_scene_node_ptr->y);

    wlmtk_element_set_position(&element, 30, 40);
    BS_TEST_VERIFY_EQ(test_ptr, 30, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 40, element.wlr_scene_node_ptr->y);

    wlmtk_element_set_parent_container(&element, NULL);
    wlmtk_element_fini(&element);
}

/* == End of toolkit.c ===================================================== */
