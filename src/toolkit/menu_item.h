/* ========================================================================= */
/**
 * @file menu_item.h
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
#ifndef __WLMTK_MENU_ITEM_H__
#define __WLMTK_MENU_ITEM_H__

#include <libbase/libbase.h>

/** Forward declaration: State of the menu item. */
typedef struct _wlmtk_menu_item_t wlmtk_menu_item_t;

#include "env.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Create a menu item.
 *
 * @param env_ptr
 *
 * @return Pointer to the menu item, or NULL on error.
 */
wlmtk_menu_item_t *wlmtk_menu_item_create(wlmtk_env_t *env_ptr);

/**
 * Destroys the menu item.
 *
 * @param menu_item_ptr
 */
void wlmtk_menu_item_destroy(wlmtk_menu_item_t *menu_item_ptr);

/** Returns pointer to @ref wlmtk_menu_item_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_menu_item(
    wlmtk_menu_item_t *menu_item_ptr);

/** Returns the base @ref wlmtk_menu_item_t from `dlnode_ptr`. */
wlmtk_menu_item_t *wlmtk_menu_item_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_menu_item_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_ITEM_H__ */
/* == End of menu_item.h =================================================== */
