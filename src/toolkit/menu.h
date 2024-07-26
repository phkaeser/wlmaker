/* ========================================================================= */
/**
 * @file menu.h
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
#ifndef __WLMTK_MENU_H__
#define __WLMTK_MENU_H__

/** Forward declaration: Menu handle. */
typedef struct _wlmtk_menu_t wlmtk_menu_t;

#include "env.h"
#include "menu_item.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Initializes the menu.
 *
 * @param menu_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_menu_init(wlmtk_menu_t *menu_ptr, wlmtk_env_t *env_ptr);

/**
 * Uninitializes the menu.
 *
 * @param menu_ptr
 */
void wlmtk_menu_fini(wlmtk_menu_t *menu_ptr);

/**
 * Adds a menu item to the menu.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_add_item(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_item_t *menu_item_ptr);

/**
 * Removes a menu item from the menu.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_remove_item(wlmtk_menu_t *menu_ptr,
                            wlmtk_menu_item_t *menu_item_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_menu_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_H__ */
/* == End of menu.h ======================================================== */
