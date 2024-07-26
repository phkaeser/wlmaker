/* ========================================================================= */
/**
 * @file simple_menu_item.h
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
#ifndef __WLMTK_SIMPLE_MENU_ITEM_H__
#define __WLMTK_SIMPLE_MENU_ITEM_H__

/** Forward declaration: State of simple menu item. */
typedef struct _wlmtk_simple_menu_item_t wlmtk_simple_menu_item_t;

#include "env.h"
#include "menu_item.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a simple menu item.
 *
 * @param text_ptr
 * @param env_ptr
 *
 * @return Pointer to the simple menu item state.
 */
wlmtk_simple_menu_item_t *wlmtk_simple_menu_item_create(
    const char *text_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the simple menu item.
 *
 * @param simple_menu_item_ptr
 */
void wlmtk_simple_menu_item_destroy(
    wlmtk_simple_menu_item_t *simple_menu_item_ptr);

/** Gets pointer to the superclass @ref wlmtk_menu_item_t. */
wlmtk_menu_item_t *wlmtk_simple_menu_item_menu_item(
    wlmtk_simple_menu_item_t *simple_menu_item_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SIMPLE_MENU_ITEM_H__ */
/* == End of simple_menu_item.h ============================================ */
