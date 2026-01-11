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
#include <stdbool.h>
#include <stdlib.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

struct wlr_output;

/* == Declarations ========================================================= */

/** Background state. */
struct _wlmaker_background_t {
    /** Links to layer. */
    wlmtk_layer_t             *layer_ptr;
    /** color of the background. */
    uint32_t                  color;
    /** Tracks the outputs available. */
    wlmtk_output_tracker_t    *output_tracker_ptr;
};

/** Background panel: The workspace's backgrund for the output. */
typedef struct  {
    /** A layer background for one output is a panel. */
    wlmtk_panel_t             super_panel;
    /** Initial implementation: The background is a uni-color rectangle. */
    wlmtk_rectangle_t         *rectangle_ptr;
} wlmaker_background_panel_t;

static uint32_t _wlmaker_background_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

static void *_wlmaker_background_panel_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);
static void _wlmaker_background_panel_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

/* == Data ================================================================= */

/** The background panels' virtual method table. */
static const wlmtk_panel_vmt_t _wlmaker_background_panel_vmt = {
    .request_size = _wlmaker_background_panel_request_size
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
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    uint32_t color)
{
    wlmaker_background_t *background_ptr = logged_calloc(
        1, sizeof(wlmaker_background_t));
    if (NULL == background_ptr) return NULL;
    background_ptr->layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_BACKGROUND),
    background_ptr->color = color;

    background_ptr->output_tracker_ptr = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        background_ptr,
        _wlmaker_background_panel_create,
        NULL,
        _wlmaker_background_panel_destroy);

    return background_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_background_destroy(wlmaker_background_t *background_ptr)
{
    if (NULL != background_ptr->output_tracker_ptr) {
        wlmtk_output_tracker_destroy(background_ptr->output_tracker_ptr);
        background_ptr->output_tracker_ptr = NULL;
    }
    free(background_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_panel_vmt_t::request_size. Updates the panel size. */
uint32_t _wlmaker_background_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height)
{
    wlmaker_background_panel_t *background_ptr = BS_CONTAINER_OF(
        panel_ptr, wlmaker_background_panel_t, super_panel);

    wlmtk_rectangle_set_size(background_ptr->rectangle_ptr, width, height);

    wlmtk_panel_commit(
        &background_ptr->super_panel, 0,
        &_wlmaker_background_panel_position);
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Ctor. */
void *_wlmaker_background_panel_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    wlmaker_background_t *background_ptr = ud_ptr;

    wlmaker_background_panel_t *background_panel_ptr = logged_calloc(
        1, sizeof(wlmaker_background_panel_t));
    if (NULL == background_panel_ptr) return NULL;

    if (!wlmtk_panel_init(&background_panel_ptr->super_panel,
                          &_wlmaker_background_panel_position)) {
        _wlmaker_background_panel_destroy(
            NULL, NULL, background_panel_ptr);
        return NULL;
    }
    wlmtk_panel_extend(&background_panel_ptr->super_panel,
                       &_wlmaker_background_panel_vmt);

    background_panel_ptr->rectangle_ptr = wlmtk_rectangle_create(
        0, 0, background_ptr->color);
    if (NULL == background_panel_ptr->rectangle_ptr) {
        _wlmaker_background_panel_destroy(
            NULL, NULL, background_panel_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr),
        true);
    wlmtk_container_add_element(
        &background_panel_ptr->super_panel.super_container,
        wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr));

    wlmtk_element_set_visible(
        wlmtk_panel_element(&background_panel_ptr->super_panel),
        true);

    wlmtk_layer_add_panel(
        background_ptr->layer_ptr,
        &background_panel_ptr->super_panel,
        wlr_output_ptr);

    return background_panel_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor. */
void _wlmaker_background_panel_destroy(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    __UNUSED__ void *ud_ptr,
    void *output_ptr)
{
    wlmaker_background_panel_t *background_panel_ptr = output_ptr;

    if (NULL != wlmtk_panel_get_layer(
            &background_panel_ptr->super_panel)) {
        wlmtk_layer_remove_panel(
            wlmtk_panel_get_layer(&background_panel_ptr->super_panel),
            &background_panel_ptr->super_panel);
    }

    if (NULL != background_panel_ptr->rectangle_ptr) {
        wlmtk_container_remove_element(
            &background_panel_ptr->super_panel.super_container,
            wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr));

        wlmtk_rectangle_destroy(background_panel_ptr->rectangle_ptr);
        background_panel_ptr->rectangle_ptr = NULL;
    }

    wlmtk_panel_fini(&background_panel_ptr->super_panel);

    free(background_panel_ptr);
}

/* == End of background.c ================================================== */
