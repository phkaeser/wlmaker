/* ========================================================================= */
/**
 * @file submenu.h
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
#ifndef __WLMTK_SUBMENU_H__
#define __WLMTK_SUBMENU_H__

#include "menu.h"
#include "menu_item.h"
#include "popup_menu.h"

/** Forward declaration: State of the submenu. */
typedef struct _wlmtk_submenu_t wlmtk_submenu_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a submenu: A menu item that opens a sub-menu, in a separate popup.
 *
 * @apram style_ptr
 * @poaram env_ptr
 *
 * @return State of the submenu.
 */
wlmtk_submenu_t *wlmtk_submenu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_popup_menu_t *parent_pum_ptr);

/**
 * Destroys the submenu. Detaches the item from the parent, if still attached.
 *
 * @param submenu_ptr
 */
void wlmtk_submenu_destroy(wlmtk_submenu_t *submenu_ptr);

wlmtk_menu_t *wlmtk_submenu_menu(wlmtk_submenu_t *submenu_ptr);

wlmtk_menu_item_t *wlmtk_submenu_menu_item(wlmtk_submenu_t *submenu_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SUBMENU_H__ */
/* == End of submenu.h ===================================================== */
