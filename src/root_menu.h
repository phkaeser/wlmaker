/* ========================================================================= */
/**
 * @file root_menu.h
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
#ifndef __WLMAKER_ROOT_MENU_H__
#define __WLMAKER_ROOT_MENU_H__

#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of root menu. */
typedef struct _wlmaker_root_menu_t wlmaker_root_menu_t;

/**
 * Creates a root menu.
 *
 * @param window_style_ptr
 * @param menu_style_ptr
 * @param env_ptr
 *
 * @return Handle of the root menu, or NULL on error.
 */
wlmaker_root_menu_t *wlmaker_root_menu_create(
    const wlmtk_window_style_t *window_style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the root menu.
 *
 * @param root_menu_ptr
 */
void wlmaker_root_menu_destroy(wlmaker_root_menu_t *root_menu_ptr);

/** @return the window holding the root menu. */
wlmtk_window_t *wlmaker_root_menu_window(wlmaker_root_menu_t *root_menu_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ROOT_MENU_H__ */
/* == End of root_menu.h =================================================== */
