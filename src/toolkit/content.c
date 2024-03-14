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

#include "surface.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void _wlmtk_content_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);

/* == Data ================================================================= */

/** Virtual method table for the content's superclass @ref wlmtk_element_t. */
static const wlmtk_element_vmt_t _wlmtk_content_element_vmt = {
    .get_dimensions = _wlmtk_content_element_get_dimensions,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    wlmtk_surface_t *surface_ptr,
    wlmtk_env_t *env_ptr)
{
    BS_ASSERT(NULL != content_ptr);
    memset(content_ptr, 0, sizeof(wlmtk_content_t));

    if (!wlmtk_container_init(&content_ptr->super_container, env_ptr)) {
        return false;
    }
    content_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &content_ptr->super_container.super_element,
        &_wlmtk_content_element_vmt);

    if (NULL != surface_ptr) {
        wlmtk_content_set_surface(content_ptr, surface_ptr);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_fini(
    wlmtk_content_t *content_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = content_ptr->popups.head_ptr)) {
        wlmtk_content_t *popup_content_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmtk_content_t, dlnode);
        wlmtk_content_remove_popup(content_ptr, popup_content_ptr);
    }

    if (NULL != content_ptr->surface_ptr) {
        wlmtk_container_remove_element(
            &content_ptr->super_container,
            wlmtk_surface_element(content_ptr->surface_ptr));
        content_ptr->surface_ptr = NULL;
    }
    memset(content_ptr, 0, sizeof(wlmtk_content_t));
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_set_surface(
    wlmtk_content_t *content_ptr,
    wlmtk_surface_t *surface_ptr)
{
    if (NULL == surface_ptr && NULL == content_ptr->surface_ptr) return;

    memset(&content_ptr->client, 0, sizeof(wlmtk_util_client_t));

    if (NULL != content_ptr->surface_ptr) {
        wlmtk_element_set_visible(
            wlmtk_surface_element(content_ptr->surface_ptr), false);
        wlmtk_container_remove_element(
            &content_ptr->super_container,
            wlmtk_surface_element(content_ptr->surface_ptr));
        content_ptr->surface_ptr = NULL;
    }

    if (NULL != surface_ptr) {
        wlmtk_container_add_element(
            &content_ptr->super_container,
            wlmtk_surface_element(surface_ptr));
        content_ptr->surface_ptr = surface_ptr;
        wlmtk_element_set_visible(wlmtk_surface_element(surface_ptr), true);

        if (NULL != surface_ptr->wlr_surface_ptr) {
            wl_client_get_credentials(
                surface_ptr->wlr_surface_ptr->resource->client,
                &content_ptr->client.pid,
                &content_ptr->client.uid,
                &content_ptr->client.gid);
        }
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_content_vmt_t wlmtk_content_extend(
    wlmtk_content_t *content_ptr,
    const wlmtk_content_vmt_t *content_vmt_ptr)
{
    wlmtk_content_vmt_t orig_vmt = content_ptr->vmt;

    if (NULL != content_vmt_ptr->request_maximized) {
        content_ptr->vmt.request_maximized =
            content_vmt_ptr->request_maximized;
    }
    if (NULL != content_vmt_ptr->request_fullscreen) {
        content_ptr->vmt.request_fullscreen =
            content_vmt_ptr->request_fullscreen;
    }
    if (NULL != content_vmt_ptr->request_size) {
        content_ptr->vmt.request_size =
            content_vmt_ptr->request_size;
    }
    if (NULL != content_vmt_ptr->request_close) {
        content_ptr->vmt.request_close =
            content_vmt_ptr->request_close;
    }
    if (NULL != content_vmt_ptr->set_activated) {
        content_ptr->vmt.set_activated =
            content_vmt_ptr->set_activated;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr)
{
    if (NULL == content_ptr->surface_ptr) {
        if (NULL != width_ptr) *width_ptr = 0;
        if (NULL != height_ptr) *height_ptr = 0;
    } else {
        wlmtk_surface_get_size(content_ptr->surface_ptr, width_ptr, height_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_commit_serial(
    wlmtk_content_t *content_ptr,
    uint32_t serial)
{
    if (NULL != content_ptr->window_ptr) {
        wlmtk_window_serial(content_ptr->window_ptr, serial);
    }
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
    return &content_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_add_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_content_t *popup_content_ptr)
{
    BS_ASSERT(wlmtk_content_element(popup_content_ptr)->parent_container_ptr ==
              NULL);
    BS_ASSERT(NULL == popup_content_ptr->parent_content_ptr);
    wlmtk_container_add_element(
        &content_ptr->super_container,
        wlmtk_content_element(popup_content_ptr));
    popup_content_ptr->parent_content_ptr = content_ptr;

    bs_dllist_push_back(
        &content_ptr->popups,
        &popup_content_ptr->dlnode);
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_remove_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_content_t *popup_content_ptr)
{
    BS_ASSERT(wlmtk_content_element(popup_content_ptr)->parent_container_ptr ==
              &content_ptr->super_container);
    BS_ASSERT(content_ptr == popup_content_ptr->parent_content_ptr);
    bs_dllist_remove(
        &content_ptr->popups,
        &popup_content_ptr->dlnode);
    wlmtk_container_remove_element(
        &content_ptr->super_container,
        wlmtk_content_element(popup_content_ptr));
    popup_content_ptr->parent_content_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
wlmtk_content_t *wlmtk_content_get_parent_content(
    wlmtk_content_t *content_ptr)
{
    return content_ptr->parent_content_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the content's dimension: Considers only the surface, and leaves
 * out pop-ups, in order to draw margins and decorations for just the main
 * surface.
 *
 * @param element_ptr
 * @param left_ptr
 * @param top_ptr
 * @param right_ptr
 * @param bottom_ptr
 */
void _wlmtk_content_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_content_t *content_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_content_t, super_container.super_element);

    wlmtk_element_get_dimensions(
        wlmtk_surface_element(content_ptr->surface_ptr),
        left_ptr, top_ptr, right_ptr, bottom_ptr);
}

/* == Fake content, for tests ============================================== */

static void _wlmtk_fake_content_request_close(wlmtk_content_t *content_ptr);
static uint32_t _wlmtk_fake_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void _wlmtk_fake_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/** Virtual method table for the fake content. */
static wlmtk_content_vmt_t    _wlmtk_fake_content_vmt = {
    .request_size = _wlmtk_fake_content_request_size,
    .request_close = _wlmtk_fake_content_request_close,
    .set_activated = _wlmtk_fake_content_set_activated,
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_content_t *wlmtk_fake_content_create(
    wlmtk_fake_surface_t *fake_surface_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_content_t));
    if (NULL == fake_content_ptr) return NULL;
    fake_content_ptr->fake_surface_ptr = fake_surface_ptr;

    if (!wlmtk_content_init(&fake_content_ptr->content,
                            &fake_surface_ptr->surface,
                            NULL)) {
        wlmtk_fake_content_destroy(fake_content_ptr);
        return NULL;
    }
    wlmtk_content_extend(&fake_content_ptr->content, &_wlmtk_fake_content_vmt);

    return fake_content_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_content_destroy(wlmtk_fake_content_t *fake_content_ptr)
{
    wlmtk_content_fini(&fake_content_ptr->content);
    free(fake_content_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_content_commit(wlmtk_fake_content_t *fake_content_ptr)
{
    wlmtk_content_commit_serial(
        &fake_content_ptr->content,
        fake_content_ptr->serial);

    wlmtk_fake_surface_commit_size(
        fake_content_ptr->fake_surface_ptr,
        fake_content_ptr->requested_width,
        fake_content_ptr->requested_height);
}

/* ------------------------------------------------------------------------- */
/** Test implementation of @ref wlmtk_content_vmt_t::request_size. */
uint32_t _wlmtk_fake_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    fake_content_ptr->requested_width = width;
    fake_content_ptr->requested_height = height;
    return fake_content_ptr->serial;
}

/* ------------------------------------------------------------------------- */
/** Test implementation of @ref wlmtk_content_vmt_t::request_close. */
void _wlmtk_fake_content_request_close(wlmtk_content_t *content_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    fake_content_ptr->request_close_called = true;
}

/* ------------------------------------------------------------------------- */
/** Test implementation of @ref wlmtk_content_vmt_t::set_activated. */
void _wlmtk_fake_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmtk_fake_content_t *fake_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmtk_fake_content_t, content);
    fake_content_ptr->activated = activated;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_set_clear_surface(bs_test_t *test_ptr);
static void test_add_remove_popup(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_content_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "set_clear_surface", test_set_clear_surface },
    { 1, "add_remove_popup", test_add_remove_popup },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    struct wlr_box box;

    wlmtk_fake_surface_t *fs_ptr = wlmtk_fake_surface_create();
    BS_ASSERT(NULL != fs_ptr);
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create(fs_ptr);
    BS_ASSERT(NULL != fake_content_ptr);

    wlmtk_element_t *element_ptr = wlmtk_content_element(
        &fake_content_ptr->content);

    // Initial size is zero.
    box = wlmtk_element_get_dimensions_box(element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.height);

    // Pointer motion, should report to not be within the content.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, 10, 10, 0));

    // Request & commit a sensible size, verifies the content reports it.
    wlmtk_content_request_size(&fake_content_ptr->content, 200, 100);
    wlmtk_fake_content_commit(fake_content_ptr);
    box = wlmtk_element_get_dimensions_box(element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Pointer motion shouuld now report to be within the content.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, 10, 10, 0));

    wlmtk_fake_content_destroy(fake_content_ptr);
    wlmtk_fake_surface_destroy(fs_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests setting and clearing the surface. */
void test_set_clear_surface(bs_test_t *test_ptr)
{
    wlmtk_fake_surface_t *fs_ptr = wlmtk_fake_surface_create();
    BS_ASSERT(NULL != fs_ptr);

    wlmtk_content_t content;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_content_init(&content, NULL, NULL));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, content.surface_ptr);

    wlmtk_content_set_surface(&content, &fs_ptr->surface);
    BS_TEST_VERIFY_EQ(test_ptr, &fs_ptr->surface, content.surface_ptr);

    wlmtk_content_set_surface(&content, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, content.surface_ptr);

    wlmtk_content_fini(&content);
    wlmtk_fake_surface_destroy(fs_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests adding and removing popups. */
void test_add_remove_popup(bs_test_t *test_ptr)
{
    wlmtk_content_t parent, popup;

    wlmtk_fake_surface_t *fs0_ptr = wlmtk_fake_surface_create();
    wlmtk_fake_surface_commit_size(fs0_ptr, 100, 10);
    wlmtk_fake_surface_t *fs1_ptr = wlmtk_fake_surface_create();
    wlmtk_fake_surface_commit_size(fs1_ptr, 200, 20);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_content_init(&parent, &fs0_ptr->surface, NULL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_content_init(&popup, &fs1_ptr->surface, NULL));

    wlmtk_element_set_visible(wlmtk_content_element(&parent), true);
    wlmtk_element_set_visible(wlmtk_content_element(&popup), true);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_content_get_parent_content(&parent));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_content_get_parent_content(&popup));

    struct wlr_box box;
    box = wlmtk_element_get_dimensions_box(wlmtk_content_element(&parent));
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.height);

    wlmtk_content_add_popup(&parent, &popup);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &parent,
        wlmtk_content_get_parent_content(&popup));

    box = wlmtk_element_get_dimensions_box(wlmtk_content_element(&parent));
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.height);

    wlmtk_content_remove_popup(&parent, &popup);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_content_get_parent_content(&popup));
}

/* == End of content.c ===================================================== */
