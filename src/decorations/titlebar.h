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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "../view.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Titlebar of window decoration. */
typedef struct _wlmaker_decorations_titlebar_t wlmaker_decorations_titlebar_t;

/**
 * Creates the title bar for window decoration.
 *
 * @param wlr_scene_tree_ptr
 * @param width
 * @param view_ptr
 *
 * @return A titlebar handle, or NULL on error. Must be free-d by calling
 *     @ref wlmaker_decorations_titlebar_destroy.
 */
wlmaker_decorations_titlebar_t *wlmaker_decorations_titlebar_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    unsigned width,
    wlmaker_view_t *view_ptr);

/**
 * Destroys a window decoration title bar.
 *
 * @param titlebar_ptr
 */
void wlmaker_decorations_titlebar_destroy(
    wlmaker_decorations_titlebar_t *titlebar_ptr);

/**
 * Sets the width of the titlebar.
 *
 * @param titlebar_ptr
 * @param width
 */
void wlmaker_decorations_titlebar_set_width(
    wlmaker_decorations_titlebar_t *titlebar_ptr,
    unsigned width);

/**
 * Returns the height of the titlebar. Includes the top margin.
 *
 * @param titlebar_ptr
 */
unsigned wlmaker_decorations_titlebar_get_height(
    wlmaker_decorations_titlebar_t *titlebar_ptr);

/**
 * Sets the title of the titlebar. Will pull it from the view.
 *
 * @param titlebar_ptr
 */
void wlmaker_decorations_titlebar_update_title(
    wlmaker_decorations_titlebar_t *titlebar_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TITLEBAR_H__ */
/* == End of titlebar.h ==================================================== */
