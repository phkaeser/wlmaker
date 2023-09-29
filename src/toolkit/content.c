/* ========================================================================= */
/**
 * @file content.c
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
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

#define WLR_USE_UNSTABLE
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

/* == Data ================================================================= */

/** Method table for the container's virtual methods. */
const wlmtk_element_impl_t  super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
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
    BS_ASSERT(NULL != content_impl_ptr);
    BS_ASSERT(NULL != content_impl_ptr->destroy);
    BS_ASSERT(NULL != content_impl_ptr->create_scene_node);
    BS_ASSERT(NULL != content_impl_ptr->get_size);
    BS_ASSERT(NULL != content_impl_ptr->set_activated);

    memset(content_ptr, 0, sizeof(wlmtk_content_t));

    if (!wlmtk_element_init(&content_ptr->super_element,
                            &super_element_impl)) {
        return false;
    }

    content_ptr->wlr_seat_ptr = wlr_seat_ptr;

    content_ptr->impl_ptr = content_impl_ptr;
    content_ptr->identifier_ptr = wlmtk_content_identifier_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_fini(wlmtk_content_t *content_ptr)
{
    wlmtk_element_fini(&content_ptr->super_element);
    content_ptr->impl_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_set_window(
    wlmtk_content_t *content_ptr,
    wlmtk_window_t *window_ptr)
{
    content_ptr->window_ptr = window_ptr;
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
    content_ptr->impl_ptr->destroy(content_ptr);
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
    return content_ptr->impl_ptr->create_scene_node(
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
    wlmtk_content_get_size(content_ptr, right_ptr, bottom_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's motion method: Sets the surface as active
 * and -- if (x, y) is within the area -- returns this element's pointer.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return element_ptr
 */
wlmtk_element_t *element_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    __UNUSED__ uint32_t time_msec)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_element);

    // Guard clause: only forward if the motion is inside the content.
    int width, height;
    wlmtk_content_get_size(content_ptr, &width, &height);
    if (x < 0 || width <= x || y < 0 || height <= y) return NULL;

    if (NULL != content_ptr->wlr_surface_ptr) {
        wlr_seat_pointer_notify_enter(
            content_ptr->wlr_seat_ptr,
            content_ptr->wlr_surface_ptr,
            x, y);
    }
    return element_ptr;
}

/* == Fake content, useful for unit tests. ================================= */

static void fake_content_destroy(
    wlmtk_content_t *content_ptr);
static struct wlr_scene_node *fake_content_create_scene_node(
    wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void fake_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr);
static void fake_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/** Method table of the fake content. */
static const wlmtk_content_impl_t wlmtk_fake_content_impl = {
    .destroy = fake_content_destroy,
    .create_scene_node = fake_content_create_scene_node,
    .get_size = fake_content_get_size,
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

    BS_ASSERT(NULL != fake_content_ptr->content.super_element.impl_ptr);
    BS_ASSERT(NULL != fake_content_ptr->content.impl_ptr);
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
    BS_ASSERT(NULL == fake_content_ptr->content.super_element.impl_ptr);
    BS_ASSERT(NULL == fake_content_ptr->content.impl_ptr);
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
/** Gets the size of the fake content. */
void fake_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr, int *height_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    if (NULL != width_ptr) *width_ptr = fake_content_ptr->width;
    if (NULL != height_ptr) *height_ptr = fake_content_ptr->height;
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
        fake_content_ptr->content.super_element.impl_ptr);
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL,
        fake_content_ptr->content.impl_ptr);

    int l, t, r, b;
    fake_content_ptr->width = 42;
    fake_content_ptr->height = 21;
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
