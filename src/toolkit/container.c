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
static void element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static wlmtk_element_t *element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static void element_pointer_leave(
    wlmtk_element_t *element_ptr);

static void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);
static wlmtk_element_t *update_pointer_focus_at(
    wlmtk_container_t *container_ptr,
    double x,
    double y,
    uint32_t time_msec);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_impl_t super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
    .get_pointer_area = element_get_pointer_area,
    .pointer_motion = element_pointer_motion,
    .pointer_leave = element_pointer_leave
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

    bs_dllist_push_front(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    wlmtk_element_set_parent_container(element_ptr, container_ptr);

    // Need to re-compute pointer focus, since we might have added an element
    // below the current cursor position.
    wlmtk_container_pointer_refocus_tree(container_ptr);
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

    // We can be more lenient in asking for re-focus: If the removed element
    // is NOT having pointer focus, we won't have to bother.
    if (element_ptr == container_ptr->pointer_focus_element_ptr) {
        wlmtk_container_pointer_refocus_tree(container_ptr);
    }
    BS_ASSERT(element_ptr != container_ptr->pointer_focus_element_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_tree *wlmtk_container_wlr_scene_tree(
    wlmtk_container_t *container_ptr)
{
    return container_ptr->wlr_scene_tree_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_pointer_refocus_tree(wlmtk_container_t *container_ptr)
{
    // Guard clause: Don't throw over if there's no container.
    if (NULL == container_ptr) return;

    while (NULL != container_ptr->super_element.parent_container_ptr) {
        container_ptr = container_ptr->super_element.parent_container_ptr;
    }

    update_pointer_focus_at(
        container_ptr,
        container_ptr->super_element.last_pointer_x,
        container_ptr->super_element.last_pointer_y,
        container_ptr->super_element.last_pointer_time_msec);
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

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_dimensions(element_ptr, &x1, &y1, &x2, &y2);
        left = BS_MIN(left, x_pos + x1);
        top = BS_MIN(top, y_pos + y1);
        right = BS_MAX(right, x_pos + x2);
        bottom = BS_MAX(bottom, y_pos + y2);
    }

    if (NULL != left_ptr) *left_ptr = left;
    if (NULL != top_ptr) *top_ptr = top;
    if (NULL != right_ptr) *right_ptr = right;
    if (NULL != bottom_ptr) *bottom_ptr = bottom;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the minimal rectangle covering all element's pointer areas.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void element_get_pointer_area(
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

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_pointer_area(element_ptr, &x1, &y1, &x2, &y2);
        left = BS_MIN(left, x_pos + x1);
        top = BS_MIN(top, y_pos + y1);
        right = BS_MAX(right, x_pos + x2);
        bottom = BS_MAX(bottom, y_pos + y2);
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
wlmtk_element_t *element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    return update_pointer_focus_at(container_ptr, x, y, time_msec);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's leave method: Forwards it to the element
 * currently having pointer focus, and clears that.
 *
 * @param element_ptr
 */
void element_pointer_leave(wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    if (NULL == container_ptr->pointer_focus_element_ptr) return;

    wlmtk_element_pointer_leave(container_ptr->pointer_focus_element_ptr);
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
 * Updates pointer focus of container for position (x, y).
 *
 * Updates wlmtk_container_t::pointer_focus_element_ptr.
 *
 * @param container_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return The leave element at position (x, y).
 */
wlmtk_element_t *update_pointer_focus_at(
    wlmtk_container_t *container_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        if (!element_ptr->visible) continue;

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_pointer_area(element_ptr, &x1, &y1, &x2, &y2);
        if (x_pos + x1 <= x && x < x_pos + x2 &&
            y_pos + y1 <= y && y < y_pos + y2) {
            wlmtk_element_t *motion_element_ptr = wlmtk_element_pointer_motion(
                element_ptr, x - x_pos, y - y_pos, time_msec);
            if (NULL == motion_element_ptr) continue;

            if (NULL != container_ptr->pointer_focus_element_ptr &&
                container_ptr->pointer_focus_element_ptr != element_ptr) {
                wlmtk_element_pointer_leave(
                    container_ptr->pointer_focus_element_ptr);
            }
            container_ptr->pointer_focus_element_ptr = element_ptr;
            return motion_element_ptr;
        }
    }

    // Getting here implies we didn't have an element catching the motion,
    // so it must have happened outside our araea. We also should free
    // pointer focus element now.
    if (NULL != container_ptr->pointer_focus_element_ptr) {
        wlmtk_element_pointer_leave(container_ptr->pointer_focus_element_ptr);
        container_ptr->pointer_focus_element_ptr = NULL;
    }
    return NULL;
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
static void test_pointer_motion(bs_test_t *test_ptr);
static void test_pointer_focus(bs_test_t *test_ptr);
static void test_pointer_focus_layered(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_container_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "add_remove", test_add_remove },
    { 1, "add_remove_with_scene_graph", test_add_remove_with_scene_graph },
    { 1, "pointer_motion", test_pointer_motion },
    { 1, "pointer_focus", test_pointer_focus },
    { 1, "pointer_focus_layered", test_pointer_focus_layered },
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
void test_pointer_motion(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, &wlmtk_container_fake_impl));
    wlmtk_element_set_visible(&container.super_element, true);

    // Note: pointer area extends by (-1, -2, 3, 4) on each fake element.
    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem1_ptr->element, -20, -40);
    elem1_ptr->width = 10;
    elem1_ptr->height = 5;
    elem1_ptr->pointer_motion_return_value = &elem1_ptr->element;
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 100, 200);
    elem2_ptr->width = 10;
    elem2_ptr->height = 5;
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    elem2_ptr->pointer_motion_return_value = &elem2_ptr->element;
    wlmtk_container_add_element(&container, &elem2_ptr->element);

    // Verify 'dimensions' and 'pointer_area', derived from children.
    int l, t, r, b;
    wlmtk_element_get_dimensions(&container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);

    wlmtk_element_get_pointer_area(&container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -21, l);
    BS_TEST_VERIFY_EQ(test_ptr, -42, t);
    BS_TEST_VERIFY_EQ(test_ptr, 113, r);
    BS_TEST_VERIFY_EQ(test_ptr, 209, b);

    // Same must hold for the parent container.
    wlmtk_container_t parent_container;
    BS_ASSERT(wlmtk_container_init(&parent_container,
                                   &wlmtk_container_fake_impl));
    wlmtk_container_add_element(&parent_container,
                                &container.super_element);

    wlmtk_element_get_dimensions(
        &parent_container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);

    wlmtk_element_get_pointer_area(
        &parent_container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -21, l);
    BS_TEST_VERIFY_EQ(test_ptr, -42, t);
    BS_TEST_VERIFY_EQ(test_ptr, 113, r);
    BS_TEST_VERIFY_EQ(test_ptr, 209, b);

    // There's nothing at (0, 0).
    wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);

    wlmtk_element_pointer_motion(&parent_container.super_element, 0, 0, 7);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);

    // elem1 is at (-20, -40).
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_pointer_motion(&container.super_element, -20, -40, 7));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_motion_called);
    elem1_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_y);

    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, -20, -40, 7));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_motion_called);
    elem1_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_y);

    // elem2 is covering the area at (107, 302).
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 107, 203, 7));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_leave_called);
    elem1_ptr->pointer_leave_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_motion_called);
    elem2_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 7, elem2_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 3, elem2_ptr->element.last_pointer_y);

    // The pointer area of elem2 is covering the area at (112, 208).
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 112, 208, 7));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_leave_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_motion_called);
    elem2_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 12, elem2_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 8, elem2_ptr->element.last_pointer_y);

    // The pointer area of elem2 does not include (113, 209).
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 113, 209, 7));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);
    elem2_ptr->pointer_leave_called = false;

    // All set. clean it up.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);

    wlmtk_container_remove_element(&parent_container,
                                   &container.super_element);
    wlmtk_container_fini(&parent_container);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer focus is updated when elements are updated. */
