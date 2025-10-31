/* ========================================================================= */
/**
 * @file base.c
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

#include "base.h"

#include "test.h"

/* == Declarations ========================================================= */

static void _wlmtk_base_element_get_dimensions(
        wlmtk_element_t *element_ptr,
        int *x1_ptr,
        int *y1_ptr,
        int *x2_ptr,
        int *y2_ptr);

/* == Data ================================================================= */

/** Virtual method table for the base's elemnt superclass. */
static const wlmtk_element_vmt_t _wlmtk_base_element_vmt = {
    .get_dimensions = _wlmtk_base_element_get_dimensions
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_base_init(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr)
{
    *base_ptr = (wlmtk_base_t){};
    if (!wlmtk_container_init(&base_ptr->super_container)) return false;
    base_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &base_ptr->super_container.super_element,
        &_wlmtk_base_element_vmt);

    if (NULL != element_ptr) {
        wlmtk_container_add_element_atop(
            &base_ptr->super_container, NULL, element_ptr);
        base_ptr->content_element_ptr = element_ptr;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_base_fini(wlmtk_base_t *base_ptr)
{
    if (NULL != base_ptr->content_element_ptr) {
        wlmtk_container_remove_element(
            &base_ptr->super_container,
            base_ptr->content_element_ptr);
        base_ptr->content_element_ptr = NULL;
    }

    wlmtk_container_fini(&base_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_base_element(wlmtk_base_t *base_ptr)
{
    return &base_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_base_push_element(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr)
{
    if (NULL != base_ptr->content_element_ptr) {
        BS_ASSERT(base_ptr->content_element_ptr != element_ptr);
    }

    wlmtk_container_add_element(
        &base_ptr->super_container,
        element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_base_pop_element(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr)
{
    if (NULL != base_ptr->content_element_ptr) {
        BS_ASSERT(base_ptr->content_element_ptr != element_ptr);
    }

    wlmtk_container_remove_element(
        &base_ptr->super_container,
        element_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Gets the base's dimensions: Relays it to the base element. */
void _wlmtk_base_element_get_dimensions(
        wlmtk_element_t *element_ptr,
        int *x1_ptr,
        int *y1_ptr,
        int *x2_ptr,
        int *y2_ptr)
{
    wlmtk_base_t *base_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_base_t, super_container.super_element);

    if (NULL == base_ptr->content_element_ptr) {
        if (NULL != x1_ptr) *x1_ptr = 0;
        if (NULL != y1_ptr) *y1_ptr = 0;
        if (NULL != x2_ptr) *x2_ptr = 0;
        if (NULL != y2_ptr) *y2_ptr = 0;
    } else {
        wlmtk_element_get_dimensions(
            base_ptr->content_element_ptr, x1_ptr, y1_ptr, x2_ptr, y2_ptr);
    }
}

/* == Unit Tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_base_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_base_t base;
    wlmtk_fake_element_t *fe1, *fe2;

    fe1 = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1);
    wlmtk_fake_element_set_dimensions(fe1, 20, 10);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_base_init(&base, &fe1->element));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &base.super_container.super_element,
        wlmtk_base_element(&base));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 20, 10,
        wlmtk_element_get_dimensions_box(wlmtk_base_element(&base)));

    // Push an element. It spans beyond the content. Must not show in
    // get_dimensions.
    fe2 = wlmtk_fake_element_create();
    wlmtk_fake_element_set_dimensions(fe2, 100, 200);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2);
    wlmtk_base_push_element(&base, &fe2->element);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 20, 10,
        wlmtk_element_get_dimensions_box(wlmtk_base_element(&base)));

    wlmtk_base_pop_element(&base, &fe2->element);
    wlmtk_element_destroy(&fe2->element);

    wlmtk_base_fini(&base);
    wlmtk_element_destroy(&fe1->element);
}

/* == End of base.c ======================================================== */
