/* ========================================================================= */
/**
 * @file resizebar.h
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
#ifndef __RESIZEBAR_H__
#define __RESIZEBAR_H__

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
 * Creates a resizebar interactive.
 *
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the button.
 * @param cursor_ptr          Cursor. Must outlive the resizebar.
 * @param view_ptr            View owning the resizebar. Must outlive this.
 * @param resizebar_buffer_ptr WLR buffer, resize bar texture. This resizebar
 *                            interactive will hold a consumer lock on it.
 * @param resizebar_pressed_buffer_ptr WLR buffer, resize bar texture when
 *                            pressed.
 * @param edges               Edges that are controlled by this element.
 *
 * @return A pointer to the interactive. Must be destroyed via
 *     |_resizebar_destroy|.
 */
wlmaker_interactive_t *wlmaker_resizebar_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *resizebar_buffer_ptr,
    struct wlr_buffer *resizebar_pressed_buffer_ptr,
    uint32_t edges);

/**
 * Sets (replaces) the textures for the resizebar interactive.
 *
 * @param interactive_ptr
 * @param resizebar_buffer_ptr WLR buffer, resize bar texture. This resizebar
 *                            interactive will hold a consumer lock on it.
 * @param resizebar_pressed_buffer_ptr WLR buffer, resize bar texture when
 *                            pressed.
 */
void wlmaker_resizebar_set_textures(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *resizebar_buffer_ptr,
    struct wlr_buffer *resizebar_pressed_buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __RESIZEBAR_H__ */
/* == End of resizebar.h =================================================== */
