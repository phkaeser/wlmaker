/* ========================================================================= */
/**
 * @file item.h
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
#ifndef __WLMAKER_ITEM_H__
#define __WLMAKER_ITEM_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* == Declarations ========================================================= */

struct wlmtool_item;
struct wlmtool_menu;

/** Destroys the item. */
void wlmtool_item_destroy(struct wlmtool_item *item_ptr);

/** Returns a plist array representing the item (or menu). */
bspl_array_t *wlmtool_item_create_array(struct wlmtool_item *item_ptr);

/**
 * Creates a menu.
 *
 * @param title_ptr
 *
 * @return The menu, or NULL on failure. To release associates, the function
 *      @ref wlmtool_item_destroy must be called, on the menu's item
 *      (@see wlmtool_item_from_menu).
 */
struct wlmtool_menu *wlmtool_menu_create(const char *title_ptr);

/** @return the parent item handle for the menu. */
struct wlmtool_item *wlmtool_item_from_menu(struct wlmtool_menu *menu_ptr);

/**
 * Adds an item to this menu.
 *
 * @param menu_ptr
 * @param item_ptr            The item to add. The menu will take ownership.
 *
 * @return true on success.
 */
bool wlmtool_menu_add_item(
    struct wlmtool_menu *menu_ptr,
    struct wlmtool_item *item_ptr);

/**
 * Obtain (or create) the submenu with the provided key within a menu.
 *
 * @param menu_ptr
 * @param key_ptr
 *
 * @return The submenu, or NULL on error.
 */
struct wlmtool_menu *wlmtool_menu_get_or_create_submenu(
    struct wlmtool_menu *menu_ptr,
    const void *key_ptr);

/**
 * Creates a "file action" menu item.
 *
 * @param title_ptr            Title to use for the menu entry.
 * @param action_ptr          Action command to use.
 * @param resolved_path_ptr   Fully resolved path to the file to act on.
 *
 * @return Menu item, or NULL on return.
 */
struct wlmtool_item *wlmtool_file_action_create(
    const char *title_ptr,
    const char *action_ptr,
    const char *resolved_path_ptr);

/** Unit tests for the menu item classes. */
extern const bs_test_set_t wlmtool_item_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_ITEM_H__
/* == End of item.h =========================================================== */
