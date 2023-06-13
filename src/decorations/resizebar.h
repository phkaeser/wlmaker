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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "../view.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Resizebar of window decoration. */
typedef struct _wlmaker_decorations_resizebar_t wlmaker_decorations_resizebar_t;

/**
 * Creates the title bar for window decoration.
 *
 * @param wlr_scene_tree_ptr
 * @param width
 * @param height
 * @param view_ptr
 *
 * @return A resizebar handle, or NULL on error. Must be free-d by calling
 *     @ref wlmaker_decorations_resizebar_destroy.
 */
wlmaker_decorations_resizebar_t *wlmaker_decorations_resizebar_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    unsigned width,
    unsigned height,
    wlmaker_view_t *view_ptr);

/**
 * Destroys a window decoration resize bar.
 *
 * @param resizebar_ptr
 */
void wlmaker_decorations_resizebar_destroy(
    wlmaker_decorations_resizebar_t *resizebar_ptr);

/**
 * Sets the width of the resizebar.
 *
 * @param resizebar_ptr
 * @param width
 * @param height
 */
void wlmaker_decorations_resizebar_set_size(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    unsigned width,
    unsigned height);

/**
 * Returns the height of the resizebar. Includes the bottom margin.
 *
 * @param resizebar_ptr
 */
unsigned wlmaker_decorations_resizebar_get_height(
    wlmaker_decorations_resizebar_t *resizebar_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __RESIZEBAR_H__ */
/* == End of resizebar.h =================================================== */
