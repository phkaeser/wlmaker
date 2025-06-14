/* ========================================================================= */
/**
 * @file rectangle.c
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

#include "rectangle.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "container.h"
#include "input.h"
#include "util.h"

/* == Declarations ========================================================= */

/** State of a unicolor rectangle. */
struct _wlmtk_rectangle_t {
    /** Superclass element. */
    wlmtk_element_t           super_element;
    /** Original virtual method table of the superclass element. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Width of the rectangle. */
    int                       width;
    /** Height of the rectangle. */
    int                       height;
    /** Color of the rectangle, as an ARGB8888 value. */
    uint32_t                  color;

    /** WLR rectangle. */
    struct wlr_scene_rect     *wlr_scene_rect_ptr;
    /** Listener for the `destroy` signal of `wlr_rect_buffer_ptr->node`. */
    struct wl_listener        wlr_scene_rect_node_destroy_listener;
};

static void _wlmtk_rectangle_element_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *_wlmtk_rectangle_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static bool _wlmtk_rectangle_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr);
static void _wlmtk_rectangle_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr);
static void handle_wlr_scene_rect_node_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Virtual method table of the rectangle, extending the element. */
static const wlmtk_element_vmt_t _wlmtk_rectangle_element_vmt = {
    .destroy = _wlmtk_rectangle_element_destroy,
    .create_scene_node = _wlmtk_rectangle_element_create_scene_node,
    .pointer_motion = _wlmtk_rectangle_element_pointer_motion,
    .get_dimensions = _wlmtk_rectangle_get_dimensions,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_rectangle_t *wlmtk_rectangle_create(
    int width,
    int height,
    uint32_t color)
{
    wlmtk_rectangle_t *rectangle_ptr = logged_calloc(
        1, sizeof(wlmtk_rectangle_t));
    if (NULL == rectangle_ptr) return NULL;
    rectangle_ptr->width = width;
    rectangle_ptr->height = height;
    wlmtk_rectangle_set_color(rectangle_ptr, color);

    if (!wlmtk_element_init(&rectangle_ptr->super_element)) {
        wlmtk_rectangle_destroy(rectangle_ptr);
        return NULL;
    }
    rectangle_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &rectangle_ptr->super_element,
        &_wlmtk_rectangle_element_vmt);

    return rectangle_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_rectangle_destroy(wlmtk_rectangle_t *rectangle_ptr)
{
    if (NULL != rectangle_ptr->wlr_scene_rect_ptr) {
        wlr_scene_node_destroy(&rectangle_ptr->wlr_scene_rect_ptr->node);
        rectangle_ptr->wlr_scene_rect_ptr = NULL;
    }

    wlmtk_element_fini(&rectangle_ptr->super_element);
    free(rectangle_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_rectangle_set_size(
    wlmtk_rectangle_t *rectangle_ptr,
    int width,
    int height)
{
    rectangle_ptr->width = width;
    rectangle_ptr->height = height;

    if (NULL != rectangle_ptr->wlr_scene_rect_ptr) {
        wlr_scene_rect_set_size(
            rectangle_ptr->wlr_scene_rect_ptr,
            rectangle_ptr->width,
            rectangle_ptr->height);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_rectangle_set_color(
    wlmtk_rectangle_t *rectangle_ptr,
    uint32_t color)
{
    rectangle_ptr->color = color;

    if (NULL != rectangle_ptr->wlr_scene_rect_ptr) {
        float fcolor[4];
        bs_gfxbuf_argb8888_to_floats(
            color, &fcolor[0], &fcolor[1], &fcolor[2], &fcolor[3]);
        wlr_scene_rect_set_color(rectangle_ptr->wlr_scene_rect_ptr, fcolor);
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_rectangle_element(wlmtk_rectangle_t *rectangle_ptr)
{
    return &rectangle_ptr->super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_rectangle_t *wlmtk_rectangle_from_element(wlmtk_element_t *element_ptr)
{
    BS_ASSERT(element_ptr->vmt.destroy = _wlmtk_rectangle_element_destroy);
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_rectangle_t, super_element);
    return rectangle_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual dtor: Invoke the rectangle's dtor. */
void _wlmtk_rectangle_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_rectangle_t, super_element);
    wlmtk_rectangle_destroy(rectangle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * Creates a `struct wlr_scene_rect` attached to `wlr_scene_tree_ptr`.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 */
struct wlr_scene_node *_wlmtk_rectangle_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_rectangle_t, super_element);

    BS_ASSERT(NULL == rectangle_ptr->wlr_scene_rect_ptr);
    float color[4];
    bs_gfxbuf_argb8888_to_floats(
        rectangle_ptr->color, &color[0], &color[1], &color[2], &color[3]);
    rectangle_ptr->wlr_scene_rect_ptr = wlr_scene_rect_create(
        wlr_scene_tree_ptr,
        rectangle_ptr->width,
        rectangle_ptr->height,
        color);
    if (NULL == rectangle_ptr->wlr_scene_rect_ptr) return NULL;

    wlmtk_util_connect_listener_signal(
        &rectangle_ptr->wlr_scene_rect_ptr->node.events.destroy,
        &rectangle_ptr->wlr_scene_rect_node_destroy_listener,
        handle_wlr_scene_rect_node_destroy);
    return &rectangle_ptr->wlr_scene_rect_ptr->node;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::pointer_motion. Sets cursor. */
bool _wlmtk_rectangle_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_rectangle_t, super_element);
    bool rv = rectangle_ptr->orig_super_element_vmt.pointer_motion(
        element_ptr, motion_event_ptr);
    if (rv) {
        wlmtk_pointer_set_cursor(
            motion_event_ptr->pointer_ptr,
            WLMTK_POINTER_CURSOR_DEFAULT);
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param x1_ptr              0.
 * @param y1_ptr              0.
 * @param x2_ptr              Width. May be NULL.
 * @param y2_ptr              Height. May be NULL.
 */
void _wlmtk_rectangle_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_rectangle_t, super_element);
    if (NULL != x1_ptr) *x1_ptr = 0;
    if (NULL != y1_ptr) *y1_ptr = 0;
    if (NULL != x2_ptr) *x2_ptr = rectangle_ptr->width;
    if (NULL != y2_ptr) *y2_ptr = rectangle_ptr->height;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'destroy' callback of wlr_scene_rect_ptr->node.
 *
 * Will reset the wlr_scene_rect_ptr value. Destruction of the node had
 * been triggered (hence the callback).
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_wlr_scene_rect_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_rectangle_t, wlr_scene_rect_node_destroy_listener);

    rectangle_ptr->wlr_scene_rect_ptr = NULL;
    wl_list_remove(&rectangle_ptr->wlr_scene_rect_node_destroy_listener.link);
}

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_create_destroy_scene(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_rectangle_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "create_destroy_scene", test_create_destroy_scene },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown of rectangle. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_rectangle_t *rectangle_ptr = wlmtk_rectangle_create(
        10, 20, 0x01020304);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, rectangle_ptr);

    int x1, y1, x2, y2;
    wlmtk_element_get_dimensions(
        &rectangle_ptr->super_element, &x1, &y1, &x2, &y2);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x1);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y1);
    BS_TEST_VERIFY_EQ(test_ptr, 10, x2);
    BS_TEST_VERIFY_EQ(test_ptr, 20, y2);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &rectangle_ptr->super_element,
        wlmtk_rectangle_element(rectangle_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        rectangle_ptr,
        wlmtk_rectangle_from_element(&rectangle_ptr->super_element));

    wlmtk_rectangle_destroy(rectangle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown of rectangle, when attached to scene graph. */
void test_create_destroy_scene(bs_test_t *test_ptr)
{
    wlmtk_container_t *c_ptr = wlmtk_container_create_fake_parent();
    wlmtk_rectangle_t *rectangle_ptr = wlmtk_rectangle_create(
        10, 20, 0x01020304);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, rectangle_ptr);
    wlmtk_element_t *element_ptr = wlmtk_rectangle_element(rectangle_ptr);

    wlmtk_container_add_element(c_ptr, element_ptr);

    int x1, y1, x2, y2;
    wlmtk_element_get_dimensions(element_ptr, &x1, &y1, &x2, &y2);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x1);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y1);
    BS_TEST_VERIFY_EQ(test_ptr, 10, x2);
    BS_TEST_VERIFY_EQ(test_ptr, 20, y2);

    BS_TEST_VERIFY_NEQ(test_ptr, NULL, rectangle_ptr->wlr_scene_rect_ptr);

    wlmtk_container_remove_element(c_ptr, element_ptr);

    wlmtk_element_destroy(element_ptr);
    wlmtk_container_destroy_fake_parent(c_ptr);
}

/* == End of rectangle.c =================================================== */
