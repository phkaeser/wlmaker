/* ========================================================================= */
/**
 * @file workspace.h
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
#ifndef __WLMTK_WORKSPACE_H__
#define __WLMTK_WORKSPACE_H__

#include "container.h"
#include "window.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the workspace. */
typedef struct _wlmtk_workspace_t wlmtk_workspace_t;

/** Forward declaration. */
struct wlr_pointer_button_event;
/** Forward declaration. */
struct wlr_box;

/**
 * Creates a workspace.
 *
 * TODO(kaeser@gubbe.ch): Consider replacing the interface with a container,
 * and permit a "toplevel" container that will be at the server level.
 *
 * @param env_ptr
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the workspace state, or NULL on error. Must be free'd
 *     via @ref wlmtk_workspace_destroy.
 */
wlmtk_workspace_t *wlmtk_workspace_create(
    wlmtk_env_t *env_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/**
 * Destroys the workspace. Will destroy any stil-contained element.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr);

/**
 * Sets (or updates) the extents of the workspace.
 *
 * @param workspace_ptr
 * @param extents_ptr
 */
void wlmtk_workspace_set_extents(wlmtk_workspace_t *workspace_ptr,
                                 const struct wlr_box *extents_ptr);

/**
 * Returns the extents of the workspace available for maximized windows.
 *
 * @param workspace_ptr
 *
 * @return A `struct wlr_box` that lines out the available space and position.
 */
struct wlr_box wlmtk_workspace_get_maximize_extents(
    wlmtk_workspace_t *workspace_ptr);

/**
 * Maps the window: Adds it to the workspace container and makes it visible.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_map_window(wlmtk_workspace_t *workspace_ptr,
                                wlmtk_window_t *window_ptr);

/**
 * Unmaps the window: Sets it as invisible and removes it from the container.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr);

/**
 * Type conversion: Returns the @ref wlmtk_workspace_t from the container_ptr
 * pointing to wlmtk_workspace_t::super_container.
 *
 * Asserts that the container is indeed from a wlmtk_workspace_t.
 *
 * @return Pointer to the workspace.
 */
wlmtk_workspace_t *wlmtk_workspace_from_container(
    wlmtk_container_t *container_ptr);

/**
 * Handles a motion event.
 *
 * TODO(kaeser@gubbe.ch): Move this to the server, and have the workspace's
 * motion handling dealt with the element's pointer_motion method.
 *
 * @param workspace_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether there was an element under the pointer.
 */
bool wlmtk_workspace_motion(
    wlmtk_workspace_t *workspace_ptr,
    double x,
    double y,
    uint32_t time_msec);

/**
 * Handles a button event: Translates to button down/up/click/dblclick events.
 *
 * Each button activity (button pressed or released) will directly trigger a
 * corresponding BUTTON_DOWN or BUTTON_UP event. Depending on timing and
 * motion, a "released" event may also triccer a CLICK, DOUBLE_CLICK or
 * DRAG event.
 * These events will be forwarded to the event currently having pointer focus.
 *
 * TODO(kaeser@gubbe.ch): Implement DOUBLE_CLICK and DRAG events. Also, move
 * this code into the server and make it well tested.
 *
 * @param workspace_ptr
 * @param event_ptr
 */
void wlmtk_workspace_button(
    wlmtk_workspace_t *workspace_ptr,
    const struct wlr_pointer_button_event *event_ptr);

/**
 * Initiates a 'move' for the window.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_begin_window_move(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr);

/**
 * Initiates a 'resize' for the window.
 *
 * @param workspace_ptr
 * @param window_ptr
 * @param edges
 */
void wlmtk_workspace_begin_window_resize(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr,
    uint32_t edges);

/** Acticates `window_ptr`. Will de-activate an earlier window. */
void wlmtk_workspace_activate_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr);

/** Raises `window_ptr`: Will show it atop all other windows. */
void wlmtk_workspace_raise_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr);

/** Unit tests for the workspace. */
extern const bs_test_case_t wlmtk_workspace_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WORKSPACE_H__ */
/* == End of workspace.h =================================================== */
