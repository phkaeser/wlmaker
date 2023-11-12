/* ========================================================================= */
/**
 * @file content.c
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

#include "content.h"

#include "container.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
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
static void element_pointer_leave(wlmtk_element_t *element_ptr);
static bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    __UNUSED__ uint32_t time_msec);
static bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

/* == Data ================================================================= */

/** Method table for the container's virtual methods. */
static const wlmtk_element_impl_t super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
    .get_pointer_area = element_get_pointer_area,
    .pointer_leave = element_pointer_leave,
    .pointer_motion = element_pointer_motion,
    .pointer_button = element_pointer_button,
};

void *wlmtk_content_identifier_ptr = wlmtk_content_init;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    const wlmtk_content_impl_t *content_impl_ptr,
    struct wlr_seat *wlr_seat_ptr)
{
    BS_ASSERT(NULL != content_ptr);
    memset(content_ptr, 0, sizeof(wlmtk_content_t));
    BS_ASSERT(NULL != content_impl_ptr);
    BS_ASSERT(NULL != content_impl_ptr->destroy);
    BS_ASSERT(NULL != content_impl_ptr->create_scene_node);
    BS_ASSERT(NULL != content_impl_ptr->request_size);
    BS_ASSERT(NULL != content_impl_ptr->set_activated);

    if (!wlmtk_element_init(&content_ptr->super_element,
                            &super_element_impl)) {
        return false;
    }

    content_ptr->wlr_seat_ptr = wlr_seat_ptr;
    content_ptr->identifier_ptr = wlmtk_content_identifier_ptr;

