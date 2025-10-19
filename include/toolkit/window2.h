/* ========================================================================= */
/**
 * @file window2.h
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
#ifndef __WLMTK_WINDOW2_H__
#define __WLMTK_WINDOW2_H__

#include "element.h"
#include "menu.h"
#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Window. */
typedef struct _wlmtk_window2_t wlmtk_window2_t;

/**
 * Creates a window.
 *
 * @param content_element_ptr
 * @param style_ptr
 * @param menu_style_ptr
 *
 * @return The window handle, or NULL on error.
 */
wlmtk_window2_t *wlmtk_window2_create(
    wlmtk_element_t *content_element_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr);

/**
 * Destroys the window.
 *
 * @param window_ptr
 */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr);

/** Window unit test cases. */
extern const bs_test_case_t wlmtk_window2_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW2_H__ */
/* == End of window2.h ===================================================== */