void test_pointer_focus(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, &wlmtk_container_fake_impl));

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    elem1_ptr->pointer_motion_return_value = &elem1_ptr->element;
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    elem2_ptr->pointer_motion_return_value = &elem2_ptr->element;
    wlmtk_element_set_visible(&elem2_ptr->element, true);

    // Case 1: An empty container, will not have a pointer-focussed element.
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.pointer_focus_element_ptr);

    // Case 2: Adding a visible element at (0, 0): Focus remains NULL, since
    // motion() was not called yet and we don't have known pointer position.
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.pointer_focus_element_ptr);
    wlmtk_container_remove_element(&container, &elem1_ptr->element);

    // Case 3: Call motion() first, then add a visible element at (0, 0). Focus
    // should switch there.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7));
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 4: Add another visible element. Focus changes, since in front.
    wlmtk_container_add_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 5: Elem2 (added last = in front) becomes invisible. Focus changes.
    wlmtk_element_set_visible(&elem2_ptr->element, false);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 6: Elem1 becomes invisible. Focus changes to NULL.
    wlmtk_element_set_visible(&elem1_ptr->element, false);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);

    // Case 7: Elem1 becomes visible. Focus changes to elem1 again.
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 8: Remove Elem1. Focus changes to NULL.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);

    // Case 9: Elem2 becomes visible, focus changes there.
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 10: Elem2 is removed. Focus is now NULL, and leave() is called for
    // the element that was removed.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        elem2_ptr->pointer_leave_called);

    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer focus is updated across layers of containers. */
void test_pointer_focus_layered(bs_test_t *test_ptr)
{
    wlmtk_container_t container1;
    BS_ASSERT(wlmtk_container_init(&container1, &wlmtk_container_fake_impl));
    wlmtk_container_t container2;
    BS_ASSERT(wlmtk_container_init(&container2, &wlmtk_container_fake_impl));
    wlmtk_element_set_visible(&container2.super_element, true);

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    elem1_ptr->pointer_motion_return_value = &elem1_ptr->element;
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    elem2_ptr->pointer_motion_return_value = &elem2_ptr->element;
    wlmtk_element_set_visible(&elem2_ptr->element, true);

    // Prepare: Motion was called, will not have any focus.
    wlmtk_element_pointer_motion(&container1.super_element, 0, 0, 7);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container1.pointer_focus_element_ptr);

    // Case 1: Add element 2 to second container, then add this container.
    // this must re-trigger focus and pass it to elem2.
    wlmtk_container_add_element(&container2, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container1.pointer_focus_element_ptr);
    wlmtk_container_add_element(&container1, &container2.super_element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container2.super_element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element, container2.pointer_focus_element_ptr);

    // Case 2: Add elem1 to container1. Must change focus there, and call
    // leave for container2 and elem2.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_add_element(&container1, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);

    // Case 3: Bring container2 in front. Now elem2 has focus.
    elem1_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container1, &container2.super_element);
    wlmtk_container_add_element(&container1, &container2.super_element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container2.super_element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container2.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_leave_called);

    // Case 4: Remove elem2, drop focus back to elem1.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container2, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);

    wlmtk_container_remove_element(&container1, &elem1_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);

    wlmtk_container_remove_element(&container1, &container2.super_element);
    wlmtk_container_fini(&container2);
    wlmtk_container_fini(&container1);
}

/* == End of container.c =================================================== */
