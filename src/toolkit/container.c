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

#include "container.h"

#include "util.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void element_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static wlmtk_element_t *element_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static void element_leave(
    wlmtk_element_t *element_ptr);
static void element_get_bounding_box(
    wlmtk_element_t *element_ptr,
    int *x_from_ptr, int *y_from_ptr, int *x_to_ptr, int *y_to_ptr);

static void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_impl_t super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
    .motion = element_motion,
    .leave = element_leave
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_impl_t *container_impl_ptr)
{
    BS_ASSERT(NULL != container_ptr);
    BS_ASSERT(NULL != container_impl_ptr);
    BS_ASSERT(NULL != container_impl_ptr->destroy);
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
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);

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
    BS_ASSERT(element_ptr->parent_container_ptr == container_ptr);

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
 * Creates the wlroots scene graph tree for the container, and will attach all
 * already-contained elements to the scene graph, as well.
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

    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    wlmtk_util_connect_listener_signal(
        &container_ptr->wlr_scene_tree_ptr->node.events.destroy,
        &container_ptr->wlr_scene_tree_node_destroy_listener,
        handle_wlr_scene_tree_node_destroy);
    return &container_ptr->wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    int left = 0, top = 0, right = 0, bottom = 0;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        int x_from, y_from, x_to, y_to;
        element_get_bounding_box(element_ptr, &x_from, &y_from, &x_to, &y_to);
        left = BS_MIN(left, x_from);
        top = BS_MIN(top, y_from);
        right = BS_MAX(right, x_to);
        bottom = BS_MAX(bottom, y_to);
    }

    if (NULL != left_ptr) *left_ptr = left;
    if (NULL != top_ptr) *top_ptr = top;
    if (NULL != right_ptr) *right_ptr = right;
    if (NULL != bottom_ptr) *bottom_ptr = bottom;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's motion method: Handle pointer moves.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Pointer to the (non-container) element handling the motion, or NULL
 *     if the motion wasn't handled.
 */
wlmtk_element_t *element_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        int x_from, y_from, x_to, y_to;
        element_get_bounding_box(element_ptr, &x_from, &y_from, &x_to, &y_to);
        if (x_from <= x && x < x_to && y_from <= y && y < y_to) {
            int x_pos, y_pos;
            wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
            wlmtk_element_t *motion_element_ptr = wlmtk_element_motion(
                element_ptr, x - x_pos, y - y_pos, time_msec);
            if (NULL == motion_element_ptr) continue;

            if (NULL != container_ptr->pointer_focus_element_ptr) {
                wlmtk_element_leave(container_ptr->pointer_focus_element_ptr);
            }
            container_ptr->pointer_focus_element_ptr = element_ptr;
            return motion_element_ptr;
        }
    }

    // Getting here implies we didn't have an element catching the motion,
    // so it must have happened outside our araea. We also should free
    // pointer focus element now.
    if (NULL != container_ptr->pointer_focus_element_ptr) {
        wlmtk_element_leave(container_ptr->pointer_focus_element_ptr);
        container_ptr->pointer_focus_element_ptr = NULL;
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's leave method: Forwards it to the element
 * currently having pointer focus, and clears that.
 *
 * @param element_ptr
 */
void element_leave(wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    if (NULL == container_ptr->pointer_focus_element_ptr) return;

    wlmtk_element_leave(container_ptr->pointer_focus_element_ptr);
    container_ptr->pointer_focus_element_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'destroy' callback of wlr_scene_tree_ptr->node.
 *
 * Will also detach (but not destroy) each of the still-contained elements.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_container_t, wlr_scene_tree_node_destroy_listener);

    container_ptr->wlr_scene_tree_ptr = NULL;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    // Since this is a callback from the tree node dtor, the tree is going to
    // be destroyed. We are using this to reset the container's reference.
    wl_list_remove(&container_ptr->wlr_scene_tree_node_destroy_listener.link);
}

/* ------------------------------------------------------------------------- */
/**
 * Computes the bounding box position for the element, relative to parent.
 *
 * The element (or the element's sub-elements) will all be contained in the
 * area spanned by [*x_from_ptr, *x_to_ptr), [*y_from_ptr, *y_to_ptr).
 *
 * @param element_ptr
 * @param x_from_ptr           Minimum horizontal position (inclusive).
 * @param y_from_ptr           Minimum vertical position (inclusive).
 * @param x_to_ptr             Maximum horizontal position (exclusive).
 * @param y_to_ptr             Maximum vertical position (exclusive).
 */
