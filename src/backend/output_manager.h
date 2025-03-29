/* ========================================================================= */
/**
 * @file output_manager.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMBE_OUTPUT_MANAGER_H__
#define __WLMBE_OUTPUT_MANAGER_H__

#include <wayland-server-core.h>

#include <conf/model.h>
#include <toolkit/toolkit.h>

struct wlr_allocator;
struct wlr_backend;

/** Forward declaration: Handle for output manager. */
typedef struct _wlmbe_output_manager_t wlmbe_output_manager_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Wraps two OMs.
wlmbe_output_manager_t *wlmbe_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr);

void wlmbe_output_manager_destroy(
    wlmbe_output_manager_t *output_manager_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_OUTPUT_MANAGER_H__ */
/* == End of output_manager.h ============================================== */
