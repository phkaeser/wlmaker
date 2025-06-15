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

#include "container.h"
#include "element.h"
#include "input.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE
#include <wlr/util/box.h>
#include <libbase/libbase.h>

/* == Declarations ========================================================= */

static void _wlmtk_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr);
static bool _wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr);
static bool _wlmtk_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_element_pointer_axis(
    __UNUSED__ wlmtk_element_t *element_ptr,
    __UNUSED__ struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static void _wlmtk_element_keyboard_blur(wlmtk_element_t *element_ptr);
static bool _wlmtk_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    const xkb_keysym_t *key_syms,
    size_t key_syms_count,
    uint32_t modifiers);

static void handle_wlr_scene_node_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Default virtual method table. Initializes the non-abstract methods. */
static const wlmtk_element_vmt_t element_vmt = {
    .get_pointer_area = _wlmtk_element_get_pointer_area,
    .pointer_motion = _wlmtk_element_pointer_motion,
    .pointer_button = _wlmtk_element_pointer_button,
    .pointer_axis = _wlmtk_element_pointer_axis,
    .keyboard_blur = _wlmtk_element_keyboard_blur,
    .keyboard_event = _wlmtk_element_keyboard_event,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_element_init(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    *element_ptr = (wlmtk_element_t){ .vmt = element_vmt };

    wl_signal_init(&element_ptr->events.pointer_enter);
    wl_signal_init(&element_ptr->events.pointer_leave);

    element_ptr->last_pointer_motion_event = (wlmtk_pointer_motion_event_t){
        .x = NAN, .y = NAN, .time_msec = 0 };
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_vmt_t wlmtk_element_extend(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_vmt_t *element_vmt_ptr)
{
    wlmtk_element_vmt_t orig_vmt = element_ptr->vmt;

    // Only overwrite provided methods.
    if (NULL != element_vmt_ptr->destroy) {
        element_ptr->vmt.destroy = element_vmt_ptr->destroy;
    }
    if (NULL != element_vmt_ptr->create_scene_node) {
        element_ptr->vmt.create_scene_node = element_vmt_ptr->create_scene_node;
    }
    if (NULL != element_vmt_ptr->get_dimensions) {
        element_ptr->vmt.get_dimensions = element_vmt_ptr->get_dimensions;
    }
    if (NULL != element_vmt_ptr->get_pointer_area) {
        element_ptr->vmt.get_pointer_area = element_vmt_ptr->get_pointer_area;
    }
    if (NULL != element_vmt_ptr->pointer_motion) {
        element_ptr->vmt.pointer_motion = element_vmt_ptr->pointer_motion;
    }
    if (NULL != element_vmt_ptr->pointer_button) {
        element_ptr->vmt.pointer_button = element_vmt_ptr->pointer_button;
    }
    if (NULL != element_vmt_ptr->pointer_axis) {
        element_ptr->vmt.pointer_axis = element_vmt_ptr->pointer_axis;
    }
    if (NULL != element_vmt_ptr->pointer_grab_cancel) {
        element_ptr->vmt.pointer_grab_cancel =
            element_vmt_ptr->pointer_grab_cancel;
    }
    if (NULL != element_vmt_ptr->keyboard_blur) {
        element_ptr->vmt.keyboard_blur = element_vmt_ptr->keyboard_blur;
    }
    if (NULL != element_vmt_ptr->keyboard_event) {
        element_ptr->vmt.keyboard_event = element_vmt_ptr->keyboard_event;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr)
{
    // Verify we're no longer part of the scene graph, nor part of a container.
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);

    *element_ptr = (wlmtk_element_t){};
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
        element_ptr->wlr_scene_node_ptr = element_ptr->vmt.create_scene_node(
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

    if (NULL != element_ptr->parent_container_ptr) {
        wlmtk_container_update_layout(element_ptr->parent_container_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_get_position(
    wlmtk_element_t *element_ptr,
    int *x_ptr,
    int *y_ptr)
{
    // The node may have been moved without us noticing... update it.
    if (NULL != element_ptr->wlr_scene_node_ptr) {
        element_ptr->x = element_ptr->wlr_scene_node_ptr->x;
        element_ptr->y = element_ptr->wlr_scene_node_ptr->y;
    }

    if (NULL != x_ptr) *x_ptr = element_ptr->x;
    if (NULL != y_ptr) *y_ptr = element_ptr->y;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_set_position(
    wlmtk_element_t *element_ptr,
    int x,
    int y)
{
    if (NULL != element_ptr->wlr_scene_node_ptr) {
        wlr_scene_node_set_position(element_ptr->wlr_scene_node_ptr, x, y);
    }

    // Optimization clause: Can leave here, if coordinates didn't change.
    if (element_ptr->x == x && element_ptr->y == y) return;
    element_ptr->x = x;
    element_ptr->y = y;

    if (NULL != element_ptr->parent_container_ptr) {
        wlmtk_container_update_pointer_focus(
            element_ptr->parent_container_ptr);
    }
}

/* ------------------------------------------------------------------------- */
bool wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    bool within = element_ptr->vmt.pointer_motion(
        element_ptr, motion_event_ptr);
    if (within == element_ptr->pointer_inside) return within;

    if (within) {
        element_ptr->pointer_inside = true;
        wl_signal_emit(&element_ptr->events.pointer_enter, motion_event_ptr);
    } else {
        element_ptr->pointer_inside = false;
        wl_signal_emit(&element_ptr->events.pointer_leave, motion_event_ptr);
    }

    return within;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Wraps to wlmtk_element_vmt_t::get_dimensions as default implementation. */
void _wlmtk_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr)
{
    wlmtk_element_get_dimensions(element_ptr, x1_ptr, y1_ptr, x2_ptr, y2_ptr);
}

/* ------------------------------------------------------------------------- */
/** Stores pointer coordinates and timestamp. Returns true is x,y not NAN. */
bool _wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    element_ptr->last_pointer_motion_event = *motion_event_ptr;
    if (isnan(motion_event_ptr->x) || isnan(motion_event_ptr->y)) {
        element_ptr->last_pointer_motion_event.x = NAN;
        element_ptr->last_pointer_motion_event.y = NAN;
    }

    return !isnan(element_ptr->last_pointer_motion_event.x) &&
        !isnan(element_ptr->last_pointer_motion_event.y);
}

/* ------------------------------------------------------------------------- */
/** Does nothing, returns false. */
bool _wlmtk_element_pointer_button(
    __UNUSED__ wlmtk_element_t *element_ptr,
    __UNUSED__ const wlmtk_button_event_t *button_event_ptr)
{
    return false;
}

/* ------------------------------------------------------------------------- */
/** Does nothing, returns false. */
bool _wlmtk_element_pointer_axis(
    __UNUSED__ wlmtk_element_t *element_ptr,
    __UNUSED__ struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    return false;
}

/* ------------------------------------------------------------------------- */
/** Handler for losing keyboard focus. Nothing for default impl. */
void _wlmtk_element_keyboard_blur(__UNUSED__ wlmtk_element_t *element_ptr)
{
    // Nothing.
}

/* ------------------------------------------------------------------------- */
/** Handler for keyboard events. By default: Nothing is handled. */
bool _wlmtk_element_keyboard_event(
    __UNUSED__ wlmtk_element_t *element_ptr,
    __UNUSED__ struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    __UNUSED__ const xkb_keysym_t *key_syms,
    __UNUSED__ size_t key_syms_count,
    __UNUSED__ uint32_t modifiers)
{
    return false;
}

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
static bool fake_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr);
static bool fake_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool fake_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static void fake_pointer_grab_cancel(
    wlmtk_element_t *element_ptr);
static void fake_keyboard_blur(
    wlmtk_element_t *element_ptr);
static bool fake_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    const xkb_keysym_t *key_syms,
    size_t key_syms_count,
    uint32_t modifiers);

/** Virtual method table for the fake element. */
static const wlmtk_element_vmt_t fake_element_vmt = {
    .destroy = fake_destroy,
    .create_scene_node = fake_create_scene_node,
    .get_dimensions = fake_get_dimensions,
    .get_pointer_area = fake_get_pointer_area,
    .pointer_motion = fake_pointer_motion,
    .pointer_button = fake_pointer_button,
    .pointer_axis = fake_pointer_axis,
    .pointer_grab_cancel = fake_pointer_grab_cancel,
    .keyboard_blur = fake_keyboard_blur,
    .keyboard_event = fake_keyboard_event,
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_element_t *wlmtk_fake_element_create(void)
{
    wlmtk_fake_element_t *fake_element_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_element_t));
    if (NULL == fake_element_ptr) return NULL;

    if (!wlmtk_element_init(&fake_element_ptr->element)) {
        fake_destroy(&fake_element_ptr->element);
        return NULL;
    }

    fake_element_ptr->orig_vmt = wlmtk_element_extend(
        &fake_element_ptr->element, &fake_element_vmt);

    return fake_element_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_element_grab_keyboard(wlmtk_fake_element_t *fake_element_ptr)
{
    fake_element_ptr->has_keyboard_focus = true;
    if (NULL != fake_element_ptr->element.parent_container_ptr) {
        wlmtk_container_set_keyboard_focus_element(
            fake_element_ptr->element.parent_container_ptr,
            &fake_element_ptr->element);
    }
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
    if (NULL != left_ptr) *left_ptr = fake_element_ptr->dimensions.x;
    if (NULL != top_ptr) *top_ptr = fake_element_ptr->dimensions.y;
    if (NULL != right_ptr) *right_ptr = (
        fake_element_ptr->dimensions.width - fake_element_ptr->dimensions.x);
    if (NULL != bottom_ptr) *bottom_ptr = (
        fake_element_ptr->dimensions.height - fake_element_ptr->dimensions.y);
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
    if (NULL != right_ptr) *right_ptr = (
        fake_element_ptr->dimensions.width + 3);
    if (NULL != bottom_ptr) *bottom_ptr = (
        fake_element_ptr->dimensions.height + 4);
}

/* ------------------------------------------------------------------------- */
/** Handles 'motion' events for the fake element, updates last position. */
bool fake_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->orig_vmt.pointer_motion(element_ptr, motion_event_ptr);
    fake_element_ptr->pointer_motion_called = true;
    return (-1 <= motion_event_ptr->x &&
            motion_event_ptr->x < fake_element_ptr->dimensions.width + 3 &&
            -2 < motion_event_ptr->y &&
            motion_event_ptr->y < fake_element_ptr->dimensions.height + 4);
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
    fake_element_ptr->pointer_button_event = *button_event_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles 'axis' events for the fake element. */
bool fake_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);

    fake_element_ptr->pointer_axis_called = true;
    fake_element_ptr->wlr_pointer_axis_event = *wlr_pointer_axis_event_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
/** Records calls to @ref wlmtk_element_vmt_t::pointer_grab_cancel. */
void fake_pointer_grab_cancel(
    wlmtk_element_t *element_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->pointer_grab_cancel_called = true;
}

/* ------------------------------------------------------------------------- */
/** Registers losing keyboard focus. */
void fake_keyboard_blur(wlmtk_element_t *element_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->has_keyboard_focus = false;
}

/* ------------------------------------------------------------------------- */
/** Handles 'keyboard_event' events for the fake element. */
bool fake_keyboard_event(
    wlmtk_element_t *element_ptr,
    __UNUSED__ struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    __UNUSED__ const xkb_keysym_t *key_syms,
    __UNUSED__ size_t key_syms_count,
    __UNUSED__ uint32_t modifiers)
{
    wlmtk_fake_element_t *fake_element_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_element_t, element);
    fake_element_ptr->keyboard_event_called = true;
    return true;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_set_parent_container(bs_test_t *test_ptr);
static void test_set_get_position(bs_test_t *test_ptr);
static void test_get_dimensions(bs_test_t *test_ptr);
static void test_get_pointer_area(bs_test_t *test_ptr);
static void test_pointer_motion_leave(bs_test_t *test_ptr);
static void test_pointer_button(bs_test_t *test_ptr);
static void test_pointer_axis(bs_test_t *test_ptr);
static void test_keyboard_focus(bs_test_t *test_ptr);
static void test_keyboard_event(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "set_parent_container", test_set_parent_container },
    { 1, "set_get_position", test_set_get_position },
    { 1, "get_dimensions", test_get_dimensions },
    { 1, "get_pointer_area", test_get_pointer_area },
    { 1, "pointer_motion_leave", test_pointer_motion_leave },
    { 1, "pointer_button", test_pointer_button },
    { 1, "pointer_axis", test_pointer_axis },
    { 1, "keyboard_focus", test_keyboard_focus },
    { 1, "keyboard_event", test_keyboard_event },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element));
    wlmtk_element_extend(&element, &fake_element_vmt);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, element.vmt.destroy);

    wlmtk_element_fini(&element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.vmt.destroy);
}

