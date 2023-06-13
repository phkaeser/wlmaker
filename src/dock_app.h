/* ========================================================================= */
/**
 * @file dock_app.h
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
 *
 * An application attached to the dock.
 *
 * More verbosely: Handlers and status for the interactive element describing
 * an application attached to the wlmaker dock. Used to launch applications
 * conveniently.
 */
#ifndef __DOCK_APP_H__
#define __DOCK_APP_H__

/** Forward declaration: State of the dock-attached application. */
typedef struct _wlmaker_dock_app_t wlmaker_dock_app_t;

#include "interactive.h"
#include "view.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Configuration of an attached application. */
typedef struct {
    /** Application ID, as used in Wayland. */
    char                      *app_id_ptr;
    /** Commandline. Will be tokenized, first token is the executable. */
    char                      *cmdline_ptr;
    /** Path to an icon file. */
    char                      *icon_path_ptr;
} wlmaker_dock_app_config_t;

/**
 * Creates an application attached to the dock.
 *
 * @param view_ptr
 * @param wlr_scene_tree_ptr
 * @param x                   X-Position relative to parent.
 * @param y                   Y-Position relative to parent.
 * @param dock_app_config_ptr Configuration of the docked application. Must
 *     outlive the dock app.
 *
 * @return The handle for the attached application or NULL on error.
 */
wlmaker_dock_app_t *wlmaker_dock_app_create(
    wlmaker_view_t *view_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    int x, int y,
    wlmaker_dock_app_config_t *dock_app_config_ptr);

/**
 * Destroys the application.
 *
 * @param dock_app_ptr
 */
void wlmaker_dock_app_destroy(wlmaker_dock_app_t *dock_app_ptr);

/**
 * Type cast: Returns the `wlmaker_dock_app_t` from the dlnode.
 *
 * @param dlnode_ptr
 *
 * @return A pointer to the `wlmaker_dock_app_t` holding `bs_dllist_node_t`.
 */
wlmaker_dock_app_t *wlmaker_dock_app_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Type cast: Returns the dlnode from |dock_app_ptr|.
 *
 * @param dock_app_ptr
 *
 * @return A pointer to the `bs_dllist_node_t` of `wlmaker_dock_app_t`.
 */
bs_dllist_node_t *wlmaker_dlnode_from_dock_app(
    wlmaker_dock_app_t *dock_app_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __DOCK_APP_H__ */
/* == End of dock_app.h ==================================================== */
