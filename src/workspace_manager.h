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

struct wl_display;
struct wlmaker_workspace_manager;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the workspace manager.
 *
 * @param wl_display_ptr
 *
 * @return The workspace manager handle or NULL on error.
 */
struct wlmaker_workspace_manager *wlmaker_workspace_manager_create(
    struct wl_display *wl_display_ptr);

/**
 * Destroys the workspace manager.
 *
 * @param workspace_manager_ptr
 */
void wlmaker_workspace_manager_destroy(
    struct wlmaker_workspace_manager *workspace_manager_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_WORKSPACE_MANAGER_H__
/* == End of workspace_manager.h =========================================== */
