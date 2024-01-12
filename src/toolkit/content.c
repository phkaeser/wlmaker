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

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
uint32_t wlmtk_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    return wlmtk_surface_request_size(content_ptr->surface_ptr, width, height);
}

/* ------------------------------------------------------------------------- */
void wlmtk_content_request_close(wlmtk_content_t *content_ptr)
{
    wlmtk_surface_request_close(content_ptr->surface_ptr);
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
    int width,
    int height)
{
    wlmtk_surface_commit_size(content_ptr->surface_ptr, serial, width, height);

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
    wlmtk_content_t content;
    struct wlr_box box;

    wlmtk_fake_surface_t *fs_ptr = wlmtk_fake_surface_create();

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_content_init(&content, &fs_ptr->surface, NULL));
    wlmtk_element_t *element_ptr = wlmtk_content_element(&content);

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
    wlmtk_surface_request_size(&fs_ptr->surface, 200, 100);
    wlmtk_fake_surface_commit(fs_ptr);
    box = wlmtk_element_get_dimensions_box(element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Pointer motion shouuld now report to be within the content.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, 10, 10, 0));

    wlmtk_content_fini(&content);
    wlmtk_fake_surface_destroy(fs_ptr);

}

/* == End of content.c ===================================================== */
