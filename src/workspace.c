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

#include "tile_container.h"
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

    /**
     * Holds all mapped `wlmaker_layer_surface_t` which are mapped on this
     * layer and workspace. As it contains only the `wlmaker_layer_surface_t`
     * elements, it is a subset of the mapped views.
     */
    bs_dllist_t               layer_surfaces;
} wlmaker_workspace_layer_data_t;

/** Workspace state. */
struct _wlmaker_workspace_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Node of the `workspaces` element in @ref wlmaker_server_t. */
    bs_dllist_node_t          dlnode;

    /** Double-linked list of views on the SHELL layer of this workspace. */
    bs_dllist_t               views;
    /** Double-linked list of views on the other layers this workspace. */
    bs_dllist_t               layer_views;

    /** Container for iconified tiles. */
    wlmaker_tile_container_t  *tile_container_ptr;

    /** Holds the `wlr_scene_rect` defining the background. */
    struct wlr_scene_rect     *background_wlr_scene_rect_ptr;

    /** Scene graph subtree holding all layers of this workspace. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Transitional: Link up to toolkit workspace. */
    wlmtk_workspace_t          *wlmtk_workspace_ptr;

    /** Data regarding each layer. */
    wlmaker_workspace_layer_data_t layers[WLMAKER_WORKSPACE_LAYER_NUM];

    /** Scene graph subtree for fullscreen views. Holds at most one view. */
    struct wlr_scene_tree     *fullscreen_wlr_scene_tree_ptr;
    /** View currently at the fullscreen layer. May be NULL. */
    wlmaker_view_t            *fullscreen_view_ptr;
    /** Originating layer for the fullscreen view. */
    wlmaker_workspace_layer_t fullscreen_view_layer;

    /** Points to the currently-activated view, or NULL if none. */
    wlmaker_view_t            *activated_view_ptr;
    /** Whether this workspace is currently enabled (visible) or not. */
    bool                      enabled;

    /** Index of this workspace. */
    int                       index;
    /** Name of this workspace. */
    char                      *name_ptr;

    /** Usable area of the workspace (output minus clip and dock). */
    struct wlr_box            usable_area;

    /** Injeactable: replaces call to wlmaker_view_set_active. */
    void (*injectable_view_set_active)(wlmaker_view_t *view_ptr, bool active);
};

