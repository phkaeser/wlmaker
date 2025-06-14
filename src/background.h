/* ========================================================================= */
/**
 * @file background.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __BACKGROUND_H__
#define __BACKGROUND_H__

#include <stdint.h>

#include "toolkit/toolkit.h"

struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Background state. */
typedef struct _wlmaker_background_t wlmaker_background_t;

/**
 * Creates a background, derived from a @ref wlmtk_panel_t.
 *
 * @param workspace_ptr
 * @param wlr_output_layout_ptr
 * @param color
 *
 * @return A handle for the background, or NULL on error.
 */
wlmaker_background_t *wlmaker_background_create(
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    uint32_t color);

/**
 * Destroys the background.
 *
 * @param background_ptr
 */
void wlmaker_background_destroy(wlmaker_background_t *background_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __BACKGROUND_H__ */
/* == End of background.h ================================================== */
