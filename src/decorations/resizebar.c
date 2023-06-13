/* ========================================================================= */
/**
 * @file resizebar.c
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

#include "resizebar.h"

#include "element.h"
#include "../config.h"
#include "../resizebar.h"

/* == Declarations ========================================================= */

/** State of a window's resize bar. */
struct _wlmaker_decorations_resizebar_t {
    /** Back-link to the view it decorates. */
    wlmaker_view_t            *view_ptr;

    /** Scene tree, for just the resize bar elements and margin. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Left element of the resize bar, or NULL if not set. */
    wlmaker_decorations_resize_t *left_resize_ptr;
    /** Center element of the resize bar, or NULL if not set. */
    wlmaker_decorations_resize_t *center_resize_ptr;
    /** Right element of the resize bar. */
    wlmaker_decorations_resize_t *right_resize_ptr;

    /** Width of the left element, or 0 if not set. */
    unsigned                  left_width;
    /** Width of the center element, or 0 if not set. */
    unsigned                  center_width;
    /** Width of the right element. */
    unsigned                  right_width;

    /** Overall width of the decorated window. */
    unsigned                  width;
    /** Height of the decorated window. */
    unsigned                  height;
};

/** Hardcoded: Width of bezel. */
static const uint32_t         bezel_width = 1;
/** Hardcoded: Height of the resize bar. */
static const uint32_t         resizebar_height = 7;
/** Hardcoded: Width of the corner elements of the resize bar. */
static const uint32_t         resizebar_corner_width = 29;

