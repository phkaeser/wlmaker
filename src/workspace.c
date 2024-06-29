/* ========================================================================= */
/**
 * @file workspace.c
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

#include "workspace.h"

#include "toolkit/toolkit.h"

#include <libbase/libbase.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE
#endif  // DOXYGEN_SHOULD_SKIP_THIS

/* == Declarations ========================================================= */

/** Data specific to one layer. */
typedef struct {
    /** Merely for reference: Which layer this constitutes. */
    wlmaker_workspace_layer_t layer;

    /** Scene graph subtree holding all nodes of this layer. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;
} wlmaker_workspace_layer_data_t;

/** Workspace state. */
struct _wlmaker_workspace_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Node of the `workspaces` element in @ref wlmaker_server_t. */
    bs_dllist_node_t          dlnode;

    /** Holds the `wlr_scene_rect` defining the background. */
    struct wlr_scene_rect     *background_wlr_scene_rect_ptr;

    /** Scene graph subtree holding all layers of this workspace. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Transitional: Link up to toolkit workspace. */
    wlmtk_workspace_t          *wlmtk_workspace_ptr;

    /** Data regarding each layer. */
    wlmaker_workspace_layer_data_t layers[WLMAKER_WORKSPACE_LAYER_NUM];

    /** Whether this workspace is currently enabled (visible) or not. */
    bool                      enabled;

    /** Index of this workspace. */
    int                       index;
    /** Name of this workspace. */
    char                      *name_ptr;

    /** Usable area of the workspace (output minus clip and dock). */
    struct wlr_box            usable_area;

};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_workspace_t *wlmaker_workspace_create(wlmaker_server_t *server_ptr,
                                              __UNUSED__ uint32_t color,
                                              int index,
                                              const char *name_ptr)
{
    wlmaker_workspace_t *workspace_ptr = logged_calloc(
        1, sizeof(wlmaker_workspace_t));
    if (NULL == workspace_ptr) return NULL;

    workspace_ptr->server_ptr = server_ptr;
    workspace_ptr->index = index;
    workspace_ptr->name_ptr = logged_strdup(name_ptr);
    if (NULL == workspace_ptr->name_ptr) {
        wlmaker_workspace_destroy(workspace_ptr);
        return NULL;
    }

    workspace_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        &server_ptr->wlr_scene_ptr->tree);
    if (NULL == workspace_ptr->wlr_scene_tree_ptr) {
        wlmaker_workspace_destroy(workspace_ptr);
        return NULL;
    }

    workspace_ptr->wlmtk_workspace_ptr = wlmtk_workspace_create(
        workspace_ptr->server_ptr->env_ptr, workspace_ptr->wlr_scene_tree_ptr);
    if (NULL == workspace_ptr->wlmtk_workspace_ptr) {
        wlmaker_workspace_destroy(workspace_ptr);
        return NULL;
    }
    struct wlr_box extents;
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr, NULL, &extents);
    wlmtk_workspace_set_extents(workspace_ptr->wlmtk_workspace_ptr, &extents);

    wlmtk_workspace_set_signals(
        workspace_ptr->wlmtk_workspace_ptr,
        &workspace_ptr->server_ptr->window_mapped_event,
        &workspace_ptr->server_ptr->window_unmapped_event);

    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_destroy(wlmaker_workspace_t *workspace_ptr)
{
    if (NULL != workspace_ptr->wlmtk_workspace_ptr) {
        wlmtk_workspace_destroy(workspace_ptr->wlmtk_workspace_ptr);
        workspace_ptr->wlmtk_workspace_ptr = NULL;
    }

    if (NULL != workspace_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&workspace_ptr->wlr_scene_tree_ptr->node);
    }

    if (NULL != workspace_ptr->name_ptr) {
        free(workspace_ptr->name_ptr);
        workspace_ptr->name_ptr = NULL;
    }
    free(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_set_extents(
    wlmaker_workspace_t *workspace_ptr,
    const struct wlr_box *extents_ptr)
{
    wlmtk_workspace_set_extents(workspace_ptr->wlmtk_workspace_ptr,
                                extents_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_get_details(
    wlmaker_workspace_t *workspace_ptr,
    int *index_ptr,
    const char **name_ptr_ptr)
{
    *index_ptr = workspace_ptr->index;
    *name_ptr_ptr = workspace_ptr->name_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_get_maximize_area(
    wlmaker_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr,
    struct wlr_box *maximize_area_ptr)
{
    struct wlr_box area;
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr,
        wlr_output_ptr,
        &area);
    wlr_box_intersection(
        maximize_area_ptr,
        &area,
        &workspace_ptr->usable_area);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_get_fullscreen_area(
    wlmaker_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr,
    struct wlr_box *fullscreen_area_ptr)
{
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr,
        wlr_output_ptr,
        fullscreen_area_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_workspace_t *wlmaker_workspace_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmaker_workspace_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_workspace(
    wlmaker_workspace_t *workspace_ptr)
{
    return &workspace_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmaker_workspace_wlmtk(wlmaker_workspace_t *workspace_ptr)
{
    return workspace_ptr->wlmtk_workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_set_enabled(wlmaker_workspace_t *workspace_ptr,
                                   bool enabled)
{
    workspace_ptr->enabled = enabled;
    wlr_scene_node_set_enabled(
        &workspace_ptr->wlr_scene_tree_ptr->node,
        workspace_ptr->enabled);
}

/* == Static (local) methods =============================================== */

/* == End of workspace.c =================================================== */
