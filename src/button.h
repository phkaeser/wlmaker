/* ========================================================================= */
/**
 * @file button.h
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
#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "cursor.h"
#include "interactive.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a button interactive.
 *
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the button.
 * @param cursor_ptr
 * @param button_callback     Will be called if/when the button is clicked.
 * @param button_callback_arg Extra arg to |button_callback|.
 * @param button_released_ptr WLR buffer, button texture in "released" state.
 *                            The button will hold a consumer lock on it.
 * @param button_pressed_ptr  WLR buffer, button texture in "pressed" state.
 *                            The button will hold a consumer lock on it.
 * @param button_blurred_ptr  WLR buffer, button texture in "blurred" state.
 *                            The button will hold a consumer lock on it.
 *
 * @return A pointer to the interactive. Must be destroyed via
 *     |_button_destroy|.
 */
wlmaker_interactive_t *wlmaker_button_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t button_callback,
    void *button_callback_arg,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr);

/**
 * Sets (replaces) the textures for the button interactive.
 *
 * @param interactive_ptr
 * @param button_released_ptr WLR buffer, button texture in "released" state.
 *                            The button will hold a consumer lock on it.
 * @param button_pressed_ptr  WLR buffer, button texture in "pressed" state.
 *                            The button will hold a consumer lock on it.
 * @param button_blurred_ptr  WLR buffer, button texture in "blurred" state.
 *                            The button will hold a consumer lock on it.
 */
void wlmaker_button_set_textures(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __BUTTON_H__ */
/* == End of button.h ================================================== */
