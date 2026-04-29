/* ========================================================================= */
/**
 * @file item.c
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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

#include "item.h"

#include <stdlib.h>
#include <string.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>

/* == Declarations ========================================================= */

/** Abstract definition of a menu item. Can hold a submenu, or a file item. */
struct wlmtool_item {
    /** To prevent duplicate entries. Node of @ref wlmtool_menu::tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** Item of @ref wlmtool_menu::items. */
    bs_dllist_node_t          dlnode;
    /** Key used for @ref wlmtool_menu::tree_ptr. */
    const char                *key_ptr;
    /** Dtor for this item. */
    void (*destroy)(struct wlmtool_item *item_ptr);
    /** Constructs an array from this item. */
    bspl_array_t *(*create_array)(struct wlmtool_item *item_ptr);
};

/** Menu item: A menu. */
struct wlmtool_menu {
    /** Parent class: An item. */
    struct wlmtool_item       menu_item;
    /** To disambiguate duplicate entries, and to lookup by key. */
    bs_avltree_t              *tree_ptr;
    /** Items, through @ref wlmtool_item::dlnode. Defines the order. */
    bs_dllist_t               items;
    /** The menu's title. */
    bspl_string_t             *title_ptr;
};

/** Menu item: An action bound to a file. */
struct wlmtool_file_action {
    /** Parent class: An item. */
    struct wlmtool_item       menu_item;
    /** Plist array describing the action: (Title, Action, Path). */
    bspl_array_t              *array_ptr;
};

static void _wlmtool_item_destroy_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static int _wlmtool_item_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);
static int _wlmtool_item_dlnode_cmp(
    const bs_dllist_node_t *n1,
    const bs_dllist_node_t *n2);
