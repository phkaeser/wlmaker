/* ========================================================================= */
/**
 * @file margin.c
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

#include "margin.h"

#include "../config.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** Handle for margin. */
struct _wlmaker_decorations_margin_t {
    /** Parent's WLR scene tree. */
    struct wlr_scene_tree     *parent_wlr_scene_tree_ptr;

    /** Width of the element surrounded by the margin(s). */
    unsigned                  width;
    /** Height of the surrounded element. */
    unsigned                  height;
    /** X-coordinate of the top-left corner of the decorated area. */
    int                       x;
    /** Y-coordinate of the top-left corner of the decorated area. */
    int                       y;
    /** Which edges of the margin should be drawn. */
    uint32_t                  edges;

    /** Scene rectangle holding the left edge of the margin. If any. */
    struct wlr_scene_rect     *left_rect_ptr;
    /** Scene rectangle holding the top edge of the margin. If any. */
    struct wlr_scene_rect     *top_rect_ptr;
    /** Scene rectangle holding the right edge of the margin. If any. */
    struct wlr_scene_rect     *right_rect_ptr;
    /** Scene rectangle holding the bottom edge of the margin. If any. */
    struct wlr_scene_rect     *bottom_rect_ptr;
};

static bool recreate_edges(
    wlmaker_decorations_margin_t *margin_ptr,
    uint32_t edges);
static struct wlr_scene_rect *create_rect(
    struct wlr_scene_tree *decoration_wlr_scene_tree_ptr,
    uint32_t color);
static void rect_set_size(
    struct wlr_scene_rect *wlr_scene_rect_ptr,
    unsigned width,
    unsigned height);
static void rect_set_position(
    struct wlr_scene_rect *wlr_scene_rect_ptr,
    int x,
    int y);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_decorations_margin_t *wlmaker_decorations_margin_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    int x, int y,
    unsigned width, unsigned height,
    uint32_t edges)
{
    wlmaker_decorations_margin_t *margin_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_margin_t));
    if (NULL == margin_ptr) return NULL;
    margin_ptr->parent_wlr_scene_tree_ptr = wlr_scene_tree_ptr;

    if (!recreate_edges(margin_ptr, edges)) {
        wlmaker_decorations_margin_destroy(margin_ptr);
        return NULL;
    }
    wlmaker_decorations_margin_set_position(margin_ptr, x, y);
    wlmaker_decorations_margin_set_size(margin_ptr, width, height);
    return margin_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_margin_destroy(
    wlmaker_decorations_margin_t *margin_ptr)
{
    if (NULL != margin_ptr->bottom_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->bottom_rect_ptr->node);
        margin_ptr->bottom_rect_ptr = NULL;
    }
    if (NULL != margin_ptr->right_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->right_rect_ptr->node);
        margin_ptr->right_rect_ptr = NULL;
    }
    if (NULL != margin_ptr->top_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->top_rect_ptr->node);
        margin_ptr->top_rect_ptr = NULL;
    }
    if (NULL != margin_ptr->left_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->left_rect_ptr->node);
        margin_ptr->left_rect_ptr = NULL;
    }

    free(margin_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_margin_set_position(
    wlmaker_decorations_margin_t *margin_ptr,
    int x,
    int y)
{
    unsigned margin_width = wlmaker_config_theme.window_margin_width;

    int hx = 0;
    if (margin_ptr->edges & WLR_EDGE_LEFT) {
        hx -= margin_width;
    }

    rect_set_position(margin_ptr->left_rect_ptr,
                      x - margin_width, y);
    rect_set_position(margin_ptr->top_rect_ptr,
                      x + hx, y - margin_width);
    rect_set_position(margin_ptr->right_rect_ptr,
                      x + margin_ptr->width, y);
    rect_set_position(margin_ptr->bottom_rect_ptr,
                      x + hx, y + margin_ptr->height);

    margin_ptr->x = x;
    margin_ptr->y = y;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_margin_set_size(
    wlmaker_decorations_margin_t *margin_ptr,
    unsigned width,
    unsigned height)
{
    unsigned margin_width = wlmaker_config_theme.window_margin_width;

    // Horizontal margin area will cover the "corner" area if both margins
    // are set.
    unsigned hwidth = width;
    if (margin_ptr->edges & WLR_EDGE_LEFT) {
        hwidth += wlmaker_config_theme.window_margin_width;
    }
    if (margin_ptr->edges & WLR_EDGE_RIGHT) {
        hwidth += wlmaker_config_theme.window_margin_width;
    }
    rect_set_size(margin_ptr->left_rect_ptr, margin_width, height);
    rect_set_size(margin_ptr->top_rect_ptr, hwidth, margin_width);
    rect_set_size(margin_ptr->right_rect_ptr, margin_width, height);
    rect_set_size(margin_ptr->bottom_rect_ptr, hwidth, margin_width);

    margin_ptr->width = width;
    margin_ptr->height = height;

    // Need to update the position of the margins.
    wlmaker_decorations_margin_set_position(
        margin_ptr, margin_ptr->x, margin_ptr->y);

}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_margin_set_edges(
    wlmaker_decorations_margin_t *margin_ptr,
    uint32_t edges)
{
    BS_ASSERT(recreate_edges(margin_ptr, edges));
    wlmaker_decorations_margin_set_position(
        margin_ptr, margin_ptr->x, margin_ptr->y);
    wlmaker_decorations_margin_set_size(
        margin_ptr, margin_ptr->width, margin_ptr->height);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Re-creates the rectangles for the specified edges. */
bool recreate_edges(
    wlmaker_decorations_margin_t *margin_ptr,
    uint32_t edges)
{
    uint32_t col = wlmaker_config_theme.window_margin_color;
    struct wlr_scene_tree *wlr_scene_tree_ptr =
        margin_ptr->parent_wlr_scene_tree_ptr;

    if (edges & WLR_EDGE_LEFT) {
        margin_ptr->left_rect_ptr = create_rect(wlr_scene_tree_ptr, col);
        if (NULL == margin_ptr->left_rect_ptr) return false;
    } else if (NULL != margin_ptr->left_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->left_rect_ptr->node);
        margin_ptr->left_rect_ptr = NULL;
    }

    if (edges & WLR_EDGE_TOP) {
        margin_ptr->top_rect_ptr = create_rect(wlr_scene_tree_ptr, col);
        if (NULL == margin_ptr->top_rect_ptr) return false;
    } else if (NULL != margin_ptr->top_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->top_rect_ptr->node);
        margin_ptr->top_rect_ptr = NULL;
    }

    if (edges & WLR_EDGE_RIGHT) {
        margin_ptr->right_rect_ptr = create_rect(wlr_scene_tree_ptr, col);
        if (NULL == margin_ptr->right_rect_ptr) return false;
    } else if (NULL != margin_ptr->right_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->right_rect_ptr->node);
        margin_ptr->right_rect_ptr = NULL;
    }

    if (edges & WLR_EDGE_BOTTOM) {
        margin_ptr->bottom_rect_ptr = create_rect(wlr_scene_tree_ptr, col);
        if (NULL == margin_ptr->bottom_rect_ptr) return false;
    } else if (NULL != margin_ptr->bottom_rect_ptr) {
        wlr_scene_node_destroy(&margin_ptr->bottom_rect_ptr->node);
        margin_ptr->bottom_rect_ptr = NULL;
    }
    margin_ptr->edges = edges;
    return true;
}
/* ------------------------------------------------------------------------- */
/**
 * Helper: Creates a `wlr_scene_rect` with the given `color`.
 *
 * The rectangle will not be set to correct size nor position. Use
 * @ref rect_set_size and @ref rect_set_position for that.
 *
 * @param decoration_wlr_scene_tree_ptr
 * @param color               As an ARGB8888 32-bit value.
 */