static void arrange_layers(wlmaker_workspace_t *workspace_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_workspace_t *wlmaker_workspace_create(wlmaker_server_t *server_ptr,
                                              uint32_t color,
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

    workspace_ptr->fullscreen_wlr_scene_tree_ptr =
        wlr_scene_tree_create(workspace_ptr->wlr_scene_tree_ptr);
    if (NULL == workspace_ptr->fullscreen_wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_tree_create()");
        wlmaker_workspace_destroy(workspace_ptr);
        return NULL;
    }

    for (int idx = 0; idx < WLMAKER_WORKSPACE_LAYER_NUM; ++idx) {
        workspace_ptr->layers[idx].layer = idx;
        workspace_ptr->layers[idx].wlr_scene_tree_ptr =
            wlr_scene_tree_create(workspace_ptr->wlr_scene_tree_ptr);
        if (NULL == workspace_ptr->layers[idx].wlr_scene_tree_ptr) {
            bs_log(BS_ERROR, "Failed wlr_scene_tree_create()");
            wlmaker_workspace_destroy(workspace_ptr);
            return NULL;
        }
        if (idx <= WLMAKER_WORKSPACE_LAYER_TOP) {
            wlr_scene_node_raise_to_top(
                &workspace_ptr->fullscreen_wlr_scene_tree_ptr->node);
        }
    }

    float fcolor[4];
    bs_gfxbuf_argb8888_to_floats(
        color, &fcolor[0], &fcolor[1], &fcolor[2], &fcolor[3]);
    workspace_ptr->background_wlr_scene_rect_ptr = wlr_scene_rect_create(
        workspace_ptr->layers[WLMAKER_WORKSPACE_LAYER_BACKGROUND].wlr_scene_tree_ptr,
        1, 1, fcolor);
    wlr_scene_node_set_position(
        &workspace_ptr->background_wlr_scene_rect_ptr->node, 0, 0);
    wlr_scene_node_set_enabled(
        &workspace_ptr->background_wlr_scene_rect_ptr->node, true);

    workspace_ptr->tile_container_ptr = wlmaker_tile_container_create(
        workspace_ptr->server_ptr, workspace_ptr);

    workspace_ptr->injectable_view_set_active = wlmaker_view_set_active;
    wlmaker_workspace_arrange_views(workspace_ptr);

#if defined(ENABLE_TOOLKIT_PROTOTYPE)
    // Transitional -- enable for prototyping: Toolkit-based workspace.
    workspace_ptr->wlmtk_workspace_ptr = wlmtk_workspace_create(
        workspace_ptr->wlr_scene_tree_ptr);
    if (NULL == workspace_ptr->wlmtk_workspace_ptr) {
        wlmaker_workspace_destroy(workspace_ptr);
        return NULL;
    }
    struct wlr_box extents;
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr, NULL, &extents);
    wlmtk_workspace_set_extents(workspace_ptr->wlmtk_workspace_ptr, &extents);
#endif  // defined(ENABLE_TOOLKIT_PROTOTYPE)

    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_destroy(wlmaker_workspace_t *workspace_ptr)
{
    if (NULL != workspace_ptr->tile_container_ptr) {
        wlmaker_tile_container_destroy(workspace_ptr->tile_container_ptr);
        workspace_ptr->tile_container_ptr = NULL;
    }

    for (bs_dllist_node_t *node_ptr = workspace_ptr->layer_views.head_ptr;
         node_ptr != NULL;
         node_ptr = node_ptr->next_ptr) {
        wlmaker_workspace_remove_view(workspace_ptr,
                                      wlmaker_view_from_dlnode(node_ptr));
    }
    for (bs_dllist_node_t *node_ptr = workspace_ptr->views.head_ptr;
         node_ptr != NULL;
         node_ptr = node_ptr->next_ptr) {
        wlmaker_workspace_remove_view(workspace_ptr,
                                      wlmaker_view_from_dlnode(node_ptr));
    }

    for (int idx = 0; idx < WLMAKER_WORKSPACE_LAYER_NUM; ++idx) {
        if (NULL != workspace_ptr->layers[idx].wlr_scene_tree_ptr) {
            wlr_scene_node_destroy(
                &workspace_ptr->layers[idx].wlr_scene_tree_ptr->node);
            workspace_ptr->layers[idx].wlr_scene_tree_ptr = NULL;
        }
    }

    if (NULL != workspace_ptr->fullscreen_wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(
            &workspace_ptr->fullscreen_wlr_scene_tree_ptr->node);
        workspace_ptr->fullscreen_wlr_scene_tree_ptr = NULL;
    }

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
void wlmaker_workspace_set_enabled(wlmaker_workspace_t *workspace_ptr,
                                   bool enabled)
{
    workspace_ptr->enabled = enabled;
    wlr_scene_node_set_enabled(
        &workspace_ptr->wlr_scene_tree_ptr->node,
        workspace_ptr->enabled);

    // Inactive workspaces should not have any activated views, update that.
    if (NULL != workspace_ptr->activated_view_ptr) {
        workspace_ptr->injectable_view_set_active(
            workspace_ptr->activated_view_ptr,
            workspace_ptr->enabled);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_add_view(wlmaker_workspace_t *workspace_ptr,
                                wlmaker_view_t *view_ptr,
                                wlmaker_workspace_layer_t layer)
{
    BS_ASSERT(0 <= layer && layer < WLMAKER_WORKSPACE_LAYER_NUM);

    if (layer == WLMAKER_WORKSPACE_LAYER_SHELL) {
        bs_dllist_push_front(&workspace_ptr->views,
                             wlmaker_dlnode_from_view(view_ptr));
    } else {
        bs_dllist_push_front(&workspace_ptr->layer_views,
                             wlmaker_dlnode_from_view(view_ptr));
    }

    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        workspace_ptr->layers[layer].wlr_scene_tree_ptr);
    wlr_scene_node_set_enabled(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        true);

    wlmaker_workspace_arrange_views(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_remove_view(wlmaker_workspace_t *workspace_ptr,
                                   wlmaker_view_t *view_ptr)
{
    if (view_ptr->iconified_ptr) {
        wlmaker_workspace_iconified_set_as_view(
            workspace_ptr, view_ptr->iconified_ptr);
    }

    if (workspace_ptr->fullscreen_view_ptr == view_ptr) {
        wlmaker_view_set_fullscreen(view_ptr, false);
        BS_ASSERT(NULL == workspace_ptr->fullscreen_view_ptr);
    }


    if (view_ptr->default_layer == WLMAKER_WORKSPACE_LAYER_SHELL) {
        bs_dllist_remove(&workspace_ptr->views,
                         wlmaker_dlnode_from_view(view_ptr));
    } else {
        bs_dllist_remove(&workspace_ptr->layer_views,
                         wlmaker_dlnode_from_view(view_ptr));
    }
    workspace_ptr->injectable_view_set_active(view_ptr, false);
    wlr_scene_node_set_enabled(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        false);
    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        &workspace_ptr->server_ptr->void_wlr_scene_ptr->tree);

    if (workspace_ptr->activated_view_ptr == view_ptr) {
        workspace_ptr->activated_view_ptr = NULL;
        for (bs_dllist_node_t *node_ptr = workspace_ptr->views.head_ptr;
             node_ptr != NULL;
             node_ptr = node_ptr->next_ptr) {
            wlmaker_view_t *node_view_ptr = wlmaker_view_from_dlnode(node_ptr);
            if (NULL != node_view_ptr->impl_ptr->set_activated) {
                wlmaker_workspace_activate_view(workspace_ptr, node_view_ptr);
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_raise_view(
    __UNUSED__ wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr)
{
    wlr_scene_node_raise_to_top(wlmaker_wlr_scene_node_from_view(view_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_lower_view(
    __UNUSED__ wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr)

{
    wlr_scene_node_lower_to_bottom(wlmaker_wlr_scene_node_from_view(view_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_activate_view(wlmaker_workspace_t *workspace_ptr,
                                     wlmaker_view_t *view_ptr)
{
    if (NULL == view_ptr->impl_ptr->set_activated) BS_ABORT();

    if (workspace_ptr->fullscreen_view_ptr != NULL &&
        workspace_ptr->fullscreen_view_ptr != view_ptr) {
        wlmaker_view_set_fullscreen(workspace_ptr->fullscreen_view_ptr, false);
    }

    if (workspace_ptr->activated_view_ptr == view_ptr) {
        // Nothing to do here. Just check if the keyboard focus matches.
        struct wlr_seat *seat_ptr = workspace_ptr->server_ptr->wlr_seat_ptr;
        if (NULL != seat_ptr) {
            BS_ASSERT(seat_ptr->keyboard_state.focused_surface ==
                      wlmaker_view_get_wlr_surface(view_ptr));
        }
        return;
    }

    if (NULL != workspace_ptr->activated_view_ptr) {
        workspace_ptr->injectable_view_set_active(
            workspace_ptr->activated_view_ptr, false);
    }

    workspace_ptr->activated_view_ptr = view_ptr;
    if (workspace_ptr->enabled) {
        workspace_ptr->injectable_view_set_active(view_ptr, true);
    }
}

/* ------------------------------------------------------------------------- */
wlmaker_view_t *wlmaker_workspace_get_activated_view(
    wlmaker_workspace_t *workspace_ptr)
{
    return workspace_ptr->activated_view_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_activate_next_view(
    wlmaker_workspace_t *workspace_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    if (NULL != workspace_ptr->activated_view_ptr) {
        dlnode_ptr = wlmaker_dlnode_from_view(
            workspace_ptr->activated_view_ptr);
        dlnode_ptr = dlnode_ptr->next_ptr;
        if (NULL == dlnode_ptr) {
            // Cycle through, if we reached the end.
            dlnode_ptr = workspace_ptr->views.head_ptr;
        }
    } else {
        dlnode_ptr = workspace_ptr->views.head_ptr;
    }
    if (NULL == dlnode_ptr) return;

    wlmaker_workspace_activate_view(
        workspace_ptr,
        wlmaker_view_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_activate_previous_view(
    wlmaker_workspace_t *workspace_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    if (NULL != workspace_ptr->activated_view_ptr) {
        dlnode_ptr = wlmaker_dlnode_from_view(
            workspace_ptr->activated_view_ptr);
        dlnode_ptr = dlnode_ptr->prev_ptr;
        if (NULL == dlnode_ptr) {
            // Cycle through, if we reached the beginning.
            dlnode_ptr = workspace_ptr->views.tail_ptr;
        }
    } else {
        dlnode_ptr = workspace_ptr->views.tail_ptr;
    }
    if (NULL == dlnode_ptr) return;

    wlmaker_workspace_activate_view(
        workspace_ptr,
        wlmaker_view_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
const bs_dllist_t *wlmaker_workspace_get_views_dllist(
    wlmaker_workspace_t *workspace_ptr)
{
    return &workspace_ptr->views;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_arrange_views(wlmaker_workspace_t *workspace_ptr)
{
    arrange_layers(workspace_ptr);

    struct wlr_box extents;
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr, NULL, &extents);

    if (0 < extents.width && 0 < extents.height) {
        wlr_scene_node_set_position(
            &workspace_ptr->background_wlr_scene_rect_ptr->node,
            extents.x, extents.y);
        wlr_scene_rect_set_size(
            workspace_ptr->background_wlr_scene_rect_ptr,
            extents.width, extents.height);
    }

    for (bs_dllist_node_t *dlnode_ptr = workspace_ptr->layer_views.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmaker_view_t *view_ptr = wlmaker_view_from_dlnode(dlnode_ptr);

        struct wlr_box bbox;
        wlmaker_view_get_position(view_ptr, &bbox.x, &bbox.y);
        uint32_t width, height;
        wlmaker_view_get_size(view_ptr, &width, &height);
        bbox.width = width;
        bbox.height = height;

        uint32_t anchor = wlmaker_view_get_anchor(view_ptr);
        if (anchor & WLMAKER_VIEW_ANCHOR_TOP) {
            bbox.y = extents.y;
        } else if (anchor & WLMAKER_VIEW_ANCHOR_BOTTOM) {
            bbox.y = extents.y + extents.height - bbox.height;
        }

        if (anchor & WLMAKER_VIEW_ANCHOR_LEFT) {
            bbox.x = extents.x;
        } else if (anchor & WLMAKER_VIEW_ANCHOR_RIGHT) {
            bbox.x = extents.x + extents.width - bbox.width;
        }
        wlmaker_view_set_position(view_ptr, bbox.x, bbox.y);
    }

    // As of 2022-11-27, clip and dock are hardcoded for size and anchoring,
    // so for now, we'll just adjust the usable area by that.
    workspace_ptr->usable_area.x = extents.x;
    workspace_ptr->usable_area.y = extents.y;
    workspace_ptr->usable_area.width = extents.width - 64;
    workspace_ptr->usable_area.height = extents.height - 64;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_promote_view_to_fullscreen(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr)
{
    BS_ASSERT(view_ptr->workspace_ptr == workspace_ptr);

    if (NULL != workspace_ptr->fullscreen_view_ptr) {
        wlmaker_view_set_fullscreen(workspace_ptr->fullscreen_view_ptr, false);
        BS_ASSERT(NULL == workspace_ptr->fullscreen_view_ptr);
    }

    // The fullscreen view should be active, to receive and handle events.
    wlmaker_workspace_activate_view(workspace_ptr, view_ptr);

    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        workspace_ptr->fullscreen_wlr_scene_tree_ptr);
    workspace_ptr->fullscreen_view_ptr = view_ptr;
    workspace_ptr->fullscreen_view_layer = WLMAKER_WORKSPACE_LAYER_SHELL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_demote_view_from_fullscreen(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr)
{
    // Nothing to do if |view_ptr| is not fullscreen.
    if (view_ptr != workspace_ptr->fullscreen_view_ptr) return;

    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(
            workspace_ptr->fullscreen_view_ptr),
        workspace_ptr->layers[
            workspace_ptr->fullscreen_view_layer].wlr_scene_tree_ptr);

    workspace_ptr->fullscreen_view_layer = WLMAKER_WORKSPACE_LAYER_NUM;
    workspace_ptr->fullscreen_view_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_view_set_as_iconified(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_view_t *view_ptr)
{
    if (view_ptr == workspace_ptr->activated_view_ptr) {
        workspace_ptr->injectable_view_set_active(view_ptr, false);
        workspace_ptr->activated_view_ptr = NULL;
    }
    BS_ASSERT(bs_dllist_contains(
                  &workspace_ptr->views,
                  wlmaker_dlnode_from_view(view_ptr)));

    bs_dllist_remove(
        &workspace_ptr->views,
        wlmaker_dlnode_from_view(view_ptr));

    wlr_scene_node_set_enabled(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        false);
    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        &workspace_ptr->server_ptr->void_wlr_scene_ptr->tree);

    wlmaker_iconified_t *iconified_ptr = wlmaker_iconified_create(view_ptr);
    wlmaker_tile_container_add(
        workspace_ptr->tile_container_ptr, iconified_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_iconified_set_as_view(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_iconified_t *iconified_ptr)
{
    wlmaker_tile_container_remove(
        workspace_ptr->tile_container_ptr, iconified_ptr);

    wlmaker_view_t *view_ptr = wlmaker_view_from_iconified(iconified_ptr);
    bs_dllist_push_front(
        &workspace_ptr->views,
        wlmaker_dlnode_from_view(view_ptr));
    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        workspace_ptr->layers[WLMAKER_WORKSPACE_LAYER_SHELL].wlr_scene_tree_ptr);
    wlr_scene_node_set_enabled(
        wlmaker_wlr_scene_node_from_view(view_ptr),
        true);

    wlmaker_iconified_destroy(iconified_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_layer_surface_add(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_workspace_layer_t layer,
    wlmaker_layer_surface_t *layer_surface_ptr)
{
    BS_ASSERT(0 <= layer);
    BS_ASSERT(layer < WLMAKER_WORKSPACE_LAYER_NUM);
    bs_dllist_push_back(
        &workspace_ptr->layers[layer].layer_surfaces,
        wlmaker_dlnode_from_layer_surface(layer_surface_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_layer_surface_remove(
    wlmaker_workspace_t *workspace_ptr,
    wlmaker_workspace_layer_t layer,
    wlmaker_layer_surface_t *layer_surface_ptr)
{
    BS_ASSERT(0 <= layer);
    BS_ASSERT(layer < WLMAKER_WORKSPACE_LAYER_NUM);
    bs_dllist_remove(
        &workspace_ptr->layers[layer].layer_surfaces,
        wlmaker_dlnode_from_layer_surface(layer_surface_ptr));
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
wlmaker_tile_container_t *wlmaker_workspace_get_tile_container(
    wlmaker_workspace_t *workspace_ptr)
{
    return workspace_ptr->tile_container_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmaker_workspace_wlmtk(wlmaker_workspace_t *workspace_ptr)
{
    return workspace_ptr->wlmtk_workspace_ptr;
}

/* == Static (local) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Arranges the `wlmaker_layer_surface_t` layer elements.
 *
 * @param workspace_ptr
 */
void arrange_layers(wlmaker_workspace_t *workspace_ptr)
{
    struct wlr_box extents;
    wlr_output_layout_get_box(
        workspace_ptr->server_ptr->wlr_output_layout_ptr, NULL, &extents);
    struct wlr_box usable_area = extents;

    for (int idx = 0; idx < WLMAKER_WORKSPACE_LAYER_NUM; ++idx) {
        wlmaker_workspace_layer_data_t *layer_data_ptr =
            &workspace_ptr->layers[idx];
        bs_dllist_node_t *dlnode_ptr;
        for (dlnode_ptr = layer_data_ptr->layer_surfaces.head_ptr;
             dlnode_ptr != NULL;
             dlnode_ptr = dlnode_ptr->next_ptr) {
            wlmaker_layer_surface_t *layer_surface_ptr =
                wlmaker_layer_surface_from_dlnode(dlnode_ptr);
            if (wlmaker_layer_surface_is_exclusive(layer_surface_ptr)) {
                wlmaker_layer_surface_configure(
                    layer_surface_ptr, &extents, &usable_area);
            }
        }

        for (dlnode_ptr = layer_data_ptr->layer_surfaces.head_ptr;
             dlnode_ptr != NULL;
             dlnode_ptr = dlnode_ptr->next_ptr) {
            wlmaker_layer_surface_t *layer_surface_ptr =
                wlmaker_layer_surface_from_dlnode(dlnode_ptr);
            if (!wlmaker_layer_surface_is_exclusive(layer_surface_ptr)) {
                wlmaker_layer_surface_configure(
                    layer_surface_ptr, &extents, &usable_area);
            }
        }

        // TODO(kaeser@gubbe.ch): We may have to update the node positions in
        // case the outputs are different. The layer nodes may not always be
        // positioned at (0, 0).
    }
}

/* == Unit tests =========================================================== */

/** Max fake calls. */
#define MAX_CALLS 10

static void test_single_view(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_workspace_test_cases[] = {
    { 1, "single_view", test_single_view },
    { 0, NULL, NULL }
};


/** Fake argument. */
typedef struct {
    /** 1st arg. */
    wlmaker_view_t            *view_ptr;
    /** 2nd arg. */
    bool                      active;
} fake_set_active_args_t;

/** Call record of fake calls. */
fake_set_active_args_t        fake_set_active_args[MAX_CALLS];
/** Call record counter. */
int                           fake_set_active_calls;

/** Fake call. */
void fake_set_active(wlmaker_view_t *view_ptr, bool active)
{
    fake_set_active_args[fake_set_active_calls].view_ptr = view_ptr;
    fake_set_active_args[fake_set_active_calls].active = active;
    fake_set_active_calls++;
}

/* ------------------------------------------------------------------------- */
/** Tests functionality when adding a single view. */
void test_single_view(bs_test_t *test_ptr)
{
    wlmaker_server_t server;
    memset(&server, 0, sizeof(wlmaker_server_t));
    server.wlr_scene_ptr = wlr_scene_create();
    server.void_wlr_scene_ptr = wlr_scene_create();

    wlmaker_workspace_t *workspace_ptr = wlmaker_workspace_create(
        &server, 0xff000000, 0, "Main");
    BS_TEST_VERIFY_NEQ(test_ptr, workspace_ptr, NULL);
    workspace_ptr->injectable_view_set_active = fake_set_active;
    fake_set_active_calls = 0;

    wlmaker_view_t view;
    memset(&view, 0, sizeof(wlmaker_view_t));
    view.elements_wlr_scene_tree_ptr = wlr_scene_tree_create(&server.wlr_scene_ptr->tree);
    wlmaker_workspace_add_view(workspace_ptr, &view, WLMAKER_WORKSPACE_LAYER_SHELL);

    // Check that activation calls into the view.
    wlmaker_workspace_activate_view(workspace_ptr, &view);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_calls, 1);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_args[0].view_ptr, &view);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_args[0].active, true);

    // Double activation does nothing.
    wlmaker_workspace_activate_view(workspace_ptr, &view);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_calls, 1);

    // Empty the nodes on destroy, will de-activate.
    wlmaker_workspace_destroy(workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_calls, 2);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_args[1].view_ptr, &view);
    BS_TEST_VERIFY_EQ(test_ptr, fake_set_active_args[1].active, false);
}

/* == End of workspace.c =================================================== */
