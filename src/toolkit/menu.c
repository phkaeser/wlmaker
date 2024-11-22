/* ========================================================================= */
/**
 * @file menu.c
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

#include "menu.h"

#include "style.h"

/* == Declarations ========================================================= */

static void _wlmtk_menu_eliminate_item(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_menu_set_item_mode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_menu_init(
    wlmtk_menu_t *menu_ptr,
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    memset(menu_ptr, 0, sizeof(wlmtk_menu_t));
    menu_ptr->style = *style_ptr;

    if (!wlmtk_box_init(
            &menu_ptr->super_box,
            env_ptr,
            WLMTK_BOX_VERTICAL,
            &menu_ptr->style.margin)) {
        wlmtk_menu_fini(menu_ptr);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_fini(wlmtk_menu_t *menu_ptr)
{
    bs_dllist_for_each(
        &menu_ptr->items,
        _wlmtk_menu_eliminate_item,
        menu_ptr);
    wlmtk_box_fini(&menu_ptr->super_box);
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_set_mode(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_mode_t mode)
{
    if (menu_ptr->mode == mode) return;
    menu_ptr->mode = mode;
    bs_dllist_for_each(
        &menu_ptr->items,
        _wlmtk_menu_set_item_mode,
        menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_menu_element(wlmtk_menu_t *menu_ptr)
{
    return wlmtk_box_element(&menu_ptr->super_box);
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_add_item(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_item_t *menu_item_ptr)
{
    bs_dllist_push_back(
        &menu_ptr->items,
        wlmtk_dlnode_from_menu_item(menu_item_ptr));
    wlmtk_box_add_element_back(
        &menu_ptr->super_box,
        wlmtk_menu_item_element(menu_item_ptr));
    wlmtk_menu_item_set_mode(menu_item_ptr, menu_ptr->mode);
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_remove_item(wlmtk_menu_t *menu_ptr,
                            wlmtk_menu_item_t *menu_item_ptr)
{
    wlmtk_box_remove_element(
        &menu_ptr->super_box,
        wlmtk_menu_item_element(menu_item_ptr));
    bs_dllist_remove(
        &menu_ptr->items,
        wlmtk_dlnode_from_menu_item(menu_item_ptr));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Removes item from items, destroys it. */
void _wlmtk_menu_eliminate_item(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_menu_item_t *item_ptr = wlmtk_menu_item_from_dlnode(dlnode_ptr);
    wlmtk_menu_t *menu_ptr = ud_ptr;

    wlmtk_menu_remove_item(menu_ptr, item_ptr);
    wlmtk_element_destroy(wlmtk_menu_item_element(item_ptr));
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for bs_dllist_for_each: Sets the menu mode for the item.
 *
 * @param dlnode_ptr
 * @param ud_ptr
 */
void _wlmtk_menu_set_item_mode(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_menu_item_set_mode(
        wlmtk_menu_item_from_dlnode(dlnode_ptr),
        ((wlmtk_menu_t*)ud_ptr)->mode);
}

/* == Unit tests =========================================================== */

static void test_add_remove(bs_test_t *test_ptr);
static void test_set_mode(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_menu_test_cases[] = {
    { 1, "add_remove", test_add_remove },
    { 1, "set_mode", test_set_mode },
    { 0, NULL, NULL }
};


/* ------------------------------------------------------------------------- */
/** Tests adding and removing menu items. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_menu_t menu;
    wlmtk_menu_style_t s = {};
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_menu_init(&menu, &s, NULL));

    wlmtk_fake_menu_item_t *fi_ptr = wlmtk_fake_menu_item_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fi_ptr);
    wlmtk_menu_add_item(&menu, &fi_ptr->menu_item);
    wlmtk_menu_remove_item(&menu, &fi_ptr->menu_item);
    wlmtk_fake_menu_item_destroy(fi_ptr);

    // Adds another item. Must be destroyed during cleanup.
    fi_ptr = wlmtk_fake_menu_item_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fi_ptr);
    wlmtk_menu_add_item(&menu, &fi_ptr->menu_item);
    wlmtk_menu_fini(&menu);
}

/* ------------------------------------------------------------------------- */
/** Tests setting the menu's mode. */
void test_set_mode(bs_test_t *test_ptr)
{
    wlmtk_menu_t menu;
    wlmtk_menu_style_t s = {};
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_menu_init(&menu, &s, NULL));

    wlmtk_fake_menu_item_t *fi1_ptr = wlmtk_fake_menu_item_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fi1_ptr);
    wlmtk_menu_add_item(&menu, &fi1_ptr->menu_item);

    // Setting the mode must propagate.
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_NORMAL, fi1_ptr->menu_item.mode);
    wlmtk_menu_set_mode(&menu, WLMTK_MENU_MODE_RIGHTCLICK);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_RIGHTCLICK, fi1_ptr->menu_item.mode);

    // A new item must get the mode applied.
    wlmtk_fake_menu_item_t *fi2_ptr = wlmtk_fake_menu_item_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fi2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_NORMAL, fi2_ptr->menu_item.mode);
    wlmtk_menu_add_item(&menu, &fi2_ptr->menu_item);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_RIGHTCLICK, fi2_ptr->menu_item.mode);

    // Setting the mode must propagate to all.
    wlmtk_menu_set_mode(&menu, WLMTK_MENU_MODE_NORMAL);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_NORMAL, fi1_ptr->menu_item.mode);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_MODE_NORMAL, fi2_ptr->menu_item.mode);

    wlmtk_menu_fini(&menu);
}

/* == End of menu.c ======================================================== */
