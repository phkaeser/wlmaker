/* ========================================================================= */
/**
 * @file window_decorations.c
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

#include "window_decorations.h"

#include "../config.h"
#include "margin.h"
#include "resizebar.h"
#include "titlebar.h"

/* == Declarations ========================================================= */

/** State of the decoration of a window. */
struct _wlmaker_window_decorations_t {
    /** Back-link to the view. */
    wlmaker_view_t            *view_ptr;

    /** Scene tree holding all decoration elements. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Window margins. */
    wlmaker_decorations_margin_t *margin_ptr;

    /** The titlebar, including buttons. */
    wlmaker_decorations_titlebar_t *titlebar_ptr;

    /** The resizebar, including all resize elements and margin. */
    wlmaker_decorations_resizebar_t *resizebar_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_window_decorations_t *wlmaker_window_decorations_create(
    wlmaker_view_t *view_ptr)
{
    wlmaker_window_decorations_t *decorations_ptr = logged_calloc(
        1, sizeof(wlmaker_window_decorations_t));
    if (NULL == decorations_ptr) return NULL;
    decorations_ptr->view_ptr = view_ptr;

    // Must be mapped. TODO(kaeser@gubbe.ch): Don't rely on internals!
    BS_ASSERT(NULL != view_ptr->workspace_ptr);
    BS_ASSERT(view_ptr->server_side_decoration_enabled);
    BS_ASSERT(!view_ptr->fullscreen);
    // TODO(kaeser@gubbe.ch): Shouldn't need to access the internals.
    uint32_t width, height;
    view_ptr->impl_ptr->get_size(view_ptr, &width, &height);

    decorations_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        view_ptr->elements_wlr_scene_tree_ptr);
    if (NULL == decorations_ptr->wlr_scene_tree_ptr) {
        wlmaker_window_decorations_destroy(decorations_ptr);
        return NULL;
    }

    // Margins around the window itself (not including title or resize bar).
    decorations_ptr->margin_ptr = wlmaker_decorations_margin_create(
        decorations_ptr->wlr_scene_tree_ptr, 0, 0, width, height,
        WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);
    if (NULL == decorations_ptr->margin_ptr) {
        wlmaker_window_decorations_destroy(decorations_ptr);
        return NULL;
    }

    decorations_ptr->titlebar_ptr = wlmaker_decorations_titlebar_create(
        decorations_ptr->wlr_scene_tree_ptr, width, view_ptr);
    if (NULL == decorations_ptr->titlebar_ptr) {
        wlmaker_window_decorations_destroy(decorations_ptr);
        return NULL;
    }

    decorations_ptr->resizebar_ptr = wlmaker_decorations_resizebar_create(
        decorations_ptr->wlr_scene_tree_ptr, width, height, view_ptr);
    if (NULL == decorations_ptr->resizebar_ptr) {
        wlmaker_window_decorations_destroy(decorations_ptr);
        return NULL;
    }


    return decorations_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_destroy(
    wlmaker_window_decorations_t *decorations_ptr)
{
    if (NULL != decorations_ptr->resizebar_ptr) {
        wlmaker_decorations_resizebar_destroy(decorations_ptr->resizebar_ptr);
        decorations_ptr->resizebar_ptr = NULL;
    }

    if (NULL != decorations_ptr->titlebar_ptr) {
        wlmaker_decorations_titlebar_destroy(decorations_ptr->titlebar_ptr);
        decorations_ptr->titlebar_ptr = NULL;
    }

    if (NULL != decorations_ptr->margin_ptr) {
        wlmaker_decorations_margin_destroy(decorations_ptr->margin_ptr);
        decorations_ptr->margin_ptr = NULL;
    }

    if (NULL != decorations_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&decorations_ptr->wlr_scene_tree_ptr->node);
        decorations_ptr->wlr_scene_tree_ptr = NULL;
    }

    free(decorations_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_set_inner_size(
    wlmaker_window_decorations_t *decorations_ptr,
    uint32_t width,
    uint32_t height)
{
    if (NULL != decorations_ptr->margin_ptr) {
        wlmaker_decorations_margin_set_size(
            decorations_ptr->margin_ptr, width, height);
    }

    if (NULL != decorations_ptr->titlebar_ptr) {
        wlmaker_decorations_titlebar_set_width(
            decorations_ptr->titlebar_ptr, width);
    }

    if (NULL != decorations_ptr->resizebar_ptr) {
        wlmaker_decorations_resizebar_set_size(
            decorations_ptr->resizebar_ptr, width, height);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_get_added_size(
    wlmaker_window_decorations_t *decorations_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr)
{
    if (NULL != width_ptr) {
        *width_ptr = 2 * wlmaker_config_theme.window_margin_width;
    }

    if (NULL != height_ptr) {
        *height_ptr = 2 * wlmaker_config_theme.window_margin_width;
        if (NULL != decorations_ptr->titlebar_ptr) {
            *height_ptr += wlmaker_decorations_titlebar_get_height(
                decorations_ptr->titlebar_ptr);
        }

        if (NULL != decorations_ptr->resizebar_ptr) {
            *height_ptr += wlmaker_decorations_resizebar_get_height(
                decorations_ptr->resizebar_ptr);
        }
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_relative_position(
    wlmaker_window_decorations_t *decorations_ptr,
    int *relx_ptr,
    int *rely_ptr)
{
    if (NULL != relx_ptr) {
        *relx_ptr = -wlmaker_config_theme.window_margin_width;
    }

    if (NULL != rely_ptr) {
        *rely_ptr = -wlmaker_config_theme.window_margin_width;
        if (NULL != decorations_ptr->titlebar_ptr) {
            *rely_ptr -= wlmaker_decorations_titlebar_get_height(
                decorations_ptr->titlebar_ptr);
        }
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_update_title(
    wlmaker_window_decorations_t *decorations_ptr)
{
    if (NULL != decorations_ptr->titlebar_ptr) {
        wlmaker_decorations_titlebar_update_title(
            decorations_ptr->titlebar_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_window_decorations_set_shade(
    wlmaker_window_decorations_t *decorations_ptr,
    bool shaded)
{
    if (shaded && NULL != decorations_ptr->resizebar_ptr) {
        wlmaker_decorations_resizebar_destroy(decorations_ptr->resizebar_ptr);
        decorations_ptr->resizebar_ptr = NULL;
    }

    if (!shaded && NULL == decorations_ptr->resizebar_ptr) {
        uint32_t width, height;
        decorations_ptr->view_ptr->impl_ptr->get_size(
            decorations_ptr->view_ptr, &width, &height);
        decorations_ptr->resizebar_ptr = wlmaker_decorations_resizebar_create(
            decorations_ptr->wlr_scene_tree_ptr,
            width, height,
            decorations_ptr->view_ptr);
    }

    wlmaker_decorations_margin_set_edges(
        decorations_ptr->margin_ptr,
        shaded ?
        WLR_EDGE_TOP :
        WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);
}

/* == End of window_decorations.c ========================================== */
