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

/* == Declarations ========================================================= */

/* == Data ================================================================= */

void *wlmtk_content_identifier_ptr = wlmtk_content_init;

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

    BS_ASSERT(NULL != surface_ptr);
    wlmtk_container_add_element(
        &content_ptr->super_container,
        wlmtk_surface_element(surface_ptr));
    content_ptr->surface_ptr = surface_ptr;
    content_ptr->identifier_ptr = wlmtk_content_identifier_ptr;

    wlmtk_element_set_visible(wlmtk_surface_element(surface_ptr), true);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_fini(
    wlmtk_content_t *content_ptr)
{
    if (NULL != content_ptr->surface_ptr) {
        wlmtk_container_remove_element(
            &content_ptr->super_container,
            wlmtk_surface_element(content_ptr->surface_ptr));
        content_ptr->surface_ptr = NULL;
    }
    memset(content_ptr, 0, sizeof(wlmtk_content_t));
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

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmtk_surface_set_activated(content_ptr->surface_ptr, activated);
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr)
{
    wlmtk_surface_get_size(content_ptr->surface_ptr, width_ptr, height_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_commit_size(
    wlmtk_content_t *content_ptr,
    uint32_t serial,
    __UNUSED__ int width,
    __UNUSED__ int height)
{
    // FIXME: Replace this, the direct wlr_surface commit event will do that.
    wlmtk_surface_commit_size(
        content_ptr->surface_ptr, serial, width, height);

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

/* == Fake content, for tests ============================================== */

static void _wlmtk_fake_content_request_close(wlmtk_content_t *content_ptr);
static uint32_t _wlmtk_fake_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);

/** Virtual method table for the fake content. */
static wlmtk_content_vmt_t    _wlmtk_fake_content_vmt = {
    .request_size = _wlmtk_fake_content_request_size,
    .request_close = _wlmtk_fake_content_request_close,
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_content_t *wlmtk_fake_content_create(
    wlmtk_fake_surface_t *fake_surface_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_content_t));
    if (NULL == fake_content_ptr) return NULL;

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
    wlmtk_content_commit_size(
        &fake_content_ptr->content,
        fake_content_ptr->serial,
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

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_content_test_cases[] = {
    { 1, "init_fini", test_init_fini },
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

/* == End of content.c ===================================================== */
