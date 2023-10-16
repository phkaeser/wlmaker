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

#include "element.h"
#include "container.h"

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
    BS_ASSERT(NULL != element_impl_ptr->get_dimensions);
    memset(element_ptr, 0, sizeof(wlmtk_element_t));

    element_ptr->impl_ptr = element_impl_ptr;

    element_ptr->last_pointer_x = NAN;
    element_ptr->last_pointer_y = NAN;
    element_ptr->last_pointer_time_msec = 0;
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

    // FIXME: If visibility changed, update parent container's pointer focus!
    element_ptr->visible = visible;
    if (NULL != element_ptr->wlr_scene_node_ptr) {
        wlr_scene_node_set_enabled(element_ptr->wlr_scene_node_ptr, visible);
    }

    if (NULL != element_ptr->parent_container_ptr) {
        wlmtk_container_update_pointer_focus(element_ptr->parent_container_ptr);
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
void wlmtk_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    element_ptr->impl_ptr->get_dimensions(
        element_ptr, left_ptr, top_ptr, right_ptr, bottom_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    if (NULL != element_ptr->impl_ptr->get_pointer_area) {
        element_ptr->impl_ptr->get_pointer_area(
            element_ptr, left_ptr, top_ptr, right_ptr, bottom_ptr);
    } else {
        wlmtk_element_get_dimensions(
            element_ptr, left_ptr, top_ptr, right_ptr, bottom_ptr);
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    element_ptr->last_pointer_x = x;
    element_ptr->last_pointer_y = y;
    element_ptr->last_pointer_time_msec = time_msec;

    // Guard clause: No implementation for `pointer_motion`.
    if (NULL == element_ptr->impl_ptr->pointer_motion) return NULL;

    return  element_ptr->impl_ptr->pointer_motion(
        element_ptr, x, y, time_msec);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_pointer_leave(
    wlmtk_element_t *element_ptr)
{
    if (NULL != element_ptr->impl_ptr->pointer_leave) {
        element_ptr->impl_ptr->pointer_leave(element_ptr);
    }
    element_ptr->last_pointer_x = NAN;
    element_ptr->last_pointer_y = NAN;
    element_ptr->last_pointer_time_msec = 0;
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
static void fake_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static void fake_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static wlmtk_element_t *fake_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool fake_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void fake_pointer_leave(
    wlmtk_element_t *element_ptr);

const wlmtk_element_impl_t wlmtk_fake_element_impl = {
    .destroy = fake_destroy,
    .create_scene_node = fake_create_scene_node,
    .get_dimensions = fake_get_dimensions,
    .get_pointer_area = fake_get_pointer_area,
    .pointer_motion = fake_pointer_motion,
    .pointer_button = fake_pointer_button,
    .pointer_leave = fake_pointer_leave,
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
/** A "fake" 'get_dimensions'. */
void fake_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    if (NULL != left_ptr) *left_ptr = 0;
    if (NULL != top_ptr) *top_ptr = 0;
    if (NULL != right_ptr) *right_ptr = fake_element_ptr->width;
    if (NULL != bottom_ptr) *bottom_ptr = fake_element_ptr->height;
}

/* ------------------------------------------------------------------------- */
/** A "fake" 'get_pointer_area'. */
void fake_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    if (NULL != left_ptr) *left_ptr = -1;
    if (NULL != top_ptr) *top_ptr = -2;
    if (NULL != right_ptr) *right_ptr = fake_element_ptr->width + 3;
    if (NULL != bottom_ptr) *bottom_ptr = fake_element_ptr->height + 4;
}

/* ------------------------------------------------------------------------- */
/** Handles 'motion' events for the fake element. */
wlmtk_element_t *fake_pointer_motion(
    wlmtk_element_t *element_ptr,
    __UNUSED__ double x,
    __UNUSED__ double y,
    __UNUSED__ uint32_t time_msec)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->pointer_motion_called = true;
    return fake_element_ptr->pointer_motion_return_value;
}

/* ------------------------------------------------------------------------- */
/** Handles 'button' events for the fake element. */
bool fake_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);

    fake_element_ptr->pointer_button_called = true;
    memcpy(
        &fake_element_ptr->pointer_button_event,
        button_event_ptr,
        sizeof(wlmtk_button_event_t));
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles 'leave' events for the fake element. */
void fake_pointer_leave(
    wlmtk_element_t *element_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->pointer_leave_called = true;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_set_parent_container(bs_test_t *test_ptr);
static void test_set_get_position(bs_test_t *test_ptr);
static void test_get_dimensions(bs_test_t *test_ptr);
static void test_get_pointer_area(bs_test_t *test_ptr);
static void test_pointer_motion_leave(bs_test_t *test_ptr);
static void test_pointer_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "set_parent_container", test_set_parent_container },
    { 1, "set_get_position", test_set_get_position },
    { 1, "get_dimensions", test_get_dimensions },
    { 1, "get_pointer_area", test_get_pointer_area },
    { 1, "pointer_motion_leave", test_pointer_motion_leave },
    { 1, "pointer_button", test_pointer_button },
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
/** Tests get_dimensions. */
void test_get_dimensions(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    fake_element_ptr->width = 42;
    fake_element_ptr->height = 21;

    // Must not crash.
    wlmtk_element_get_dimensions(
        &fake_element_ptr->element, NULL, NULL, NULL, NULL);

    int top, left, right, bottom;
    wlmtk_element_get_dimensions(
        &fake_element_ptr->element, &top, &left, &right, &bottom);
    BS_TEST_VERIFY_EQ(test_ptr, 0, top);
    BS_TEST_VERIFY_EQ(test_ptr, 0, left);
    BS_TEST_VERIFY_EQ(test_ptr, 42, right);
    BS_TEST_VERIFY_EQ(test_ptr, 21, bottom);
}

/* ------------------------------------------------------------------------- */
/** Tests get_dimensions. */
void test_get_pointer_area(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    fake_element_ptr->width = 42;
    fake_element_ptr->height = 21;

    // Must not crash.
    wlmtk_element_get_pointer_area(
        &fake_element_ptr->element, NULL, NULL, NULL, NULL);

    int top, left, right, bottom;
    wlmtk_element_get_pointer_area(
        &fake_element_ptr->element, &top, &left, &right, &bottom);
    BS_TEST_VERIFY_EQ(test_ptr, -1, top);
    BS_TEST_VERIFY_EQ(test_ptr, -2, left);
    BS_TEST_VERIFY_EQ(test_ptr, 45, right);
    BS_TEST_VERIFY_EQ(test_ptr, 25, bottom);
}

/* ------------------------------------------------------------------------- */
/** Exercises "pointer_motion" and "pointer_leave" methods. */
void test_pointer_motion_leave(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != fake_element_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_x));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_y));

    wlmtk_element_pointer_motion(&fake_element_ptr->element, 1.0, 2.0, 1234);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, fake_element_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 2.0, fake_element_ptr->element.last_pointer_y);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        1234,
        fake_element_ptr->element.last_pointer_time_msec);

    wlmtk_element_pointer_leave(&fake_element_ptr->element);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->pointer_leave_called);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_x));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_y));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        0,
        fake_element_ptr->element.last_pointer_time_msec);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Exercises "pointer_button" method. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != fake_element_ptr);

    wlmtk_button_event_t event = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&fake_element_ptr->element, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->pointer_button_called);
}

/* == End of toolkit.c ===================================================== */
