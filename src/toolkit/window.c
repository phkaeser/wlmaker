/* ========================================================================= */
/**
 * @file window.c
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

#include "window.h"

#include "container.h"

/* == Declarations ========================================================= */

/** State of the window. */
struct _wlmtk_window_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;

    /** Content of this window. */
    wlmtk_content_t           *content_ptr;
};

static void window_container_destroy(wlmtk_container_t *container_ptr);

/** Method table for the container's virtual methods. */
const wlmtk_container_impl_t  window_container_impl = {
    .destroy = window_container_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create(wlmtk_content_t *content_ptr)
{
    wlmtk_window_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window_t));
    if (NULL == window_ptr) return NULL;

    if (!wlmtk_container_init(&window_ptr->super_container,
                              &window_container_impl)) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }

    wlmtk_container_add_element(
        &window_ptr->super_container,
        wlmtk_content_element(content_ptr));
    window_ptr->content_ptr = content_ptr;
    wlmtk_content_set_window(content_ptr, window_ptr);
    wlmtk_element_set_visible(wlmtk_content_element(content_ptr), true);
    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr)
{
    wlmtk_container_remove_element(
        &window_ptr->super_container,
        wlmtk_content_element(window_ptr->content_ptr));
    wlmtk_element_set_visible(
        wlmtk_content_element(window_ptr->content_ptr), false);
    wlmtk_content_set_window(window_ptr->content_ptr, NULL);

    if (NULL != window_ptr->content_ptr) {
        wlmtk_content_destroy(window_ptr->content_ptr);
        window_ptr->content_ptr = NULL;
    }

    wlmtk_container_fini(&window_ptr->super_container);
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr)
{
    return &window_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_active(
    wlmtk_window_t *window_ptr,
    bool active)
{
    wlmtk_content_set_active(window_ptr->content_ptr, active);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void window_container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window_t, super_container);
    wlmtk_window_destroy(window_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_set_active(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "set_active", test_set_active },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        &fake_content_ptr->content);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr,
                      fake_content_ptr->content.window_ptr);

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_set_active(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        &fake_content_ptr->content);

    wlmtk_window_set_active(window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->active);

    wlmtk_window_set_active(window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, fake_content_ptr->active);

    wlmtk_window_destroy(window_ptr);
}


/* == End of window.c ====================================================== */
