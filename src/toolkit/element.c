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

/* ------------------------------------------------------------------------- */
void wlmtk_element_get_size(
    wlmtk_element_t *element_ptr,
    int *width_ptr,
    int *height_ptr)
{
    if (NULL != width_ptr) *width_ptr = element_ptr->width;
    if (NULL != height_ptr) *height_ptr = element_ptr->height;
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

/* == Fake element, useful for unit tests. ================================= */

static void fake_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *fake_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void fake_motion(
    wlmtk_element_t *element_ptr,
    int x, int y);

const wlmtk_element_impl_t wlmtk_fake_element_impl = {
    .destroy = fake_destroy,
    .create_scene_node = fake_create_scene_node,
    .motion = fake_motion
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_element_t *wlmtk_fake_element_create(void)
{
    wlmtk_fake_element_t *fake_element_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_element_t));
    if (NULL == fake_element_ptr) return NULL;

    if (!wlmtk_element_init(&fake_element_ptr->element,
                            &wlmtk_fake_element_impl)) {
        fake_destroy(&fake_element_ptr->element);
        return NULL;
    }

    return fake_element_ptr;
}

/* ------------------------------------------------------------------------- */
/** dtor for the "fake" element used for tests. */
void fake_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    wlmtk_element_fini(&fake_element_ptr->element);
    free(fake_element_ptr);
}

/* ------------------------------------------------------------------------- */
/** A "fake" 'create_scene_node': Creates a non-attached buffer node. */
struct wlr_scene_node *fake_create_scene_node(
    __UNUSED__ wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, NULL);
    return &wlr_scene_buffer_ptr->node;
}

/* ------------------------------------------------------------------------- */
/** Handles 'tnter' events for the fake element. */
void fake_motion(
    wlmtk_element_t *element_ptr,
    int x,
    int y)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->motion_called = true;
    fake_element_ptr->motion_x = x;
    fake_element_ptr->motion_y = y;
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

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_init(&element, &wlmtk_fake_element_impl));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, element.impl_ptr);

    wlmtk_element_fini(&element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.impl_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests set_parent_container, and that scene graph follows. */
void test_set_parent_container(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_init(&element, &wlmtk_fake_element_impl));

    // Setting a parent without a scene graph tree will not set a node.
    wlmtk_container_t parent_no_tree;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_container_init(&parent_no_tree, &wlmtk_container_fake_impl));
    wlmtk_element_set_parent_container(&element, &parent_no_tree);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);
    wlmtk_container_fini(&parent_no_tree);

    // Setting a parent with a tree must create & attach the node there.
    wlmtk_container_t *parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != parent_ptr);
    wlmtk_element_set_visible(&element, true);
    wlmtk_element_set_parent_container(&element, parent_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        parent_ptr->wlr_scene_tree_ptr,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Resetting the parent must also re-attach the node.
    wlmtk_container_t *other_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != other_parent_ptr);
    wlmtk_element_set_parent_container(&element, other_parent_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        other_parent_ptr->wlr_scene_tree_ptr,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Clearing the parent most remove the node.
    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_container_destroy(other_parent_ptr);
    wlmtk_container_destroy(parent_ptr);
    wlmtk_element_fini(&element);
}

/* ------------------------------------------------------------------------- */
/** Tests get_position and set_position, and that scene graph follows. */
void test_set_get_position(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_init(&element, &wlmtk_fake_element_impl));

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

    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_element_set_parent_container(&element, fake_parent_ptr);

    BS_TEST_VERIFY_EQ(test_ptr, 10, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 20, element.wlr_scene_node_ptr->y);

    wlmtk_element_set_position(&element, 30, 40);
    BS_TEST_VERIFY_EQ(test_ptr, 30, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 40, element.wlr_scene_node_ptr->y);

    wlmtk_element_set_parent_container(&element, NULL);
    wlmtk_element_fini(&element);
    wlmtk_container_destroy(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests get_size. */
void test_get_size(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_init(&element, &wlmtk_fake_element_impl));
    element.width = 42;
    element.height = 21;

    // Must not crash.
    wlmtk_element_get_size(&element, NULL, NULL);

    int width, height;
    wlmtk_element_get_size(&element, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 42, width);
    BS_TEST_VERIFY_EQ(test_ptr, 21, height);
}

/* == End of toolkit.c ===================================================== */
