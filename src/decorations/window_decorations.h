/* ========================================================================= */
/**
 * @file window_decorations.h
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
#ifndef __WINDOW_DECORATIONS_H__
#define __WINDOW_DECORATIONS_H__

/** Forward declaration: Handle for decorations for a window. */
typedef struct _wlmaker_window_decorations_t wlmaker_window_decorations_t;

#include "../view.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates window decorations for the provided window (view).
 *
 * Will create a margin, title bar and resize bar. Decorations should only
 * be created when the view (1) has decorations enabled, (2) is mapped
 * and (3) is not in fullscreen mode.
 *
 * => TODO: Take flags as to which elements are on. (resizing?
 *    menu bar elements?)
 *
 * @param view_ptr
 *
 * @return A handle for the window's decorations. Must be free'd by calling
 *     @ref wlmaker_window_decorations_destroy().
 */
wlmaker_window_decorations_t *wlmaker_window_decorations_create(
    wlmaker_view_t *view_ptr);

/**
 * Destroys the window decorations.
 *
 * @param decorations_ptr
 */
void wlmaker_window_decorations_destroy(
    wlmaker_window_decorations_t *decorations_ptr);

/**
 * Sets (or updates) the size of the decorated (inner) window.
 *
 * `width` and `height` are specifying the dimensions of the decorated window,
 * ie. without the added size of the decorations.
 *
 * @param decorations_ptr
 * @param width
 * @param height
 */
void wlmaker_window_decorations_set_inner_size(
    wlmaker_window_decorations_t *decorations_ptr,
    uint32_t width,
    uint32_t height);

/**
 * Retrieves the size added by the decoration.
 *
 * @param decorations_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmaker_window_decorations_get_added_size(
    wlmaker_window_decorations_t *decorations_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr);

/**
 * Returns the relative position of the decoration to the inner window.
 *
 * The top-left corner of the decoration of an inner window is placed at
 * `x-position of inner window` plus `*relx_ptr`. Same for Y position.
 *
 * @param decorations_ptr
 * @param relx_ptr
 * @param rely_ptr
 */
void wlmaker_window_decorations_relative_position(
    wlmaker_window_decorations_t *decorations_ptr,
    int *relx_ptr,
    int *rely_ptr);

/**
 * Updates the title used for the windo decoration. Wraps to titlebar.
 *
 * @param decorations_ptr
 */
void wlmaker_window_decorations_update_title(
    wlmaker_window_decorations_t *decorations_ptr);

/**
 * Sets the "shade" status for decorations. When shaded, the resizebar is
 * hidden.
 *
 * @param decorations_ptr
 * @param shaded
 */
void wlmaker_window_decorations_set_shade(
    wlmaker_window_decorations_t *decorations_ptr,
    bool shaded);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WINDOW_DECORATIONS_H__ */
/* == End of window_decorations.h ========================================== */
