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
 *
 * Interface for a workspace. A server has one or multiple workspaces, and
 * each workspace may hold an arbitrary number of views.
 */
#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

/** Forward definition: Workspace state. */
typedef struct _wlmaker_workspace_t wlmaker_workspace_t;
/** Forward definition: Workspace layer. */
typedef enum _wlmaker_workspace_layer_t wlmaker_workspace_layer_t;

/**
 * Indicates which layer the view shall be rendered in.
 *
 * This follows "wlr-layer-shell-unstable-v1-protocol.h", but adds an explicit
 * "shell" layer between "bottom" and "top". As specified in the layer protocol,
 * these are ordeder by z depth, bottom-most first.
 * wlroots suggests that "Fullscreen shell surfaces will typically be rendered
 * at the top layer". We'll actually render it in scene node placed just above
 * the top layer -- but won't report it as an extra layer.
 */
enum _wlmaker_workspace_layer_t {
    WLMAKER_WORKSPACE_LAYER_BACKGROUND = 0,
    WLMAKER_WORKSPACE_LAYER_BOTTOM = 1,
    WLMAKER_WORKSPACE_LAYER_SHELL = 2,
    WLMAKER_WORKSPACE_LAYER_TOP = 3,
    WLMAKER_WORKSPACE_LAYER_OVERLAY = 4,
};

#include "server.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Number of defined layers. Helpful ot iterate over layers 0...NUM. */
#define WLMAKER_WORKSPACE_LAYER_NUM (WLMAKER_WORKSPACE_LAYER_OVERLAY + 1)

/**
 * Creates a workspace.
 *
 * @param server_ptr
 * @param color
 * @param index
 * @param name_ptr
 *
 * @return Workspace handle or NULL on error. Must be destroyed with
 *     wlmaker_workspace_destroy().
 */
wlmaker_workspace_t *wlmaker_workspace_create(wlmaker_server_t *server_ptr,
                                              uint32_t color,
                                              int index,
                                              const char *name_ptr);

/**
 * Destroys a workspace.
 *
 * @param workspace_ptr
 */
void wlmaker_workspace_destroy(wlmaker_workspace_t *workspace_ptr);

/**
 * Sets this workspace as enabled.
 *
 * Expects that any other workspace has been disabled beforehand, otherwise
 * focus expectations will get wonky.
 *
 * @param workspace_ptr
 * @param enabled
 */
void wlmaker_workspace_set_enabled(wlmaker_workspace_t *workspace_ptr,
                                   bool enabled);

/**
 * Adds the view to a layer of the workspace.
 *
 * @param workspace_ptr
 * @param view_ptr
 * @param layer
 */
void wlmaker_workspace_add_view(wlmaker_workspace_t *workspace_ptr,
                                wlmaker_view_t *view_ptr,
                                wlmaker_workspace_layer_t layer);

/**
 * Removes the view from the workspace.
 *
 * If this view happened to be the currently-activated view: Will deactivate it
 * and activate the next view from the |views| stack of this workspace.
 *
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_remove_view(wlmaker_workspace_t *workspace_ptr,
                                   wlmaker_view_t *view_ptr);

/**
 * Raises the view to the top of the workspace. Does not change activation.
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_raise_view(
    __UNUSED__ wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Lowers the view to the bottom of the workspace. Does not change activation.
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_lower_view(
    __UNUSED__ wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Activates the view.
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_activate_view(wlmaker_workspace_t *workspace_ptr,
                                     wlmaker_view_t *view_ptr);

/**
 * Accessor: Gets the currently-activated view.
 *
 * @param workspace_ptr
 *
 * Returns The currently-activated view, or NULL if none.
 */
wlmaker_view_t *wlmaker_workspace_get_activated_view(
    wlmaker_workspace_t *workspace_ptr);

/**
 * Activates the view *after* the currently activated one.
 *
 * Intended to permit cycling through tasks. Will activate the view, but not
 * raise it. See @ref wlmaker_workspace_activate_previous_view.
 *
 * @param workspace_ptr
 */
void wlmaker_workspace_activate_next_view(
    wlmaker_workspace_t *workspace_ptr);

/**
 * Activates the view *before* the currently activated one.
 *
 * Intended to permit cycling through tasks. Will activate the view, but not
 * raise it. See @ref wlmaker_workspace_activate_next_view.
 *
 * @param workspace_ptr
 */
void wlmaker_workspace_activate_previous_view(
    wlmaker_workspace_t *workspace_ptr);

/**
 * Gets a pointer to the double-linked list holding all SHELL views.
 *
 * @param workspace_ptr
 *
 * @return The `bs_dllist_t`.
 */
const bs_dllist_t *wlmaker_workspace_get_views_dllist(
    wlmaker_workspace_t *workspace_ptr);

/**
 * Sets extents of the workspace.
 *
 * TODO(kaeser@gubbe.ch): Should re-trigger re-arranging.
 *
 * @param workspace_ptr
 * @param extents_ptr
 */
void wlmaker_workspace_set_extents(
    wlmaker_workspace_t *workspace_ptr,
    const struct wlr_box *extents_ptr);

/**
 * (Re)arranges the views in the workspace.
 *
 * This should be called whenever the output layout changes.
 *
 * @param workspace_ptr
 */
void wlmaker_workspace_arrange_views(wlmaker_workspace_t *workspace_ptr);

/**
 * Promotes |view_ptr| to the fullscreen layer. Will demote any view currently
 * on the fullscreen layer.
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_promote_view_to_fullscreen(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Demotes |view_ptr| from the fullscreen layer, moving it to the previously-
 * used layer.
 *
 * @param workspace_ptr
 * @param view_ptr
 */
void wlmaker_workspace_demote_view_from_fullscreen(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Retrieves the naming detalis of this workspace.
 *
 * @param workspace_ptr
 * @param index_ptr
 * @param name_ptr_ptr
 */
void wlmaker_workspace_get_details(
    wlmaker_workspace_t *workspace_ptr,
    int *index_ptr,
    const char **name_ptr_ptr);

/**
 * Gets the 'maximize' area for this workspace and outpout.
 *
 * @param workspace_ptr
 * @param wlr_output_ptr
 * @param maximize_area_ptr
 */
void wlmaker_workspace_get_maximize_area(
    wlmaker_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr,
    struct wlr_box *maximize_area_ptr);

/**
 * Gets the 'fullscreen' area for this workspace and outpout.
 *
 * @param workspace_ptr
 * @param wlr_output_ptr
 * @param fullscreen_area_ptr
 */
void wlmaker_workspace_get_fullscreen_area(
    wlmaker_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr,
    struct wlr_box *fullscreen_area_ptr);

/**
 * Cast: Returns a pointer to @ref wlmaker_workspace_t holding `dlnode_ptr`.
 *
 * @param dlnode_ptr          A pointer to the `dlnode` element.
 *
 * @return Pointer to the workspace holding the dlnode.
 */
wlmaker_workspace_t *wlmaker_workspace_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Cast: Returns a pointer to the `dlnode` element of `workspace_ptr`.
 *
 * @param workspace_ptr
 *
 * @return Pointer to the bs_dllist_node_t `dlnode` of `workspace_ptr`.
 */
bs_dllist_node_t *wlmaker_dlnode_from_workspace(
    wlmaker_workspace_t *workspace_ptr);

/** Transitional: Returns the @ref wlmtk_workspace_t. */
wlmtk_workspace_t *wlmaker_workspace_wlmtk(wlmaker_workspace_t *workspace_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmaker_workspace_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WORKSPACE_H__ */
/* == End of workspace.h =================================================== */
