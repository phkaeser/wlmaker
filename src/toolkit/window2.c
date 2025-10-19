/* ========================================================================= */
/**
 * @file window2.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "window2.h"

#include "container.h"

/* == Declarations ========================================================= */

/** Window handle. */
struct _wlmtk_window2_t {
    /** Composed of : Holds decoration, popup container and content. */
    wlmtk_container_t         container;

    /** The content. */
    wlmtk_element_t           *content_element_ptr;

    /** Window style. */
    wlmtk_window_style_t      style;
    /** Menu style. */
    wlmtk_menu_style_t        menu_style;
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_create(
    wlmtk_element_t *content_element_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr)
{
    wlmtk_window2_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window2_t));
    if (NULL == window_ptr) return NULL;
    window_ptr->style = *style_ptr;
    window_ptr->menu_style = *menu_style_ptr;

    if (!wlmtk_container_init(&window_ptr->container)) {
        wlmtk_window2_destroy(window_ptr);
        return NULL;
    }

    wlmtk_container_add_element(&window_ptr->container, content_element_ptr);
    window_ptr->content_element_ptr = content_element_ptr;

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr)
{
    if (NULL != window_ptr->content_element_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->container,
            window_ptr->content_element_ptr);
        window_ptr->content_element_ptr = NULL;
    }
    wlmtk_container_fini(&window_ptr->container);

    free(window_ptr);
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window2_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
static void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);

    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *window_ptr = wlmtk_window2_create(&fe->element, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, window_ptr);

    wlmtk_window2_destroy(window_ptr);
}

/* == End of window2.c ===================================================== */