/* ------------------------------------------------------------------------- */
/** Tests set_parent_container, and that scene graph follows. */
void test_set_parent_container(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element));
    wlmtk_element_extend(&element, &fake_element_vmt);

    // Setting a parent without a scene graph tree will not set a node.
    wlmtk_container_t parent_no_tree;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&parent_no_tree));
    wlmtk_element_set_parent_container(&element, &parent_no_tree);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);
    wlmtk_container_fini(&parent_no_tree);

    // Setting a parent with a tree must create & attach the node there.
    wlmtk_container_t *parent_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, parent_ptr);
    wlmtk_element_set_visible(&element, true);
    wlmtk_element_set_parent_container(&element, parent_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        parent_ptr->wlr_scene_tree_ptr,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Resetting the parent must also re-attach the node.
    wlmtk_container_t *other_parent_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, other_parent_ptr);
    wlmtk_element_set_parent_container(&element, other_parent_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        other_parent_ptr->wlr_scene_tree_ptr,
        element.wlr_scene_node_ptr->parent);
    BS_TEST_VERIFY_TRUE(test_ptr, element.wlr_scene_node_ptr->enabled);

    // Clearing the parent most remove the node.
    wlmtk_element_set_parent_container(&element, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, element.wlr_scene_node_ptr);

    wlmtk_container_destroy_fake_parent(other_parent_ptr);
    wlmtk_container_destroy_fake_parent(parent_ptr);
    wlmtk_element_fini(&element);
}

