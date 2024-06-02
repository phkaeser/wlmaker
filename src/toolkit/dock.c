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

    /** A dock is primarily a box, as element of the panel. */
    wlmtk_box_t               entry_box;
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
            &dock_ptr->entry_box,
            env_ptr,
            box_orientation,
            &dock_ptr->box_style)) {
        wlmtk_dock_destroy(dock_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(wlmtk_box_element(&dock_ptr->entry_box), true);
    wlmtk_container_add_element(
        &dock_ptr->super_panel.super_container,
        wlmtk_box_element(&dock_ptr->entry_box));

    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_dock_destroy(wlmtk_dock_t *dock_ptr)
{
    if (wlmtk_box_element(&dock_ptr->entry_box)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &dock_ptr->super_panel.super_container,
            wlmtk_box_element(&dock_ptr->entry_box));
        wlmtk_box_fini(&dock_ptr->entry_box);
    }

    wlmtk_panel_fini(&dock_ptr->super_panel);
    free(dock_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_panel_t *wlmtk_dock_panel(wlmtk_dock_t *dock_ptr)
{
    return &dock_ptr->super_panel;
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

/* == End of dock.c ======================================================== */
