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

#include <stdbool.h>
#include <stddef.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>

struct wl_display;
struct wlr_output_layout;
struct wlr_scene;

/** Forward declaration. */
typedef struct _wlmbe_backend_t wlmbe_backend_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the backend drivers.
 *
 * @param wl_display_ptr
 * @param wlr_scene_ptr
 * @param wlr_output_layout_ptr
 * @param width
 * @param height
 * @param config_dict_ptr
 * @param state_fname_ptr
 *
 * @return the server backend state, or NULL on error.
 */
wlmbe_backend_t *wlmbe_backend_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    int width,
    int height,
    bspl_dict_t *config_dict_ptr,
    const char *state_fname_ptr);

/**
 * Destroys the server backend.
 *
 * @param backend_ptr
 */
void wlmbe_backend_destroy(wlmbe_backend_t *backend_ptr);

/**
 * Switches to the given virtual terminal, if a wlroots session is available.
 *
 * Logs if wlr_session_change_vt() fails, but ignores the errors.
 *
 * @param backend_ptr
 * @param vt_num
 */
void wlmbe_backend_switch_to_vt(wlmbe_backend_t *backend_ptr, unsigned vt_num);

/** Accessor. TODO(kaeser@gubbe.ch): Eliminate. */
struct wlr_backend *wlmbe_backend_wlr(wlmbe_backend_t *backend_ptr);
/** Accessor. TODO(kaeser@gubbe.ch): Eliminate. */
struct wlr_compositor *wlmbe_backend_compositor(wlmbe_backend_t *backend_ptr);

/**
 * Returns the primary output. Currently that is the first output found
 * in the output layout.
 *
 * @param wlr_output_layout_ptr
 *
 * @return A pointer to the `struct wlr_output` for the primary output.
 */
struct wlr_output *wlmbe_primary_output(
    struct wlr_output_layout *wlr_output_layout_ptr);

/**
 * Returns the number of outputs active in the output layout.
 *
 * @param wlr_output_layout_ptr
 *
 * @return Number of outputs.
 */
size_t wlmbe_num_outputs(struct wlr_output_layout *wlr_output_layout_ptr);

/** Magnifies all backend outputs by @ref _wlmbke_backend_magnification. */
void wlmbe_backend_magnify(wlmbe_backend_t *backend_ptr);
/** Reduces all backend outputs by @ref _wlmbke_backend_magnification. */
void wlmbe_backend_reduce(wlmbe_backend_t *backend_ptr);

/** Saves the output's ephemeral state into the state file. */
bool wlmbe_backend_save_ephemeral_output_configs(wlmbe_backend_t *backend_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmbe_backend_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_BACKEND_H__ */
/* == End of backend.h ===================================================== */
