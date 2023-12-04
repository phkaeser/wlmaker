/* ========================================================================= */
/**
 * @file bordered.c
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

#include "bordered.h"

/* == Declarations ========================================================= */

static void _wlmtk_bordered_container_update_layout(
    wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Virtual method table: @ref wlmtk_container_t at @ref wlmtk_bordered_t. */
static const wlmtk_container_vmt_t bordered_container_vmt = {
    .update_layout = _wlmtk_bordered_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_bordered_init(wlmtk_bordered_t *bordered_ptr,
                         wlmtk_env_t *env_ptr,
                         wlmtk_element_t *element_ptr,
                         const wlmtk_margin_style_t *style_ptr)
{
    BS_ASSERT(NULL != bordered_ptr);
    memset(bordered_ptr, 0, sizeof(wlmtk_bordered_t));
    if (!wlmtk_container_init(&bordered_ptr->super_container, env_ptr)) {
        return false;
    }
    bordered_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &bordered_ptr->super_container, &bordered_container_vmt);
    memcpy(&bordered_ptr->style, style_ptr, sizeof(wlmtk_margin_style_t));

    bordered_ptr->element_ptr = element_ptr;
    wlmtk_container_add_element(&bordered_ptr->super_container,
                                bordered_ptr->element_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_bordered_fini(wlmtk_bordered_t *bordered_ptr)
{
    wlmtk_container_remove_element(&bordered_ptr->super_container,
                                   bordered_ptr->element_ptr);
    wlmtk_container_fini(&bordered_ptr->super_container);
    memset(bordered_ptr, 0, sizeof(wlmtk_bordered_t));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Updates the layout of the bordered element.
 *
 * @param container_ptr
 */
void _wlmtk_bordered_container_update_layout(
    wlmtk_container_t *container_ptr)
{
    wlmtk_bordered_t *bordered_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_bordered_t, super_container);

    bordered_ptr->orig_super_container_vmt.update_layout(container_ptr);

    // configure parent container.
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout(
            container_ptr->super_element.parent_container_ptr);
    }
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_bordered_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/** Style used for tests. */
static const wlmtk_margin_style_t test_style = {
    .width = 2,
    .color = 0xff000000
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();

    wlmtk_bordered_t bordered;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_bordered_init(
                            &bordered, NULL, &fe_ptr->element, &test_style));
    wlmtk_bordered_fini(&bordered);

    wlmtk_element_destroy(&fe_ptr->element);
}


/* == End of bordered.c ==================================================== */
