/* ========================================================================= */
/**
 * @file tilebox.c
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

#include "tilebox.h"

#include <stdbool.h>
#include <stdlib.h>
#include "libbase/libbase.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of the tile box. */
struct _wlmdock_tilebox {
    /** Root container. */
    wlmtk_container_t         container;
    /** Box holding the tiles. */
    wlmtk_box_t               tile_box;

    /** List of tiles, via @ref wlmtk_dlnode_from_tile. */
    bs_dllist_t               tiles;
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmdock_tilebox_t *wlmdock_tilebox_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmtk_box_orientation_t orientation,
    const struct wlmtk_dock_style *style_ptr)
{
    wlmdock_tilebox_t *tilebox_ptr = logged_calloc(1, sizeof(*tilebox_ptr));
    if (NULL == tilebox_ptr) return NULL;

    if (!wlmtk_box_init(
            &tilebox_ptr->tile_box,
            orientation,
            &style_ptr->margin)) {
        wlmdock_tilebox_destroy(tilebox_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(wlmtk_box_element(&tilebox_ptr->tile_box), true);

    if (!wlmtk_container_init_attached(
            &tilebox_ptr->container,
            wlr_scene_tree_ptr)) {
        wlmdock_tilebox_destroy(tilebox_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &tilebox_ptr->container,
        wlmtk_box_element(&tilebox_ptr->tile_box));
    wlmtk_element_set_visible(&tilebox_ptr->container.super_element, true);

    return tilebox_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_tilebox_destroy(wlmdock_tilebox_t *tilebox_ptr)
{
    if (wlmtk_box_element(&tilebox_ptr->tile_box)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &tilebox_ptr->container,
            wlmtk_box_element(&tilebox_ptr->tile_box));
    }
    wlmtk_container_fini(&tilebox_ptr->container);
    wlmtk_box_fini(&tilebox_ptr->tile_box);

    free(tilebox_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmdock_tilebox_element(wlmdock_tilebox_t *tilebox_ptr)
{
    return &tilebox_ptr->container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmdock_tilebox_add_tile(wlmdock_tilebox_t *tilebox_ptr,
                              wlmtk_tile_t *tile_ptr)
{
    // FIXME: Define growth orientation.
    BS_ASSERT(NULL == wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_add_element_back(
        &tilebox_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
    bs_dllist_push_back(
        &tilebox_ptr->tiles,
        wlmtk_dlnode_from_tile(tile_ptr));

    // FIXME: size update?
}

/* ------------------------------------------------------------------------- */
void wlmdock_tilebox_remove_tile(wlmdock_tilebox_t *tilebox_ptr,
                                 wlmtk_tile_t *tile_ptr)
{
    BS_ASSERT(
        &tilebox_ptr->tile_box.element_container ==
        wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_remove_element(
        &tilebox_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
    bs_dllist_remove(
        &tilebox_ptr->tiles,
        wlmtk_dlnode_from_tile(tile_ptr));

    // FIXME: size update?
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == End of tilebox.c ===================================================== */
