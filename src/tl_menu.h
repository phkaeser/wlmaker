/* ========================================================================= */
/**
 * @file tl_menu.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMAKER_TL_MENU_H__
#define __WLMAKER_TL_MENU_H__

#include "toolkit/toolkit.h"

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of a toplevel's menu. */
typedef struct _wlmaker_tl_menu_t wlmaker_tl_menu_t;

/**
 * Creates a (window) menu for a toplevel (window).
 *
 * @param window_ptr
 * @param server_ptr
 *
 * @return pointer to the toplevel's menu state or NULL on error.
 */
wlmaker_tl_menu_t *wlmaker_tl_menu_create(
    wlmtk_window2_t *window_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the toplevel's menu.
 *
 * @param tl_menu_ptr
 */
void wlmaker_tl_menu_destroy(wlmaker_tl_menu_t *tl_menu_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TL_MENU_H__ */
/* == End of tl_menu.h ===================================================== */
