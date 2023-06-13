/* ========================================================================= */
/**
 * @file menu.h
 *
 * @copyright
 * Copyright 2023 Google LLC
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
#ifndef __MENU_H__
#define __MENU_H__

#include "cursor.h"
#include "interactive.h"
#include "menu_item.h"
#include "view.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a menu interactive.
 *
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the button.
 * @param cursor_ptr
 * @param view_ptr
 * @param descriptor_ptr
 * @param callback_ud_ptr     Argument to provide to item's callbacks.
 *
 * @return A pointer to the interactive. Must be destroyed via |_menu_destroy|.
 */
wlmaker_interactive_t *wlmaker_menu_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    const wlmaker_menu_item_descriptor_t *descriptor_ptr,
    void *callback_ud_ptr);

/**
 * Retrieves the size of the menu.
 *
 * @param interactive_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmaker_menu_get_size(
    wlmaker_interactive_t *interactive_ptr,
    uint32_t *width_ptr, uint32_t *height_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmaker_menu_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __MENU_H__ */
/* == End of menu.h ======================================================== */
