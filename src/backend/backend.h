/* ========================================================================= */
/**
 * @file backend.h
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
#ifndef __WLMBE_BACKEND_H__
#define __WLMBE_BACKEND_H__

#include <conf/model.h>
#include <wayland-server-core.h>

struct wlr_backend;
struct wlr_compositor;
struct wlr_output_layout;
struct wlr_scene;

/** Forward declaration. */
typedef struct _wlmbe_backend_t wlmbe_backend_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

wlmbe_backend_t *wlmbe_backend_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    int width,
    int height,
    wlmcfg_dict_t *config_dict_ptr);

void wlmbe_backend_destroy(wlmbe_backend_t *backend_ptr);

size_t wlmbe_backend_outputs(wlmbe_backend_t *backend_ptr);

/**
 * Returns the primary output. Currently that is the first output found
 * in the output layout.
 *
 * @param output_manager_ptr
 *
 * @return A pointer to the `struct wlr_output` for the primary output.
 */
struct wlr_output *wlmbe_primary_output(
    struct wlr_output_layout *wlr_output_layout_ptr);

/**
 * Switches to the given virtual terminal, if a wlroots session is available.
 *
 * Logs if wlr_session_change_vt() fails, but ignores the errors.
 *
 * @param server_ptr
 * @param vt_num
 */
void wlmbe_backend_switch_to_vt(wlmbe_backend_t *backend_ptr, unsigned vt_num);

/** Accessor. TODO(kaeser@gubbe.ch): Eliminate. */
struct wlr_backend *wlmbe_backend_wlr(wlmbe_backend_t *backend_ptr);
/** Accessor. TODO(kaeser@gubbe.ch): Eliminate. */
struct wlr_compositor *wlmbe_backend_compositor(wlmbe_backend_t *backend_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_BACKEND_H__ */
/* == End of backend.h ===================================================== */
