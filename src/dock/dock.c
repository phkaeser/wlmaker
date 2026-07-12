/* ========================================================================= */
/**
 * @file dock.c
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

#include "dock.h"

/* == Declarations ========================================================= */

struct _wlmdk_dock {


    wlmtk_box_t               tile_box;

    bs_dllist_t               tiles;


};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmdk_dock_t *wlmdk_dock_create(
    wlmtk_box_orientation_t orientation,
    const struct wlmtk_dock_style *style_ptr)
{
    wlmdk_dock_t *dock_ptr = logged_calloc(1, sizeof(*dock_ptr));
    if (NULL == dock_ptr) return NULL;

    if (!wlmtk_box_init(
            &dock_ptr->tile_box,
            orientation,
            &style_ptr->margin)) {
        wlmdk_dock_destroy(dock_ptr);
        return NULL;
    }

    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdk_dock_destroy(wlmdk_dock_t *dock_ptr)
{
    wlmtk_box_fini(&dock_ptr->tile_box);
    free(dock_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmdk_dock_element(wlmdk_dock_t *dock_ptr);

/* ------------------------------------------------------------------------- */
void wlmdk_dock_add_tile(wlmdk_dock_t *dock_ptr,
                         wlmtk_tile_t *tile_ptr)
{
    // FIXME: Define growth orientation.
    BS_ASSERT(NULL == wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_add_element_back(
        &dock_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
    bs_dllist_push_back(
        &dock_ptr->tiles,
        wlmtk_dlnode_from_tile(tile_ptr));

    // FIXME: size update?
}

/* ------------------------------------------------------------------------- */
void wlmdk_dock_remove_tile(wlmdk_dock_t *dock_ptr,
                            wlmtk_tile_t *tile_ptr)
{
    BS_ASSERT(
        &dock_ptr->tile_box.element_container ==
        wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_remove_element(
        &dock_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
    bs_dllist_remove(
        &dock_ptr->tiles,
        wlmtk_dlnode_from_tile(tile_ptr));

    // FIXME: size update?
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == End of dock.c ================= */