static bool _wlmtool_item_dlnode_add_to_array(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static void _wlmtool_menu_destroy_item(struct wlmtool_item *item_ptr);
static bspl_array_t *_wlmtool_menu_create_array(
    struct wlmtool_item *item_ptr);

static void _wlmtool_file_action_destroy(struct wlmtool_item *item_ptr);
static bspl_array_t *_wlmtool_file_action_create_array(
    struct wlmtool_item *item_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmtool_item_destroy(struct wlmtool_item *item_ptr)
{
    item_ptr->destroy(item_ptr);
}

/* ------------------------------------------------------------------------- */
bspl_array_t *wlmtool_item_create_array(struct wlmtool_item *item_ptr)
{
    return item_ptr->create_array(item_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlmtool_menu *wlmtool_menu_create(const char *title_ptr)
{
    struct wlmtool_menu *menu_ptr = logged_calloc(1, sizeof(*menu_ptr));
    if (NULL == menu_ptr) return NULL;
    menu_ptr->menu_item.destroy = _wlmtool_menu_destroy_item;
    menu_ptr->menu_item.create_array = _wlmtool_menu_create_array;

    menu_ptr->tree_ptr = bs_avltree_create(_wlmtool_item_cmp, NULL);
    if (NULL == menu_ptr->tree_ptr) {
        wlmtool_item_destroy(&menu_ptr->menu_item);
        return NULL;
    }

    menu_ptr->title_ptr = bspl_string_create(title_ptr);
    if (NULL == menu_ptr->title_ptr) {
        wlmtool_item_destroy(&menu_ptr->menu_item);
        return NULL;
    }
    menu_ptr->menu_item.key_ptr = bspl_string_value(menu_ptr->title_ptr);

    return menu_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlmtool_item *wlmtool_item_from_menu(struct wlmtool_menu *menu_ptr)
{
    return &menu_ptr->menu_item;
}

/* ------------------------------------------------------------------------- */
/** Adds an item to the menu. */
bool wlmtool_menu_add_item(
    struct wlmtool_menu *menu_ptr,
    struct wlmtool_item *item_ptr)
{
    if (!bs_avltree_insert(menu_ptr->tree_ptr,
                           item_ptr->key_ptr,
                           &item_ptr->avlnode,
                           false)) return false;
    bs_dllist_push_back(&menu_ptr->items, &item_ptr->dlnode);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Returns a submenu for key, if it exists. If not, create it. */
struct wlmtool_menu *wlmtool_menu_get_or_create_submenu(
    struct wlmtool_menu *menu_ptr,
    const void *key_ptr)
{
    bs_avltree_node_t *a_ptr = bs_avltree_lookup(menu_ptr->tree_ptr, key_ptr);
    if (NULL != a_ptr) {
        return BS_CONTAINER_OF(a_ptr, struct wlmtool_menu, menu_item.avlnode);
    }

    struct wlmtool_menu *submenu_ptr = wlmtool_menu_create(key_ptr);
    if (NULL == submenu_ptr) return NULL;

    if (!wlmtool_menu_add_item(menu_ptr, &submenu_ptr->menu_item)) {
        bs_log(BS_ERROR, "Failed to add category \"%s\"",
               (const char*)key_ptr);
        wlmtool_item_destroy(&submenu_ptr->menu_item);
        return NULL;
    }

    return submenu_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlmtool_item *wlmtool_file_action_create(
    const char *title_ptr,
    const char *action_ptr,
    const char *resolved_path_ptr)
{
    struct wlmtool_file_action *item_ptr = logged_calloc(1, sizeof(*item_ptr));
    if (NULL == item_ptr) return NULL;
    item_ptr->menu_item.destroy = _wlmtool_file_action_destroy;
    item_ptr->menu_item.create_array = _wlmtool_file_action_create_array;

    item_ptr->array_ptr = bspl_array_create();
    if (NULL == item_ptr->array_ptr) goto error;

    bspl_string_t *s = bspl_string_create(title_ptr);
    if (NULL == s) goto error;
    bspl_array_push_back(item_ptr->array_ptr, bspl_object_from_string(s));
    bspl_string_unref(s);

    s = bspl_string_create(action_ptr);
    if (NULL == s) goto error;
    bspl_array_push_back(item_ptr->array_ptr, bspl_object_from_string(s));
    bspl_string_unref(s);

    s = bspl_string_create(resolved_path_ptr);
    if (NULL == s) goto error;
    item_ptr->menu_item.key_ptr = bspl_string_value(s);
    bspl_array_push_back(item_ptr->array_ptr, bspl_object_from_string(s));
    bspl_string_unref(s);

    return &item_ptr->menu_item;

error:
    _wlmtool_file_action_destroy(&item_ptr->menu_item);
    return NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_each(), destroy the item at `dlnode_ptr`. */
void _wlmtool_item_destroy_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct wlmtool_item *item_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmtool_item, dlnode);
    item_ptr->destroy(item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for the AVL tree. */
int _wlmtool_item_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr)
{
    struct wlmtool_item *item_ptr = BS_CONTAINER_OF(
        node_ptr, struct wlmtool_item, avlnode);
    return strcmp(item_ptr->key_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for the double-linked list. */
int _wlmtool_item_dlnode_cmp(
    const bs_dllist_node_t *n1,
    const bs_dllist_node_t *n2)
{
    struct wlmtool_item *i1 = BS_CONTAINER_OF(n1, struct wlmtool_item, dlnode);
    struct wlmtool_item *i2 = BS_CONTAINER_OF(n2, struct wlmtool_item, dlnode);
    return strcmp(i1->key_ptr, i2->key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates array from item at `dlnode_ptr` and adds to array at `ud_ptr`. */
bool _wlmtool_item_dlnode_add_to_array(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlmtool_item *item_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmtool_item, dlnode);
    bspl_array_t *parent_array_ptr = ud_ptr;

    bspl_array_t *array_ptr = item_ptr->create_array(item_ptr);
    if (NULL == array_ptr) return false;

    bspl_array_push_back(parent_array_ptr, bspl_object_from_array(array_ptr));
    bspl_array_unref(array_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtool_item::destroy. */
void _wlmtool_menu_destroy_item(struct wlmtool_item *item_ptr)
{
    struct wlmtool_menu *menu_ptr = BS_CONTAINER_OF(
        item_ptr, struct wlmtool_menu, menu_item);

    if (NULL != menu_ptr->title_ptr) {
        bspl_string_unref(menu_ptr->title_ptr);
        menu_ptr->title_ptr = NULL;
    }

    if (NULL != menu_ptr->tree_ptr) {
        bs_avltree_destroy(menu_ptr->tree_ptr);
        menu_ptr->tree_ptr = NULL;
    }

    bs_dllist_for_each(&menu_ptr->items, _wlmtool_item_destroy_dlnode, NULL);
    free(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a plist array representing the menu and it's items.
 *
 * @note The menu's items will be sorted, meaning: will change order.
 *
 * @param item_ptr
 *
 * @return the plist array, or NULL on error.
 */
bspl_array_t *_wlmtool_menu_create_array(struct wlmtool_item *item_ptr)
{
    struct wlmtool_menu *menu_ptr = BS_CONTAINER_OF(
        item_ptr, struct wlmtool_menu, menu_item);

    bspl_array_t *array_ptr = bspl_array_create();
    if (NULL == array_ptr) return NULL;

    bspl_array_push_back(
        array_ptr, bspl_object_from_string(menu_ptr->title_ptr));
    bs_dllist_sort(&menu_ptr->items, _wlmtool_item_dlnode_cmp);
    if (bs_dllist_all(
            &menu_ptr->items,
            _wlmtool_item_dlnode_add_to_array,
            array_ptr)) return array_ptr;

    bs_log(BS_ERROR, "Failed to create plist array from menu \"%s\"",
           bspl_string_value(menu_ptr->title_ptr));
    bspl_array_unref(array_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtool_item::destroy. Dtor for the file item. */
void _wlmtool_file_action_destroy(struct wlmtool_item *item_ptr)
{
    struct wlmtool_file_action *file_item_ptr = BS_CONTAINER_OF(
        item_ptr, struct wlmtool_file_action, menu_item);

    if (NULL != file_item_ptr->array_ptr) {
        bspl_array_unref(file_item_ptr->array_ptr);
        file_item_ptr->array_ptr = NULL;
    }

    free(file_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** @return A plist array for the file action, or NULL on error. */
bspl_array_t *_wlmtool_file_action_create_array(
    struct wlmtool_item *item_ptr)
{
    struct wlmtool_file_action *file_item_ptr = BS_CONTAINER_OF(
        item_ptr, struct wlmtool_file_action, menu_item);
    return bspl_array_ref(file_item_ptr->array_ptr);

}

/* == Unit Tests =========================================================== */

static void _wlmaker_item_test_create(bs_test_t *test_ptr);
static void _wlmaker_item_test_array(bs_test_t *test_ptr);

/** Test cases for the menu item classes. */
static const bs_test_case_t   _wlmtool_item_test_cases[] = {
    { true, "create", _wlmaker_item_test_create },
    { true, "array", _wlmaker_item_test_array },
    BS_TEST_CASE_SENTINEL(),
};

const bs_test_set_t           wlmtool_item_test_set = BS_TEST_SET(
    true, "item", _wlmtool_item_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests item creation and destruction. */
void _wlmaker_item_test_create(bs_test_t *test_ptr)
{
    struct wlmtool_menu *menu_ptr = wlmtool_menu_create("Menu");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);

    struct wlmtool_menu *submenu_ptr = wlmtool_menu_get_or_create_submenu(
        menu_ptr, "Submenu");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, submenu_ptr);
    // Second "get_or_create" must return the same.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        submenu_ptr,
        wlmtool_menu_get_or_create_submenu(menu_ptr, "Submenu"));

    struct wlmtool_item *item_ptr = wlmtool_file_action_create("a", "b", "c");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, item_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtool_menu_add_item(menu_ptr, item_ptr));

    // Second addition with same "file" must fail.
    item_ptr = wlmtool_file_action_create("x", "y", "c");
    BS_TEST_VERIFY_FALSE_OR_RETURN(
        test_ptr, wlmtool_menu_add_item(menu_ptr, item_ptr));
    wlmtool_item_destroy(item_ptr);

    wlmtool_item_destroy(wlmtool_item_from_menu(menu_ptr));
}

/* ------------------------------------------------------------------------- */
/** Tests plist array creation. */
void _wlmaker_item_test_array(bs_test_t *test_ptr)
{
    struct wlmtool_menu *m = wlmtool_menu_create("Menu");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, m);
    struct wlmtool_item *i0 = wlmtool_file_action_create("a0", "b0", "c0");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i0);
    struct wlmtool_item *i1 = wlmtool_file_action_create("a1", "b1", "c1");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i1);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtool_menu_add_item(m, i0));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtool_menu_add_item(m, i1));

    bspl_array_t *a = wlmtool_item_create_array(wlmtool_item_from_menu(m));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, a);

    BS_TEST_VERIFY_STREQ(test_ptr, "Menu", bspl_array_string_value_at(a, 0));
    bspl_array_t *ai = bspl_array_from_object(bspl_array_at(a, 1));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ai);
    BS_TEST_VERIFY_STREQ(test_ptr, "a0", bspl_array_string_value_at(ai, 0));
    BS_TEST_VERIFY_STREQ(test_ptr, "b0", bspl_array_string_value_at(ai, 1));
    BS_TEST_VERIFY_STREQ(test_ptr, "c0", bspl_array_string_value_at(ai, 2));
    ai = bspl_array_from_object(bspl_array_at(a, 2));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ai);
    BS_TEST_VERIFY_STREQ(test_ptr, "a1", bspl_array_string_value_at(ai, 0));
    BS_TEST_VERIFY_STREQ(test_ptr, "b1", bspl_array_string_value_at(ai, 1));
    BS_TEST_VERIFY_STREQ(test_ptr, "c1", bspl_array_string_value_at(ai, 2));
    bspl_array_unref(a);

    wlmtool_item_destroy(wlmtool_item_from_menu(m));
}

/* == End of item.c ======================================================== */
