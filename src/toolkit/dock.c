/* ========================================================================= */
/**
 * @file dock.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "box.h"

/* == Declarations ========================================================= */

/** State of the toolkit dock. */
struct _wlmtk_dock_t {
    /** Parent class: The panel. */
    wlmtk_panel_t             super_panel;
    /** Positioning information for the panel. */
    wlmtk_panel_positioning_t panel_positioning;

    /** Principal element of the dock is a box, holding tiles. */
    wlmtk_box_t               tile_box;
    /** Margin style of the box. */
    wlmtk_margin_style_t      box_style;
};

static uint32_t _wlmtk_dock_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

/* == Data ================================================================= */

/** Virtual method table of the panel. */
static const wlmtk_panel_vmt_t _wlmtk_dock_panel_vmt = {
    .request_size = _wlmtk_dock_panel_request_size
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_dock_t *wlmtk_dock_create(
    const wlmtk_dock_positioning_t *dock_positioning_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_dock_t *dock_ptr = logged_calloc(1, sizeof(wlmtk_dock_t));
    if (NULL == dock_ptr) return NULL;

    dock_ptr->panel_positioning.anchor = dock_positioning_ptr->anchor;
    dock_ptr->panel_positioning.desired_width = dock_positioning_ptr->tile_size;
    dock_ptr->panel_positioning.desired_height = dock_positioning_ptr->tile_size;

    if (!wlmtk_panel_init(
            &dock_ptr->super_panel,
            &dock_ptr->panel_positioning,
            env_ptr)) {
        bs_log(BS_ERROR, "Failed wlmtk_panel_init.");
        wlmtk_dock_destroy(dock_ptr);
        return NULL;
    }
    wlmtk_panel_extend(&dock_ptr->super_panel, &_wlmtk_dock_panel_vmt);

    wlmtk_box_orientation_t box_orientation = WLMTK_BOX_HORIZONTAL;
    if (WLMTK_DOCK_VERTICAL == dock_positioning_ptr->orientation) {
        box_orientation = WLMTK_BOX_VERTICAL;
    }
    if (!wlmtk_box_init(
            &dock_ptr->tile_box,
            env_ptr,
            box_orientation,
            &dock_ptr->box_style)) {
        wlmtk_dock_destroy(dock_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(wlmtk_box_element(&dock_ptr->tile_box), true);
    wlmtk_container_add_element(
        &dock_ptr->super_panel.super_container,
        wlmtk_box_element(&dock_ptr->tile_box));

    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_dock_destroy(wlmtk_dock_t *dock_ptr)
{
    if (wlmtk_box_element(&dock_ptr->tile_box)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &dock_ptr->super_panel.super_container,
            wlmtk_box_element(&dock_ptr->tile_box));
        wlmtk_box_fini(&dock_ptr->tile_box);
    }

    wlmtk_panel_fini(&dock_ptr->super_panel);
    free(dock_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_dock_add_tile(
    wlmtk_dock_t *dock_ptr,
    wlmtk_tile_t *tile_ptr)
{
    BS_ASSERT(NULL == wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_add_element_back(
        &dock_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_dock_remove_tile(
    wlmtk_dock_t *dock_ptr,
    wlmtk_tile_t *tile_ptr)
{
    BS_ASSERT(
        &dock_ptr->tile_box.super_container ==
        wlmtk_tile_element(tile_ptr)->parent_container_ptr);
    wlmtk_box_remove_element(
        &dock_ptr->tile_box,
        wlmtk_tile_element(tile_ptr));
}

/* ------------------------------------------------------------------------- */
wlmtk_panel_t *wlmtk_dock_panel(wlmtk_dock_t *dock_ptr)
{
    return &dock_ptr->super_panel;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_dock_element(wlmtk_dock_t *dock_ptr)
{
    return wlmtk_panel_element(wlmtk_dock_panel(dock_ptr));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the panel to change to the specified size.
 *
 * @param panel_ptr
 * @param width
 * @param height
 *
 * @return 0
 */
uint32_t _wlmtk_dock_panel_request_size(
    __UNUSED__ wlmtk_panel_t *panel_ptr,
    __UNUSED__ int width,
    __UNUSED__ int height)
{
    return 0;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_dock_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
 void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_dock_positioning_t pos = {};
    wlmtk_dock_t *dock_ptr = wlmtk_dock_create(&pos, NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dock_ptr);
    wlmtk_dock_destroy(dock_ptr);

}

/* == End of dock.c ======================================================== */
