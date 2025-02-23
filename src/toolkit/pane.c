/* ========================================================================= */
/**
 * @file pane.c
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

#include "pane.h"

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_pane_init(
    wlmtk_pane_t *pane_ptr,
    wlmtk_element_t *element_ptr,
    wlmtk_env_t *env_ptr)
{
    memset(pane_ptr, 0, sizeof(wlmtk_pane_t));
    BS_ASSERT(NULL != element_ptr);

    if (!wlmtk_container_init(&pane_ptr->super_container, env_ptr)) {
        wlmtk_pane_fini(pane_ptr);
        return false;
    }

    if (!wlmtk_container_init(&pane_ptr->popup_container, env_ptr)) {
        wlmtk_pane_fini(pane_ptr);
        return false;
    }
    wlmtk_element_set_visible(&pane_ptr->popup_container.super_element, true);

    wlmtk_container_add_element(&pane_ptr->super_container, element_ptr);
    wlmtk_container_add_element(
        &pane_ptr->super_container,
        &pane_ptr->popup_container.super_element);
    wlmtk_element_set_visible(element_ptr, true);
    pane_ptr->element_ptr = element_ptr;


    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_pane_fini(wlmtk_pane_t *pane_ptr)
{
    if (NULL != pane_ptr->element_ptr) {
        wlmtk_container_remove_element(
            &pane_ptr->super_container,
            &pane_ptr->popup_container.super_element);
        wlmtk_container_remove_element(
            &pane_ptr->super_container,
            pane_ptr->element_ptr);
        pane_ptr->element_ptr = NULL;
    }

    wlmtk_container_fini(&pane_ptr->popup_container);
    wlmtk_container_fini(&pane_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_pane_element(wlmtk_pane_t *pane_ptr)
{
    return &pane_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_pane_add_popup(wlmtk_pane_t *pane_ptr, wlmtk_pane_t *popup_ptr)
{
    wlmtk_container_add_element(
        &pane_ptr->popup_container,
        wlmtk_pane_element(popup_ptr));
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_pane_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_pane_t pane;
    wlmtk_fake_element_t *fe;

    fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_pane_init(&pane, &fe->element, NULL));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &pane.super_container.super_element,
        wlmtk_pane_element(&pane));

    wlmtk_pane_fini(&pane);
}

/* == End of pane.c ======================================================== */
