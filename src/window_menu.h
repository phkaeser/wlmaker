/* ========================================================================= */
/**
 * @file window_menu.h
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
#ifndef __WLMAKER_WINDOW_MENU_H__
#define __WLMAKER_WINDOW_MENU_H__

#include "toolkit/toolkit.h"

/** Forward declaration: State of the window menu. */
typedef struct _wlmaker_window_menu_t wlmaker_window_menu_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a window menu for the given window.
 *
 * @param window_ptr
 */
wlmaker_window_menu_t *wlmaker_window_menu_create(
    wlmtk_window_t *window_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WINDOW_MENU_H__ */
/* == End of window_menu.h ================================================= */