    memcpy(&content_ptr->impl, content_impl_ptr, sizeof(wlmtk_content_impl_t));
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_fini(wlmtk_content_t *content_ptr)
{
    wlmtk_element_fini(&content_ptr->super_element);
    memset(content_ptr, 0, sizeof(wlmtk_content_t));
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_set_window(
    wlmtk_content_t *content_ptr,
    wlmtk_window_t *window_ptr)
{
    content_ptr->window_ptr = window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_commit_size(
    wlmtk_content_t *content_ptr,
    uint32_t serial,
    unsigned width,
    unsigned height)
{
    if (content_ptr->committed_width != width ||
        content_ptr->committed_height != height) {
        content_ptr->committed_width = width;
        content_ptr->committed_height = height;
    }

    if (NULL != content_ptr->window_ptr) {
        wlmtk_window_serial(content_ptr->window_ptr, serial);
    }

    if (NULL != content_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout(
        content_ptr->super_element.parent_container_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr)
{
    if (NULL != width_ptr) *width_ptr = content_ptr->committed_width;
    if (NULL != height_ptr) *height_ptr = content_ptr->committed_height;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_content_element(wlmtk_content_t *content_ptr)
{
    return &content_ptr->super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::destroy method.
 *
 * Forwards the call to the wlmtk_content_t::destroy method.
 *
 * @param element_ptr
 */
void element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);
    content_ptr->impl.destroy(content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * Forwards the call to the wlmtk_content_t::create_scene_node method.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 */
struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);
    return content_ptr->impl.create_scene_node(
        content_ptr, wlr_scene_tree_ptr);
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
    if (NULL != left_ptr) *left_ptr = 0;
    if (NULL != top_ptr) *top_ptr = 0;

    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    if (NULL != right_ptr) *right_ptr = content_ptr->committed_width;
    if (NULL != bottom_ptr) *bottom_ptr = content_ptr->committed_height;
}

/* ------------------------------------------------------------------------- */
/**
 * Overwrites the element's get_pointer_area method: Returns the extents of
 * the surface and all subsurfaces.
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
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    struct wlr_box box;
    if (NULL == content_ptr->wlr_surface_ptr) {
        // DEBT: Should only get initialized with a valid surface.
        box.x = 0;
        box.y = 0;
        box.width = 0;
        box.height = 0;
    } else {
        wlr_surface_get_extends(content_ptr->wlr_surface_ptr, &box);
    }

    if (NULL != left_ptr) *left_ptr = box.x;
    if (NULL != top_ptr) *top_ptr = box.y;
    if (NULL != right_ptr) *right_ptr = box.width - box.x;
    if (NULL != bottom_ptr) *bottom_ptr = box.height - box.y;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements the element's leave method: If there's a WLR (sub)surface
 * currently holding focus, that will be cleared.
 *
 * @param element_ptr
 */
void element_pointer_leave(wlmtk_element_t *element_ptr)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    // If the current surface's parent is our surface: clear it.
    struct wlr_surface *focused_wlr_surface_ptr =
        content_ptr->wlr_seat_ptr->pointer_state.focused_surface;
    if (NULL != focused_wlr_surface_ptr &&
        wlr_surface_get_root_surface(focused_wlr_surface_ptr) ==
        content_ptr->wlr_surface_ptr) {
        wlr_seat_pointer_clear_focus(content_ptr->wlr_seat_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Pass pointer motion events to client's surface.
 *
 * Identifies the surface (or sub-surface) at the given coordinates, and pass
 * on the motion event to that surface. If needed, will update the seat's
 * pointer focus.
 *
 * @param element_ptr
 * @param x                   Pointer horizontal position, relative to this
 *                            element's node.
 * @param y                   Pointer vertical position, relative to this
 *                            element's node.
 * @param time_msec
 *
 * @return Whether if the motion is within the area.
 */
bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    // Get the layout local coordinates of the node, so we can adjust the
    // node-local (x, y) for the `wlr_scene_node_at` call.
    int lx, ly;
    if (!wlr_scene_node_coords(
            content_ptr->super_element.wlr_scene_node_ptr, &lx, &ly)) {
        return false;
    }
    // Get the node below the cursor. Return if there's no buffer node.
    double node_x, node_y;
    struct wlr_scene_node *wlr_scene_node_ptr = wlr_scene_node_at(
        content_ptr->super_element.wlr_scene_node_ptr,
        x + lx, y + ly, &node_x, &node_y);

    if (NULL == wlr_scene_node_ptr ||
        WLR_SCENE_NODE_BUFFER != wlr_scene_node_ptr->type) {
        return false;
    }

    struct wlr_scene_buffer *wlr_scene_buffer_ptr =
        wlr_scene_buffer_from_node(wlr_scene_node_ptr);
    struct wlr_scene_surface *wlr_scene_surface_ptr =
        wlr_scene_surface_try_from_buffer(wlr_scene_buffer_ptr);
    if (NULL == wlr_scene_surface_ptr) {
        return false;
    }

    BS_ASSERT(content_ptr->wlr_surface_ptr ==
              wlr_surface_get_root_surface(wlr_scene_surface_ptr->surface));
    wlr_seat_pointer_notify_enter(
        content_ptr->wlr_seat_ptr,
        wlr_scene_surface_ptr->surface,
        node_x, node_y);
    wlr_seat_pointer_notify_motion(
        content_ptr->wlr_seat_ptr,
        time_msec,
        node_x, node_y);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Passes pointer button event further to the focused surface, if any.
 *
 * The actual passing is handled by `wlr_seat`. Here we just verify that the
 * currently-focused surface (or sub-surface) is part of this content.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return Whether the button event was consumed.
 */
bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    // Complain if the surface isn't part of our responsibility.
    struct wlr_surface *focused_wlr_surface_ptr =
        content_ptr->wlr_seat_ptr->pointer_state.focused_surface;
    if (NULL == focused_wlr_surface_ptr) return false;
    BS_ASSERT(content_ptr->wlr_surface_ptr ==
              wlr_surface_get_root_surface(focused_wlr_surface_ptr));

    // We're only forwarding PRESSED & RELEASED events.
    if (WLMTK_BUTTON_DOWN == button_event_ptr->type ||
        WLMTK_BUTTON_UP == button_event_ptr->type) {
        wlr_seat_pointer_notify_button(
            content_ptr->wlr_seat_ptr,
            button_event_ptr->time_msec,
            button_event_ptr->button,
            (button_event_ptr->type == WLMTK_BUTTON_DOWN) ?
            WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
        return true;
    }
    return false;
}

/* == Fake content, useful for unit tests. ================================= */

static void fake_content_destroy(
    wlmtk_content_t *content_ptr);
static struct wlr_scene_node *fake_content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static uint32_t fake_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void fake_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/** Method table of the fake content. */
static const wlmtk_content_impl_t wlmtk_fake_content_impl = {
    .destroy = fake_content_destroy,
    .create_scene_node = fake_content_create_scene_node,
    .request_size = fake_content_request_size,
    .set_activated = fake_content_set_activated,
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_content_t *wlmtk_fake_content_create(void)
{
    wlmtk_fake_content_t *fake_content_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_content_t));
    if (NULL == fake_content_ptr) return NULL;

    if (!wlmtk_content_init(&fake_content_ptr->content,
                            &wlmtk_fake_content_impl,
                            NULL)) {
        free(fake_content_ptr);
        return NULL;
    }

    BS_ASSERT(NULL != fake_content_ptr->content.super_element.impl.destroy);
    BS_ASSERT(NULL != fake_content_ptr->content.impl.destroy);
    return fake_content_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor for the fake content. */
void fake_content_destroy(wlmtk_content_t *content_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);

    wlmtk_content_fini(&fake_content_ptr->content);

    // Also expect the super element to be un-initialized.
    BS_ASSERT(NULL == fake_content_ptr->content.super_element.impl.destroy);
    BS_ASSERT(NULL == fake_content_ptr->content.impl.destroy);
    free(fake_content_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates a scene node for the fake content. */
struct wlr_scene_node *fake_content_create_scene_node(
    __UNUSED__ wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, NULL);
    return &wlr_scene_buffer_ptr->node;
}

/* ------------------------------------------------------------------------- */
/** Sets the size of the fake content. */
uint32_t fake_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    fake_content_ptr->requested_width = width;
    fake_content_ptr->requested_height = height;
    return fake_content_ptr->return_request_size;
}

/* ------------------------------------------------------------------------- */
/** Sets the content's activated status. */
void fake_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    fake_content_ptr->activated = activated;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_content_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr;

    fake_content_ptr = wlmtk_fake_content_create();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_content_ptr);

    // Also expect the super element to be initialized.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        fake_content_ptr->content.super_element.impl.destroy);
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        fake_content_ptr->content.impl.destroy);

    int l, t, r, b;
    fake_content_ptr->return_request_size = 42;
    BS_TEST_VERIFY_EQ(
        test_ptr,
        42,
        wlmtk_content_request_size(&fake_content_ptr->content, 42, 21));
    wlmtk_element_get_dimensions(
        &fake_content_ptr->content.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l);
    BS_TEST_VERIFY_EQ(test_ptr, 0, t);
    BS_TEST_VERIFY_EQ(test_ptr, 0, r);
    BS_TEST_VERIFY_EQ(test_ptr, 0, b);

    wlmtk_content_commit_size(&fake_content_ptr->content, 1, 42, 21);
    wlmtk_content_get_size(&fake_content_ptr->content, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, 42, r);
    BS_TEST_VERIFY_EQ(test_ptr, 21, b);

    wlmtk_element_get_dimensions(
        &fake_content_ptr->content.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l);
    BS_TEST_VERIFY_EQ(test_ptr, 0, t);
    BS_TEST_VERIFY_EQ(test_ptr, 42, r);
    BS_TEST_VERIFY_EQ(test_ptr, 21, b);

    wlmtk_content_set_activated(&fake_content_ptr->content, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->activated);

    wlmtk_element_destroy(&fake_content_ptr->content.super_element);
}

/* == End of content.c ================================================== */
