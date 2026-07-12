/* ========================================================================= */
/**
 * @file dock.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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
#ifndef __WLMAKER_DOCK_DOCK_H__
#define __WLMAKER_DOCK_DOCK_H__

#include <toolkit/toolkit.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct _wlmdk_dock wlmdk_dock_t;

wlmdk_dock_t *wlmdk_dock_create(
    wlmtk_box_orientation_t orientation,
    const struct wlmtk_dock_style *style_ptr);

wlmtk_element_t *wlmdk_dock_element(wlmdk_dock_t *dock_ptr);

void wlmdk_dock_destroy(wlmdk_dock_t *dock_ptr);

void wlmdk_dock_add_tile(wlmdk_dock_t *dock_ptr,
                         wlmtk_tile_t *tile_ptr);

void wlmdk_dock_remove_tile(wlmdk_dock_t *dock_ptr,
                            wlmtk_tile_t *tile_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_DOCK_DOCK_H__
/* == End of dock.h ====================================================== */