static void set_width(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    unsigned width);
static bs_gfxbuf_t *create_background(unsigned width);
static void create_or_update_resize(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    wlmaker_decorations_resize_t **resize_ptr_ptr,
    bs_gfxbuf_t *background_gfxbuf_ptr,
    int pos, unsigned width,
    uint32_t edges);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_decorations_resizebar_t *wlmaker_decorations_resizebar_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    unsigned width,
    unsigned height,
    wlmaker_view_t *view_ptr)
{
    wlmaker_decorations_resizebar_t *resizebar_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_resizebar_t));
    if (NULL == resizebar_ptr) return NULL;
    resizebar_ptr->view_ptr = view_ptr;

    resizebar_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr);
    if (NULL == resizebar_ptr->wlr_scene_tree_ptr) {
        wlmaker_decorations_resizebar_destroy(resizebar_ptr);
        return NULL;
    }
    wlr_scene_node_set_position(
        &resizebar_ptr->wlr_scene_tree_ptr->node,
        0, height + wlmaker_config_theme.window_margin_width);

    wlmaker_decorations_resizebar_set_size(resizebar_ptr, width, height);
    return resizebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_resizebar_destroy(
    wlmaker_decorations_resizebar_t *resizebar_ptr)
{
    if (NULL != resizebar_ptr->right_resize_ptr) {
        wlmaker_decorations_resize_destroy(resizebar_ptr->right_resize_ptr);
        resizebar_ptr->right_resize_ptr = NULL;
    }
    if (NULL != resizebar_ptr->center_resize_ptr) {
        wlmaker_decorations_resize_destroy(resizebar_ptr->center_resize_ptr);
        resizebar_ptr->center_resize_ptr = NULL;
    }
    if (NULL != resizebar_ptr->left_resize_ptr) {
        wlmaker_decorations_resize_destroy(resizebar_ptr->left_resize_ptr);
        resizebar_ptr->left_resize_ptr = NULL;
    }

    if (NULL != resizebar_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&resizebar_ptr->wlr_scene_tree_ptr->node);
        resizebar_ptr->wlr_scene_tree_ptr = NULL;
    }

    free(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_resizebar_set_size(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    unsigned width,
    unsigned height)
{
    if (width == resizebar_ptr->width &&
        height == resizebar_ptr->height) return;
    set_width(resizebar_ptr, width);

    wlr_scene_node_set_position(
        &resizebar_ptr->wlr_scene_tree_ptr->node,
        0, height + wlmaker_config_theme.window_margin_width);

    unsigned bar_y = 0;

    if (0 < resizebar_ptr->left_width) {
        wlmaker_decorations_element_set_position(
            wlmaker_decorations_element_from_resize(
                resizebar_ptr->left_resize_ptr),
            0, bar_y);
    }

    if (0 < resizebar_ptr->center_width) {
        wlmaker_decorations_element_set_position(
            wlmaker_decorations_element_from_resize(
                resizebar_ptr->center_resize_ptr),
            resizebar_ptr->left_width, bar_y);
    }

    wlmaker_decorations_element_set_position(
        wlmaker_decorations_element_from_resize(
            resizebar_ptr->right_resize_ptr),
        width - resizebar_ptr->right_width, bar_y);

    resizebar_ptr->height = height;
    return;
}

/* ------------------------------------------------------------------------- */
unsigned wlmaker_decorations_resizebar_get_height(
    __UNUSED__ wlmaker_decorations_resizebar_t *resizebar_ptr)
{
    return resizebar_height + wlmaker_config_theme.window_margin_width;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Applies the width to the resizebar, re-creating elements if needed. */
static void set_width(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    unsigned width)
{
    if (width == resizebar_ptr->width) return;

    resizebar_ptr->left_width = resizebar_corner_width;
    resizebar_ptr->right_width = resizebar_corner_width;
    if (width > 2 * resizebar_corner_width) {
        resizebar_ptr->center_width = width - 2 * resizebar_corner_width;
    } else if (width > resizebar_corner_width) {
        resizebar_ptr->center_width = 0;
        resizebar_ptr->left_width = width - resizebar_corner_width;
    } else {
        resizebar_ptr->left_width = 0;
        resizebar_ptr->right_width = width;
    }

    BS_ASSERT(resizebar_ptr->left_width +
              resizebar_ptr->right_width +
              resizebar_ptr->center_width == width);

    bs_gfxbuf_t *gfxbuf_ptr = create_background(width);
    BS_ASSERT(NULL != gfxbuf_ptr);

    if (0 < resizebar_ptr->left_width) {
        create_or_update_resize(
            resizebar_ptr,
            &resizebar_ptr->left_resize_ptr,
            gfxbuf_ptr,
            0, resizebar_ptr->left_width,
            WLR_EDGE_LEFT | WLR_EDGE_BOTTOM);
    } else if (NULL != resizebar_ptr->left_resize_ptr) {
        wlmaker_decorations_resize_destroy(resizebar_ptr->left_resize_ptr);
        resizebar_ptr->left_resize_ptr = NULL;
    }

    if (0 < resizebar_ptr->center_width) {
        create_or_update_resize(
            resizebar_ptr,
            &resizebar_ptr->center_resize_ptr,
            gfxbuf_ptr,
            resizebar_ptr->left_width, resizebar_ptr->center_width,
            WLR_EDGE_BOTTOM);
    } else if (NULL != resizebar_ptr->center_resize_ptr) {
        wlmaker_decorations_resize_destroy(resizebar_ptr->center_resize_ptr);
        resizebar_ptr->center_resize_ptr = NULL;
    }

    create_or_update_resize(
        resizebar_ptr,
        &resizebar_ptr->right_resize_ptr,
        gfxbuf_ptr,
        width - resizebar_ptr->right_width, resizebar_ptr->right_width,
        WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM);

    bs_gfxbuf_destroy(gfxbuf_ptr);
    resizebar_ptr->width = width;
}

/* ------------------------------------------------------------------------- */
/** Creates the background texture at givenm width. */
bs_gfxbuf_t *create_background(unsigned width)
{
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(width, resizebar_height);
    if (NULL == gfxbuf_ptr) return NULL;

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(
        cairo_ptr, &wlmaker_config_theme.resizebar_fill);
    cairo_destroy(cairo_ptr);

    return gfxbuf_ptr;
}

/* ------------------------------------------------------------------------- */
/** Creates or updates a resizebar element. */
void create_or_update_resize(
    wlmaker_decorations_resizebar_t *resizebar_ptr,
    wlmaker_decorations_resize_t **resize_ptr_ptr,
    bs_gfxbuf_t *background_gfxbuf_ptr,
    int pos, unsigned width,
    uint32_t edges)
{
    cairo_t *cairo_ptr;

    struct wlr_buffer *released_wlrbuf_ptr = bs_gfxbuf_create_wlr_buffer(
        width, resizebar_height);
    BS_ASSERT(NULL != released_wlrbuf_ptr);
    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(released_wlrbuf_ptr), 0, 0,
        background_gfxbuf_ptr, pos, 0,
        width, resizebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(released_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(cairo_ptr, bezel_width, true);
    cairo_destroy(cairo_ptr);

    struct wlr_buffer *pressed_wlrbuf_ptr = bs_gfxbuf_create_wlr_buffer(
        width, resizebar_height);
    BS_ASSERT(NULL != released_wlrbuf_ptr);
    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(pressed_wlrbuf_ptr), 0, 0,
        background_gfxbuf_ptr, pos, 0,
        width, resizebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(pressed_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(cairo_ptr, bezel_width, false);
    cairo_destroy(cairo_ptr);

    if (NULL == *resize_ptr_ptr) {
        *resize_ptr_ptr = wlmaker_decorations_resize_create(
            resizebar_ptr->wlr_scene_tree_ptr,
            resizebar_ptr->view_ptr->server_ptr->cursor_ptr,
            resizebar_ptr->view_ptr,
            released_wlrbuf_ptr,
            pressed_wlrbuf_ptr,
            edges);
        BS_ASSERT(NULL != *resize_ptr_ptr);
    } else {
        wlmaker_decorations_resize_set_textures(
            *resize_ptr_ptr,
            released_wlrbuf_ptr,
            pressed_wlrbuf_ptr);
    }

    wlr_buffer_drop(pressed_wlrbuf_ptr);
    wlr_buffer_drop(released_wlrbuf_ptr);
}

/* == End of resizebar.c =================================================== */
