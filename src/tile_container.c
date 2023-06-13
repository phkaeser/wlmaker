/* ========================================================================= */
/**
 * @file tile_container.c
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

#include "tile_container.h"

#include "view.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of a tile container, holding @ref wlmaker_iconified_t. */
struct _wlmaker_tile_container_t {

    /** Base list that's holding all tiles of this container. */
    bs_dllist_t               tiles;

    /**
     * Scene graph subtree holding all tiles of this container.
     *
     * Invariant: Membership in `tiles` == membership in `wlr_scene_tree_ptr`.
     * */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Corresponding view. */
    // TODO(kaeser@gubbe.ch): Replace with a layer element.
    wlmaker_view_t            view;
};

static wlmaker_tile_container_t *wlmaker_tile_container_from_view(
    wlmaker_view_t *view_ptr);
static void tile_container_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr);
static void arrange_tiles(wlmaker_tile_container_t *tile_container_ptr);

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     tile_container_view_impl = {
    .set_activated = NULL,
    .get_size = tile_container_get_size
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_tile_container_t *wlmaker_tile_container_create(
    wlmaker_server_t *server_ptr,
    wlmaker_workspace_t *workspace_ptr)
{
    wlmaker_tile_container_t *tile_container_ptr = logged_calloc(
        1, sizeof(wlmaker_tile_container_t));
    if (NULL == tile_container_ptr) return NULL;

    tile_container_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        &server_ptr->void_wlr_scene_ptr->tree);
    if (NULL == tile_container_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_tree_create()");
        wlmaker_tile_container_destroy(tile_container_ptr);
        return NULL;
    }

    wlmaker_view_init(
        &tile_container_ptr->view,
        &tile_container_view_impl,
        server_ptr,
        NULL,  // wlr_surface_ptr.
        tile_container_ptr->wlr_scene_tree_ptr,
        NULL);  // send_close_callback.

    tile_container_ptr->view.anchor =
        WLMAKER_VIEW_ANCHOR_BOTTOM |WLMAKER_VIEW_ANCHOR_LEFT;

    wlmaker_view_map(
        &tile_container_ptr->view,
        workspace_ptr,
        WLMAKER_WORKSPACE_LAYER_TOP);
    return tile_container_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_tile_container_destroy(
    wlmaker_tile_container_t *tile_container_ptr)
{
    wlmaker_view_unmap(&tile_container_ptr->view);
    wlmaker_view_fini(&tile_container_ptr->view);

    if (NULL != tile_container_ptr->wlr_scene_tree_ptr) {
#if 0
        // TODO(kaeser@gubbe.ch): Verify this doesn't cause leaks.
        wlr_scene_node_destroy(&tile_container_ptr->wlr_scene_tree_ptr->node);
#endif
        tile_container_ptr->wlr_scene_tree_ptr = NULL;
    }
    free(tile_container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_tile_container_add(
    wlmaker_tile_container_t *tile_container_ptr,
    wlmaker_iconified_t *iconified_ptr)
{
    bs_dllist_node_t *dlnode_ptr = wlmaker_dlnode_from_iconified(iconified_ptr);
    BS_ASSERT(bs_dllist_node_orphaned(dlnode_ptr));
    bs_dllist_push_back(&tile_container_ptr->tiles, dlnode_ptr);

    struct wlr_scene_node *wlr_scene_node_ptr =
        wlmaker_wlr_scene_node_from_iconified(iconified_ptr);
    // TODO(kaeser@gubbe.ch): Rather ugly. Maybe have a "reparent" function
    // in iconified that updates the node.data field?
    wlr_scene_node_ptr->data = &tile_container_ptr->view;
    wlr_scene_node_reparent(
        wlr_scene_node_ptr,
        tile_container_ptr->wlr_scene_tree_ptr);
    wlr_scene_node_set_enabled(wlr_scene_node_ptr, true);

    bool inserted = bs_avltree_insert(
        tile_container_ptr->view.interactive_tree_ptr,
        wlr_scene_node_ptr,
        wlmaker_avlnode_from_iconified(iconified_ptr),
        false);
    BS_ASSERT(inserted);

    arrange_tiles(tile_container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_tile_container_remove(
    wlmaker_tile_container_t *tile_container_ptr,
    wlmaker_iconified_t *iconified_ptr)
{
    bs_dllist_node_t *dlnode_ptr = wlmaker_dlnode_from_iconified(iconified_ptr);
    BS_ASSERT(bs_dllist_contains(&tile_container_ptr->tiles, dlnode_ptr));
    bs_dllist_remove(&tile_container_ptr->tiles, dlnode_ptr);

    struct wlr_scene_node *wlr_scene_node_ptr =
        wlmaker_wlr_scene_node_from_iconified(iconified_ptr);
    wlr_scene_node_set_enabled(wlr_scene_node_ptr, false);
    wlr_scene_node_reparent(
        wlmaker_wlr_scene_node_from_iconified(iconified_ptr),
        &tile_container_ptr->view.server_ptr->void_wlr_scene_ptr->tree);
    // TODO(kaeser@gubbe.ch): Rather ugly. Maybe have a "reparent" function
    // in iconified that updates the node.data field?
    wlr_scene_node_ptr->data = NULL;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        tile_container_ptr->view.interactive_tree_ptr,
        wlr_scene_node_ptr);
    BS_ASSERT(avlnode_ptr == wlmaker_avlnode_from_iconified(iconified_ptr));

    arrange_tiles(tile_container_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Gets the @ref wlmaker_tile_container_t from the `view_ptr`.
 *
 * @param view_ptr
 */
wlmaker_tile_container_t *wlmaker_tile_container_from_view(
    wlmaker_view_t *view_ptr)
{
    return BS_CONTAINER_OF(view_ptr, wlmaker_tile_container_t, view);
}

/* ------------------------------------------------------------------------- */
/**
 * Retrieves the size of the tile container, including it's tiles.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
static void tile_container_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr)
{
    wlmaker_tile_container_t *tile_container_ptr =
        wlmaker_tile_container_from_view(view_ptr);
    if (NULL != width_ptr) {
        *width_ptr = bs_dllist_size(&tile_container_ptr->tiles) * 64;
    }
    if (NULL != height_ptr) *height_ptr = 64;
}

/* ------------------------------------------------------------------------- */
/**
 * Arrange the tiles, according to order from the `tiles` list.
 *
 * @param tile_container_ptr
 */
void arrange_tiles(wlmaker_tile_container_t *tile_container_ptr)
{
    uint32_t x = 0;
    for (bs_dllist_node_t *dlnode_ptr = tile_container_ptr->tiles.head_ptr;
         NULL != dlnode_ptr;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmaker_iconified_set_position(
            wlmaker_iconified_from_dlnode(dlnode_ptr), x, 0);
        x += 64;
    }
}

/* == End of tile_container.c ============================================== */
