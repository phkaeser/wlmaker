/* ========================================================================= */
/**
 * @file window.h
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
#ifndef __WLMTK_WINDOW_H__
#define __WLMTK_WINDOW_H__

/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t;

#include "element.h"
#include "content.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a window for the given content.
 *
 * @param content_ptr         Will take ownership of content_ptr.
 *
 * @return Pointer to the window state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_window_destroy.
 */
wlmtk_window_t *wlmtk_window_create(wlmtk_content_t *content_ptr);

/**
 * Destroys the window.
 *
 * @param window_ptr
 */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr);

/**
 * Returns the super Element of the window.
 *
 * TODO(kaeser@gubbe.ch): Re-evaluate whether to work with accessors, or
 *     whether to keep the ancestry members public.
 *
 * @param window_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     window_ptr.
 */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr);

/**
 * Sets the window as activated, depending on the argument's value.
 *
 * An activated window will have keyboard focus and would have distinct
 * decorations to indicate state.
 *
 * @param window_ptr
 * @param activated
 */
void wlmtk_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated);

/**
 * Sets whether to have server-side decorations for this window.
 *
 * @param window_ptr
 * @param decorated
 */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated);

/**
 * Obtains the size of the window, including potential decorations.
 *
 * @param window_ptr
 * @param width_ptr           May be NULL.
 * @param height_ptr          May be NULL.
 */
void wlmtk_window_get_size(
    wlmtk_window_t *window_ptr,
    int *width_ptr,
    int *height_ptr);

/**
 * Requesta a new size for the window, including potential decorations.
 *
 * This may be implemented as an asynchronous operation.x
 *
 * @param window_ptr
 * @param width
 * @param height
 */
void wlmtk_window_request_size(
    wlmtk_window_t *window_ptr,
    int width,
    int height);

/**
 * Requests a move for the window.
 *
 * Requires the window to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_window_move.
 *
 * @param window_ptr
 */
void wlmtk_window_request_move(wlmtk_window_t *window_ptr);

/**
 * Requests the window to be resized.
 *
 * Requires the window to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_window_resize.
 *
 * @param window_ptr
 * @param edges
 */
void wlmtk_window_request_resize(wlmtk_window_t *window_ptr, uint32_t edges);

/** Unit tests for window. */
extern const bs_test_case_t wlmtk_window_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW_H__ */
/* == End of window.h ====================================================== */
