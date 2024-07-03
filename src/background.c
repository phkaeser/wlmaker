/* ========================================================================= */
/**
 * @file background.c
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

#include "background.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** Background state. */
struct _wlmaker_background_t {
    /** The background is a panel. */
    wlmtk_panel_t             super_panel;

    /** Initial implementation: The background is a uni-color rectangle. */
    wlmtk_rectangle_t         *rectangle_ptr;
};

static void _wlmaker_background_element_destroy(
    wlmtk_element_t *element_ptr);
static uint32_t _wlmaker_background_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

/* == Data ================================================================= */

/** The background panel's element superclass virtual method. */
static const wlmtk_element_vmt_t _wlmaker_background_element_vmt = {
    .destroy = _wlmaker_background_element_destroy,
};

/** The background panels' virtual method table. */
static const wlmtk_panel_vmt_t _wlmaker_background_panel_vmt = {
    .request_size = _wlmaker_background_request_size
};

/** Panel's position: Anchored to all 4 edges, and auto-sized. */
static const wlmtk_panel_positioning_t _wlmaker_background_panel_position = {
    .desired_width = 0,
    .desired_height = 0,
    .anchor = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_background_t *wlmaker_background_create(
    uint32_t color,
    wlmtk_env_t *env_ptr)
{
    wlmaker_background_t *background_ptr = logged_calloc(
        1, sizeof(wlmaker_background_t));
    if (NULL == background_ptr) return NULL;

    if (!wlmtk_panel_init(&background_ptr->super_panel,
                          &_wlmaker_background_panel_position,
                          env_ptr)) {
        wlmaker_background_destroy(background_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &background_ptr->super_panel.super_container.super_element,
        &_wlmaker_background_element_vmt);
    wlmtk_panel_extend(&background_ptr->super_panel,
                       &_wlmaker_background_panel_vmt);

    background_ptr->rectangle_ptr = wlmtk_rectangle_create(
        env_ptr, 0, 0, color);
    if (NULL == background_ptr->rectangle_ptr) {
        wlmaker_background_destroy(background_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(background_ptr->rectangle_ptr),
        true);
    wlmtk_container_add_element(
        &background_ptr->super_panel.super_container,
        wlmtk_rectangle_element(background_ptr->rectangle_ptr));

    wlmtk_element_set_visible(
        wlmtk_panel_element(&background_ptr->super_panel),
        true);
    return background_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_background_destroy(wlmaker_background_t *background_ptr)
{
    if (NULL != background_ptr->rectangle_ptr) {
        wlmtk_container_remove_element(
            &background_ptr->super_panel.super_container,
            wlmtk_rectangle_element(background_ptr->rectangle_ptr));

        wlmtk_rectangle_destroy(background_ptr->rectangle_ptr);
        background_ptr->rectangle_ptr = NULL;
    }

    wlmtk_panel_fini(&background_ptr->super_panel);
    free(background_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_panel_t *wlmaker_background_panel(wlmaker_background_t *background_ptr)
{
    return &background_ptr->super_panel;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::destroy. Dtor for the background. */
void _wlmaker_background_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmaker_background_t *background_ptr = BS_CONTAINER_OF(
        element_ptr,
        wlmaker_background_t,
        super_panel.super_container.super_element);
    wlmaker_background_destroy(background_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_panel_vmt_t::request_size. Updates the panel size. */
uint32_t _wlmaker_background_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height)
{
    wlmaker_background_t *background_ptr = BS_CONTAINER_OF(
        panel_ptr, wlmaker_background_t, super_panel);

    wlmtk_rectangle_set_size(background_ptr->rectangle_ptr, width, height);

    wlmtk_panel_commit(
        &background_ptr->super_panel, 0, &_wlmaker_background_panel_position);
    return 0;
}


/* == End of background.c ================================================== */
