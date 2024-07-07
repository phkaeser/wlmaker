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

#include "env.h"

/** Forward declaration: Menu handle. */
typedef struct _wlmtk_menu_t wlmtk_menu_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the menu.
 *
 * @param env_ptr
 *
 * @return Pointer to the menu handle, or NULL on error.
 */
wlmtk_menu_t *wlmtk_menu_create(wlmtk_env_t *env_ptr);

/**
 * Destroys the menu.
 *
 * @param menu_ptr
 */
void wlmtk_menu_destroy(wlmtk_menu_t *menu_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_H__ */
/* == End of menu.h ======================================================== */
