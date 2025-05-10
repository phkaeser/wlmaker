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


#include <stdbool.h>
#include <stdint.h>
#include <libbase/libbase.h>

/** State of the workspace. */
typedef struct _wlmtk_workspace_t wlmtk_workspace_t;

#include "element.h"
#include "env.h"
#include "layer.h"  // IWYU pragma: keep
#include "root.h"  // IWYU pragma: keep
#include "tile.h"
#include "window.h"  // IWYU pragma: keep

/** Forward declaration: wlr output layout. */
struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Indicates which layer the view shall be rendered in.
 *
 * See `enum layer` at:
 * https://wayland.app/protocols/wlr-layer-shell-unstable-v1.
 */
typedef enum {
    WLMTK_WORKSPACE_LAYER_BACKGROUND = 0,
    WLMTK_WORKSPACE_LAYER_BOTTOM = 1,
    WLMTK_WORKSPACE_LAYER_TOP = 3,
    WLMTK_WORKSPACE_LAYER_OVERLAY = 4,
} wlmtk_workspace_layer_t;

/**
 * Creates a workspace.
 *
 * @param wlr_output_layout_ptr Output layout. Must outlive the workspace.
 * @param name_ptr
 * @param tile_style_ptr
 * @param env_ptr
 *
 * @return Pointer to the workspace state, or NULL on error. Must be free'd
 *     via @ref wlmtk_workspace_destroy.
 */
wlmtk_workspace_t *wlmtk_workspace_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr,
    const wlmtk_tile_style_t *tile_style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the workspace. Will destroy any stil-contained element.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr);

/**
 * Sets or updates workspace details.
 *
 * @param workspace_ptr
 * @param index
 */
void wlmtk_workspace_set_details(
    wlmtk_workspace_t *workspace_ptr,
    int index);

/**
 * Retrieves the naming details of this workspace.
 *
 * @param workspace_ptr
 * @param name_ptr_ptr
 * @param index_ptr
 */
void wlmtk_workspace_get_details(
    wlmtk_workspace_t *workspace_ptr,
    const char **name_ptr_ptr,
    int *index_ptr);

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
 * Returns the extents of the workspace available for fullscreen windows.
 *
 * @param workspace_ptr
 *
 * @return A `struct wlr_box` that lines out the available space and position.
 */
struct wlr_box wlmtk_workspace_get_fullscreen_extents(
    wlmtk_workspace_t *workspace_ptr);

/**
 * Confines the window to remain entirely within workspace extents.
 *
 * A no-op if window_ptr is not mapped to workspace_ptr.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_confine_within(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr);

/**
 * Enabled or disables the workspace.
 *
 * An enabled workspace can have keyboard focus and activated windows. When
 * re-enabling a workspace, the formerly activated window will get re-activated
 * and re-gains keyboard focus.
 *
 * @param workspace_ptr
 * @param enabled
 */
void wlmtk_workspace_enable(wlmtk_workspace_t *workspace_ptr, bool enabled);

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
 * Returns pointer to the @ref wlmtk_layer_t handle serving `layer`.
 *
 * @param workspace_ptr
 * @param layer
 *
 * @return Pointer to the layer state.
 */
wlmtk_layer_t *wlmtk_workspace_get_layer(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_workspace_layer_t layer);


/**
 * Returns the `bs_dllist_t` of currently mapped windows.
 *
 * @param workspace_ptr
 *
 * @return A pointer to the list. Note that the list should not be manipulated
 *     directly. It's contents can change on @ref wlmtk_workspace_map_window or
 *     @ref wlmtk_workspace_unmap_window calls.
 */
bs_dllist_t *wlmtk_workspace_get_windows_dllist(
    wlmtk_workspace_t *workspace_ptr);

/**
 * Promotes the window to the fullscreen layer (or back).
 *
 * To be called by @ref wlmtk_window_commit_fullscreen.
 *
 * @param workspace_ptr
 * @param window_ptr
 * @param fullscreen
 */
void wlmtk_workspace_window_to_fullscreen(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr,
    bool fullscreen);

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

/** @return Pointer to the activated @ref wlmtk_window_t, if any. */
wlmtk_window_t *wlmtk_workspace_get_activated_window(
    wlmtk_workspace_t *workspace_ptr);

/**
 * Activates the @ref wlmtk_window_t *before* the currently activated one.
 *
 * Intended to permit cycling through tasks. Will activate the window, but not
 * raise it. See @ref wlmtk_workspace_activate_next_window.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_activate_previous_window(
    wlmtk_workspace_t *workspace_ptr);

/**
 * Activates the @ref wlmtk_window_t *after* the currently activated one.
 *
 * Intended to permit cycling through tasks. Will activate the window, but not
 * raise it. See @ref wlmtk_workspace_activate_previous_window.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_activate_next_window(
    wlmtk_workspace_t *workspace_ptr);

/** Raises `window_ptr`: Will show it atop all other windows. */
void wlmtk_workspace_raise_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr);

/** @return Pointer to wlmtk_workspace_t::super_container::super_element. */
wlmtk_element_t *wlmtk_workspace_element(wlmtk_workspace_t *workspace_ptr);

/** @return pointer to the anchor @ref wlmtk_root_t of `workspace_ptr`. */
wlmtk_root_t *wlmtk_workspace_get_root(wlmtk_workspace_t *workspace_ptr);

/**
 * Sets the anchor @ref wlmtk_root_t of `workspace_ptr`.
 *
 * @protected Must only be called from @ref wlmtk_root_t.
 *
 * @param workspace_ptr
 * @param root_ptr
 */
void wlmtk_workspace_set_root(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_root_t *root_ptr);

/** @return Pointer to @ref wlmtk_workspace_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_workspace(
    wlmtk_workspace_t *workspace_ptr);

/** @return Poitner to the @ref wlmtk_workspace_t of the `dlnode_ptr`. */
wlmtk_workspace_t *wlmtk_workspace_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/** Unit tests for the workspace. */
extern const bs_test_case_t wlmtk_workspace_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WORKSPACE_H__ */
/* == End of workspace.h =================================================== */
