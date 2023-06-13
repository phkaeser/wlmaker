/* ========================================================================= */
/**
 * @file titlebar.h
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
#ifndef __TITLEBAR_H__
#define __TITLEBAR_H__

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
 * Creates a titlebar interactive.
 *
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the button.
 * @param cursor_ptr          Cursor. Must outlive the titlebar.
 * @param view_ptr            View owning the titlebar. Must outlive titlebar.
 * @param titlebar_buffer_ptr WLR buffer, title bar texture when focussed. This
 *                            titlebar interactive will hold a consumer lock on
 *                            it.
 * @param titlebar_blurred_buffer_ptr WLR buffer, texture when blurred. This
 *                            titlebar interactive will hold a consumer lock.
 *
 * @return A pointer to the interactive. Must be destroyed via
 *     |_titlebar_destroy|.
 */
wlmaker_interactive_t *wlmaker_titlebar_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *titlebar_buffer_ptr,
    struct wlr_buffer *titlebar_blurred_buffer_ptr);

/**
 * Sets (replaces) the texture for the titlebar interactive.
 *
 * @param interactive_ptr
 * @param titlebar_buffer_ptr
 * @param titlebar_blurred_buffer_ptr
 */
void wlmaker_title_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *titlebar_buffer_ptr,
    struct wlr_buffer *titlebar_blurred_buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TITLEBAR_H__ */
/* == End of titlebar.h ==================================================== */