void element_get_bounding_box(
    wlmtk_element_t *element_ptr,
    int *x_from_ptr, int *y_from_ptr, int *x_to_ptr, int *y_to_ptr)
{
    int x_pos, y_pos;
    wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);

    wlmtk_element_get_dimensions(
        element_ptr, x_from_ptr, y_from_ptr, x_to_ptr, y_to_ptr);
    if (NULL != x_from_ptr) *x_from_ptr += x_pos;
    if (NULL != x_to_ptr) *x_to_ptr += x_pos;
    if (NULL != y_from_ptr) *y_from_ptr += y_pos;
    if (NULL != y_to_ptr) *y_to_ptr += y_pos;
}

/* == Helper for unit test: A fake container =============================== */

static void fake_destroy(wlmtk_container_t *container_ptr);

/** Method table for the container we're using for test. */
const wlmtk_container_impl_t wlmtk_container_fake_impl = {
    .destroy = fake_destroy
};

/* ------------------------------------------------------------------------- */
/** dtor for the container under test. */
void fake_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_container_fini(container_ptr);
}

/* == Helper for unit tests: A fake container with a tree, as parent ======= */

/** State of the "fake" parent container. Refers to a scene graph. */
typedef struct {
    /** The actual container */
    wlmtk_container_t         container;
    /** A scene graph. Not attached to any output, */
    struct wlr_scene          *wlr_scene_ptr;
} fake_parent_container_t;

static void fake_parent_destroy(wlmtk_container_t *container_ptr);

/* ------------------------------------------------------------------------- */
wlmtk_container_t *wlmtk_container_create_fake_parent(void)
{
    static const wlmtk_container_impl_t fake_parent_impl = {
        .destroy = fake_parent_destroy
    };

    fake_parent_container_t *fake_parent_container_ptr = logged_calloc(
        1, sizeof(fake_parent_container_t));
    if (NULL == fake_parent_container_ptr) return NULL;

    if (!wlmtk_container_init(
        &fake_parent_container_ptr->container,
        &fake_parent_impl)) {
        fake_parent_destroy(&fake_parent_container_ptr->container);
        return NULL;
    }

    fake_parent_container_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == fake_parent_container_ptr->wlr_scene_ptr) {
        fake_parent_destroy(&fake_parent_container_ptr->container);
        return NULL;
    }
    fake_parent_container_ptr->container.wlr_scene_tree_ptr =
        &fake_parent_container_ptr->wlr_scene_ptr->tree;

    return &fake_parent_container_ptr->container;
}