/* ------------------------------------------------------------------------- */
/** Tests get_position and set_position, and that scene graph follows. */
void test_set_get_position(bs_test_t *test_ptr)
{
    wlmtk_element_t element;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_init(&element));
    wlmtk_element_extend(&element, &fake_element_vmt);

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
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_parent_ptr);
    wlmtk_element_set_parent_container(&element, fake_parent_ptr);

    BS_TEST_VERIFY_EQ(test_ptr, 10, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 20, element.wlr_scene_node_ptr->y);

    wlmtk_element_set_position(&element, 30, 40);
    BS_TEST_VERIFY_EQ(test_ptr, 30, element.wlr_scene_node_ptr->x);
    BS_TEST_VERIFY_EQ(test_ptr, 40, element.wlr_scene_node_ptr->y);

    wlr_scene_node_set_position(element.wlr_scene_node_ptr, 50, 60);
    wlmtk_element_get_position(&element, &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 50, x);
    BS_TEST_VERIFY_EQ(test_ptr, 60, y);

    wlmtk_element_set_parent_container(&element, NULL);
    wlmtk_element_fini(&element);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests get_dimensions. */
void test_get_dimensions(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    fake_element_ptr->dimensions.x = -10;
    fake_element_ptr->dimensions.y = -20;
    fake_element_ptr->dimensions.width = 42;
    fake_element_ptr->dimensions.height = 21;

    // Must not crash.
    wlmtk_element_get_dimensions(
        &fake_element_ptr->element, NULL, NULL, NULL, NULL);

    int top, left, right, bottom;
    wlmtk_element_get_dimensions(
        &fake_element_ptr->element, &top, &left, &right, &bottom);
    BS_TEST_VERIFY_EQ(test_ptr, -10, top);
    BS_TEST_VERIFY_EQ(test_ptr, -20, left);
    BS_TEST_VERIFY_EQ(test_ptr, 52, right);
    BS_TEST_VERIFY_EQ(test_ptr, 41, bottom);

    struct wlr_box box = wlmtk_element_get_dimensions_box(
        &fake_element_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, -10, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, -20, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 42, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 21, box.height);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Tests get_dimensions. */
void test_get_pointer_area(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    fake_element_ptr->dimensions.width = 42;
    fake_element_ptr->dimensions.height = 21;

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

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Verifies signals for "pointer_motion" and "pointer_leave". */
void test_pointer_motion_leave(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);

    wlmtk_util_test_listener_t          enter = {}, leave = {};
    wlmtk_util_connect_test_listener(
        &fake_element_ptr->element.events.pointer_enter, &enter);
    wlmtk_util_connect_test_listener(
        &fake_element_ptr->element.events.pointer_leave, &leave);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_motion_event.x));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_motion_event.y));

    wlmtk_pointer_motion_event_t e = { .x = 1.0, .y = 2.0, .time_msec = 3 };
    wlmtk_element_pointer_motion(&fake_element_ptr->element, &e);
    BS_TEST_VERIFY_EQ(test_ptr, 1, enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, leave.calls);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        1.0,
        fake_element_ptr->element.last_pointer_motion_event.x);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        2.0,
        fake_element_ptr->element.last_pointer_motion_event.y);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        3,
        fake_element_ptr->element.last_pointer_motion_event.time_msec);

    e = (wlmtk_pointer_motion_event_t){ .x = NAN, .y = NAN, .time_msec = 4 };
    wlmtk_element_pointer_motion(&fake_element_ptr->element, &e);
    BS_TEST_VERIFY_EQ(test_ptr, 1, enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, leave.calls);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_motion_event.x));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        isnan(fake_element_ptr->element.last_pointer_motion_event.y));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        4,
        fake_element_ptr->element.last_pointer_motion_event.time_msec);

    wlmtk_util_disconnect_test_listener(&leave);
    wlmtk_util_disconnect_test_listener(&enter);
    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Exercises "pointer_button" method. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);

    wlmtk_button_event_t event = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&fake_element_ptr->element, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->pointer_button_called);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Exercises "pointer_axis" method. */
void test_pointer_axis(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);

    struct wlr_pointer_axis_event event = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_axis(&fake_element_ptr->element, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->pointer_axis_called);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Exercises keyboard grab & blur methods. */
void test_keyboard_focus(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);

    wlmtk_fake_element_grab_keyboard(fake_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->has_keyboard_focus);
    wlmtk_element_keyboard_blur(&fake_element_ptr->element);
    BS_TEST_VERIFY_FALSE(test_ptr, fake_element_ptr->has_keyboard_focus);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* ------------------------------------------------------------------------- */
/** Exercises "keyboard_event" method. */
void test_keyboard_event(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);

    struct wlr_keyboard_key_event event = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_event(
            &fake_element_ptr->element, &event, NULL, 0, 0));
    BS_TEST_VERIFY_TRUE(test_ptr, fake_element_ptr->keyboard_event_called);

    wlmtk_element_destroy(&fake_element_ptr->element);
}

/* == End of toolkit.c ===================================================== */