struct wlr_scene_rect *create_rect(
    struct wlr_scene_tree *decoration_wlr_scene_tree_ptr, uint32_t color)
{
    float fcolor[4];
    bs_gfxbuf_argb8888_to_floats(
        color, &fcolor[0], &fcolor[1], &fcolor[2], &fcolor[3]);
    struct wlr_scene_rect *wlr_scene_rect_ptr = wlr_scene_rect_create(
        decoration_wlr_scene_tree_ptr, 1, 1, fcolor);
    if (NULL == wlr_scene_rect_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_rect_create(%p, 1, 1, %"PRIx32")",
               decoration_wlr_scene_tree_ptr, color);
        return NULL;
    }
    wlr_scene_node_set_enabled(&wlr_scene_rect_ptr->node, true);
    return wlr_scene_rect_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Helper: Updates dimensions of the `wlr_scene_rect`.
 *
 * @param wlr_scene_rect_ptr  The rectangle to update. May be NULL.
 * @param width
 * @param height
 */
static void rect_set_size(
    struct wlr_scene_rect *wlr_scene_rect_ptr,
    unsigned width,
    unsigned height)
{
    if (NULL == wlr_scene_rect_ptr) return;
    wlr_scene_rect_set_size(
        wlr_scene_rect_ptr, width, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Helper: Updates position of the `wlr_scene_rect`.
 *
 * @param wlr_scene_rect_ptr  The rectangle to update. May be NULL.
 * @param x                   Position relative to the decorated window.
 * @param y
 */
static void rect_set_position(
    struct wlr_scene_rect *wlr_scene_rect_ptr,
    int x,
    int y)
{
    if (NULL == wlr_scene_rect_ptr) return;
    wlr_scene_node_set_position(
        &wlr_scene_rect_ptr->node, x, y);
}

/* == End of margin.c ====================================================== */
