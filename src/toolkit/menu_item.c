/* ========================================================================= */
/**
 * @file menu_item.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "menu_item.h"

/* == Declarations ========================================================= */

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmtk_menu_item_create(wlmtk_env_t *env_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = logged_calloc(
        1, sizeof(wlmtk_menu_item_t));
    if (NULL == menu_item_ptr) return NULL;

    if (!wlmtk_menu_item_init(menu_item_ptr, env_ptr)) {
        wlmtk_menu_item_destroy(menu_item_ptr);
        return NULL;
    }
    return menu_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_item_destroy(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmtk_menu_item_fini(menu_item_ptr);
    free(menu_item_ptr);
}

/* -------------------------------------------------------------------------*/
bool wlmtk_menu_item_init(wlmtk_menu_item_t *menu_item_ptr,
                          wlmtk_env_t *env_ptr)
{
    memset(menu_item_ptr, 0, sizeof(wlmtk_menu_item_t));

    if (!wlmtk_buffer_init(&menu_item_ptr->super_buffer, env_ptr)) {
        wlmtk_menu_item_fini(menu_item_ptr);
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------*/
void wlmtk_menu_item_fini(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmtk_buffer_fini(&menu_item_ptr->super_buffer);
}

/* -------------------------------------------------------------------------*/
bs_dllist_node_t *wlmtk_dlnode_from_menu_item(
    wlmtk_menu_item_t *menu_item_ptr)
{
    return &menu_item_ptr->dlnode;
}

/* -------------------------------------------------------------------------*/
wlmtk_menu_item_t *wlmtk_menu_item_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_menu_item_t, dlnode);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_menu_item_element(wlmtk_menu_item_t *menu_item_ptr)
{
    return wlmtk_buffer_element(&menu_item_ptr->super_buffer);
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_ctor_dtor(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_menu_item_test_cases[] = {
    { 1, "ctor_dtor", test_ctor_dtor },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor and a few accessors. */
void test_ctor_dtor(bs_test_t *test_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_item_ptr);

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_menu_item(menu_item_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, dlnode_ptr, &menu_item_ptr->dlnode);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr,
        wlmtk_menu_item_from_dlnode(dlnode_ptr));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &menu_item_ptr->super_buffer.super_element,
        wlmtk_menu_item_element(menu_item_ptr));

    wlmtk_menu_item_destroy(menu_item_ptr);
}

/* == End of menu_item.c =================================================== */