/* ------------------------------------------------------------------------- */
/** Destructor for the "fake" parent, to be used for tests. */
void fake_parent_destroy(wlmtk_container_t *container_ptr)
{
    fake_parent_container_t *fake_parent_container_ptr = BS_CONTAINER_OF(
        container_ptr, fake_parent_container_t, container);

    if (NULL != fake_parent_container_ptr->wlr_scene_ptr) {
        // Note: There is no "wlr_scene_destroy()" method; as of 2023-09-02,
        // this is just a flat allocated struct.
        free(fake_parent_container_ptr->wlr_scene_ptr);
        fake_parent_container_ptr->wlr_scene_ptr = NULL;
    }

    wlmtk_container_fini(&fake_parent_container_ptr->container);

    free(fake_parent_container_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_add_remove(bs_test_t *test_ptr);
static void test_add_remove_with_scene_graph(bs_test_t *test_ptr);
static void test_motion(bs_test_t *test_ptr);
static void test_motion_nested(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_container_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "add_remove", test_add_remove },
    { 1, "add_remove_with_scene_graph", test_add_remove_with_scene_graph },
    { 1, "motion", test_motion },
    { 1, "motion_nested", test_motion_nested },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(
                            &container, &wlmtk_container_fake_impl));
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
                            &container, &wlmtk_container_fake_impl));

    wlmtk_fake_element_t *elem1_ptr, *elem2_ptr, *elem3_ptr;
    elem1_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem1_ptr);
    elem2_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem2_ptr);
    elem3_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem3_ptr);

    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, elem1_ptr->element.parent_container_ptr, &container);
    wlmtk_container_add_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, elem2_ptr->element.parent_container_ptr, &container);
    wlmtk_container_add_element(&container, &elem3_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, elem3_ptr->element.parent_container_ptr, &container);

    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, elem2_ptr->element.parent_container_ptr);
    wlmtk_element_destroy(&elem2_ptr->element);

    // Will destroy contained elements.
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that elements are attached, resp. detached from scene graph. */
void test_add_remove_with_scene_graph(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(
                            &container, &wlmtk_container_fake_impl));

    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_parent_ptr);

    wlmtk_element_set_parent_container(
        &container.super_element, fake_parent_ptr);

    // Want to have the node.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, container.super_element.wlr_scene_node_ptr);

    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_init(&element, &wlmtk_fake_element_impl));

    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);
    wlmtk_container_add_element(&container, &element);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_container_remove_element(&container, &element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_element_set_parent_container(&container.super_element, NULL);
    wlmtk_container_fini(&container);

    wlmtk_container_destroy(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the 'motion' method for container. */
void test_motion(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, &wlmtk_container_fake_impl));

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem1_ptr->element, -20, -40);
    elem1_ptr->width = 10;
    elem1_ptr->height = 5;
    elem1_ptr->motion_return_value = &elem1_ptr->element;
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 100, 200);
    elem2_ptr->width = 10;
    elem2_ptr->height = 5;
    elem2_ptr->motion_return_value = &elem2_ptr->element;
    wlmtk_container_add_element(&container, &elem2_ptr->element);

    wlmtk_element_motion(&container.super_element, 0, 0, 1234);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);

    elem1_ptr->motion_x = 42;
    elem1_ptr->motion_y = 42;
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, -20, -40, 1234));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->motion_called);
    elem1_ptr->motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_y);

    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, 107, 203, 1234));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->leave_called);
    elem1_ptr->leave_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->motion_called);
    elem2_ptr->motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 7, elem2_ptr->motion_x);
    BS_TEST_VERIFY_EQ(test_ptr, 3, elem2_ptr->motion_y);

    BS_TEST_VERIFY_EQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, 110, 205, 1234));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->leave_called);
    elem2_ptr->leave_called = false;

    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests the 'motion' method for container. */
void test_motion_nested(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, &wlmtk_container_fake_impl));

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem1_ptr->element, -20, -40);
    elem1_ptr->width = 10;
    elem1_ptr->height = 5;
    elem1_ptr->motion_return_value = &elem1_ptr->element;
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 100, 200);
    elem2_ptr->width = 10;
    elem2_ptr->height = 5;
    elem2_ptr->motion_return_value = &elem2_ptr->element;
    wlmtk_container_add_element(&container, &elem2_ptr->element);

    int l, t, r, b;
    wlmtk_element_get_dimensions(&container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);

    wlmtk_container_t parent_container;
    BS_ASSERT(wlmtk_container_init(&parent_container,
                                   &wlmtk_container_fake_impl));
    wlmtk_container_add_element(&parent_container,
                                &container.super_element);

    wlmtk_element_get_dimensions(&parent_container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);


    wlmtk_element_motion(&container.super_element, 0, 0, 1234);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);

    elem1_ptr->motion_x = 42;
    elem1_ptr->motion_y = 42;
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, -20, -40, 1234));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->motion_called);
    elem1_ptr->motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_y);

    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_motion(&parent_container.super_element, -20, -40, 1234));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->motion_called);
    elem1_ptr->motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->motion_y);


    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, 107, 203, 1234));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->leave_called);
    elem1_ptr->leave_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->motion_called);
    elem2_ptr->motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 7, elem2_ptr->motion_x);
    BS_TEST_VERIFY_EQ(test_ptr, 3, elem2_ptr->motion_y);

    BS_TEST_VERIFY_EQ(
        test_ptr, NULL,
        wlmtk_element_motion(&container.super_element, 110, 205, 1234));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->leave_called);
    elem2_ptr->leave_called = false;

    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);

    wlmtk_container_remove_element(&parent_container,
                                   &container.super_element);
    wlmtk_container_fini(&parent_container);
    wlmtk_container_fini(&container);
}

/*
 * TODO(kaeser@gubbe.ch): Extend motion test to verify a search is continued
 * if an element in the area returns NULL. Also verify it continues if the
 * element is not visible.
 */

/* == End of container.c =================================================== */
