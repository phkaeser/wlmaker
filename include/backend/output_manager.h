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

struct wl_display;
struct wlr_backend;
struct wlr_output_layout;
struct wlr_scene;

/** Forward declaration: Handle for output managers. */
typedef struct _wlmbe_output_manager_t wlmbe_output_manager_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Instantiates output managers for both `wlr-output-management-unstable-v1`
 * and `xdg-output-unstable-v1`. Both will listen for output changes in
 * `wlr_output_layout_ptr` and update the respective output configurations
 * is requested so.
 *
 * @param wl_display_ptr
 * @param wlr_scene_ptr
 * @param wlr_output_layout_ptr
 * @param wlr_backend_ptr
 *
 * @return Handle for the output managers, or NULL on error. Must be destroyed
 * by @ref wlmbe_output_manager_destroy.
 */
wlmbe_output_manager_t *wlmbe_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr);

/**
 * Destroy the output manager, returned from @ref wlmbe_output_manager_create.
 *
 * @param output_manager_ptr
 */
void wlmbe_output_manager_destroy(
    wlmbe_output_manager_t *output_manager_ptr);

/**
 * Scales all outputs by the provided scale factor.
 *
 * @param output_manager_ptr
 * @param scale
 */
void wlmbe_output_manager_scale(
    wlmbe_output_manager_t *output_manager_ptr,
    double scale);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_OUTPUT_MANAGER_H__ */
/* == End of output_manager.h ============================================== */
