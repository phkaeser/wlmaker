/* ========================================================================= */
/**
 * @file workspace_manager.h
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
#ifndef __WLMAKER_WORKSPACE_MANAGER_H__
#define __WLMAKER_WORKSPACE_MANAGER_H__

#include <libbase/libbase.h>
#include <stdbool.h>

#include "toolkit/toolkit.h"

struct wl_display;
struct wlm_workspace_manager;
struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the workspace manager.
 *
 * @param wl_display_ptr
 * @param wlr_output_layout_ptr
 * @param desktop_ptr
 *
 * @return The workspace manager handle or NULL on error.
 */
struct wlm_workspace_manager *wlm_workspace_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_desktop_t *desktop_ptr);

/**
 * Destroys the workspace manager.
 *
 * @param workspace_manager_ptr
 */
void wlm_workspace_manager_destroy(
    struct wlm_workspace_manager *workspace_manager_ptr);

bool wlm_workspace_manager_apply_state(
    struct wlm_workspace_manager *workspace_manager_ptr,
    bspl_dict_t *state_dict_ptr);

/**
 * Adds a workspace.
 *
 * @param name_ptr
 *
 * @return true on success.
 */
bool wlm_workspace_manager_add_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr,
    const char *name_ptr);

/**
 * Destroys the last workspace. Unless it is active.
 *
 * @param name_ptr
 *
 * @return true on success.
 */
bool wlm_workspace_manager_destroy_last_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr);

/** Unit test set. */
extern const bs_test_set_t wlm_workspace_manager_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_WORKSPACE_MANAGER_H__
/* == End of workspace_manager.h =========================================== */
