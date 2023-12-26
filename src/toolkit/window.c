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

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_window_init(
    wlmtk_window_t *window_ptr,
    wlmtk_surface_t *surface_ptr,
    wlmtk_env_t *env_ptr)
{
    BS_ASSERT(NULL != window_ptr);
    memset(window_ptr, 0, sizeof(wlmtk_window_t));
    if (!wlmtk_container_init(&window_ptr->super_container, env_ptr)) {
        return false;
    }

    BS_ASSERT(NULL != surface_ptr);
    window_ptr->surface_ptr = surface_ptr;
    wlmtk_container_add_element(
        &window_ptr->super_container,
        wlmtk_surface_element(window_ptr->surface_ptr));

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_fini(
    wlmtk_window_t *window_ptr)
{
    if (NULL != window_ptr->surface_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->super_container,
            wlmtk_surface_element(window_ptr->surface_ptr));
        window_ptr->surface_ptr = NULL;
    }

    wlmtk_container_fini(&window_ptr->super_container);
    memset(window_ptr, 0, sizeof(wlmtk_window_t));
}

/* == Local (static) methods =============================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests initialization and un-initialization. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_window_t window;
    wlmtk_surface_t surface;
    wlmtk_surface_init(&surface, NULL, NULL);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_init(&window, &surface, NULL));
    wlmtk_window_fini(&window);

    wlmtk_surface_fini(&surface);
}

/* == End of window.c ====================================================== */
