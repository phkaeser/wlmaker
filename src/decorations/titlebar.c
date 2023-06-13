/* ========================================================================= */
/**
 * @file titlebar.c
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

#include "titlebar.h"

#include <libbase/libbase.h>

#include "element.h"
#include "../config.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of a window's titlebar, including buttons and title area. */
struct _wlmaker_decorations_titlebar_t {
    /** Back-link to the view it decorates. */
    wlmaker_view_t            *view_ptr;

    /** Scene tree, for just the title bar elements and margin. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** "Minimize" button element. */
    wlmaker_decorations_button_t *minimize_button_ptr;
    /** "Close" button element. */
    wlmaker_decorations_button_t *close_button_ptr;
    /** "Title" element. */
    wlmaker_decorations_title_t *title_ptr;

    /** Background graphics buffer, focussed window. */
    bs_gfxbuf_t               *background_focussed_gfxbuf_ptr;
    /** Background graphics buffer, blurred window. */
    bs_gfxbuf_t               *background_blurred_gfxbuf_ptr;

    /** Currently configured width, excluding the outer margins. */
    unsigned                  width;
    /** Position of the title element, relative to the scene tree. */
    int                       title_pos;
    /** Width of the title element. */
    unsigned                  title_width;
    /** Position of the "close" button, relative to the scene tree. */
    int                       close_pos;
};

/** Holder for a few `struct wlr_buffer` textures, for buttons & title. */
typedef struct {
    /** Texture in released state. */
    struct wlr_buffer         *released_wlrbuf_ptr;
    /** Texture in pressed state, or NULL. */
    struct wlr_buffer         *pressed_wlrbuf_ptr;
    /** Texture in blurred state. */
    struct wlr_buffer         *blurred_wlrbuf_ptr;
} wlr_buffer_holder_t;

static bool recreate_backgrounds(
    wlmaker_decorations_titlebar_t *titlebar_ptr,
    unsigned width);

static bool create_wlr_buffers(
    wlr_buffer_holder_t *buffer_holder_ptr,
    unsigned width,
    bool press);
static void drop_wlr_buffers(wlr_buffer_holder_t *buffer_holder_ptr);

static void create_or_update_minimize_button(
    wlmaker_decorations_titlebar_t *titlebar_ptr);
static void create_or_update_close_button(
    wlmaker_decorations_titlebar_t *titlebar_ptr);
static void create_or_update_title(
    wlmaker_decorations_titlebar_t *titlebar_ptr);

static void button_minimize_callback(
    wlmaker_interactive_t *interactive_ptr,
    void *data_ptr);
static void button_close_callback(
    wlmaker_interactive_t *interactive_ptr,
    void *data_ptr);

/** Hardcoded: Width of the window buttons. */
static const unsigned         wlmaker_decorations_button_width = 22;
/** Hardcoded: Height of the title bar, in pixels. */
static const unsigned         wlmaker_decorations_titlebar_height = 22;

/** Hardcoded: Width of the bezel for buttons. */
static const unsigned         wlmaker_decorations_button_bezel_width = 1;

/**
 * Attempted minimal width of the title. If the title width falls below that
 * value, buttons will be dropped instead.
 */
static const unsigned         title_min_width = wlmaker_decorations_button_width;

/* == Exported methods ===================================================== */


/* ------------------------------------------------------------------------- */
wlmaker_decorations_titlebar_t *wlmaker_decorations_titlebar_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    unsigned width,
    wlmaker_view_t *view_ptr)
{
    wlmaker_decorations_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;
    titlebar_ptr->view_ptr = view_ptr;

    titlebar_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr);
    if (NULL == titlebar_ptr->wlr_scene_tree_ptr) {
        wlmaker_decorations_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlr_scene_node_set_position(
        &titlebar_ptr->wlr_scene_tree_ptr->node,
        0, - wlmaker_decorations_titlebar_height -
        wlmaker_config_theme.window_margin_width);

    wlmaker_decorations_titlebar_set_width(titlebar_ptr, width);
    return titlebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_titlebar_destroy(
    wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    if (NULL != titlebar_ptr->title_ptr) {
        wlmaker_decorations_title_destroy(titlebar_ptr->title_ptr);
        titlebar_ptr->title_ptr = NULL;
    }

    if (NULL != titlebar_ptr->close_button_ptr) {
        wlmaker_decorations_button_destroy(titlebar_ptr->close_button_ptr);
        titlebar_ptr->close_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->minimize_button_ptr) {
        wlmaker_decorations_button_destroy(titlebar_ptr->minimize_button_ptr);
        titlebar_ptr->minimize_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->background_focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->background_focussed_gfxbuf_ptr);
        titlebar_ptr->background_focussed_gfxbuf_ptr = NULL;
    }
    if (NULL != titlebar_ptr->background_blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->background_blurred_gfxbuf_ptr);
        titlebar_ptr->background_blurred_gfxbuf_ptr = NULL;
    }

    if (NULL != titlebar_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&titlebar_ptr->wlr_scene_tree_ptr->node);
        titlebar_ptr->wlr_scene_tree_ptr = NULL;
    }

    free(titlebar_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_titlebar_set_width(
    wlmaker_decorations_titlebar_t *titlebar_ptr,
    unsigned width)
{
    bool has_close, has_minimize;

    if (width == titlebar_ptr->width) return;

    // The 'minimize' button is shown only if there's space for everything.
    if (width > title_min_width +
        2 * wlmaker_decorations_button_width +
        2 * wlmaker_config_theme.window_margin_width) {
        titlebar_ptr->title_pos =
            wlmaker_decorations_button_width +
            wlmaker_config_theme.window_margin_width;
        has_minimize = true;
    } else {
        has_minimize = false;
        titlebar_ptr->title_pos = 0;
    }

    // The 'close' button is shown as long as there's space for title and
    // one button, at least.
    if (width > title_min_width +
        wlmaker_decorations_button_width +
        wlmaker_config_theme.window_margin_width) {
        titlebar_ptr->close_pos =
            width - wlmaker_decorations_button_width;
        has_close = true;
    } else {
        // Won't be shown, but simplifies computation...
        titlebar_ptr->close_pos =
            width + wlmaker_config_theme.window_margin_width;
        has_close = false;
    }

    BS_ASSERT(
        titlebar_ptr->close_pos >=
        (int)wlmaker_config_theme.window_margin_width + titlebar_ptr->title_pos);
    titlebar_ptr->title_width =
        titlebar_ptr->close_pos - wlmaker_config_theme.window_margin_width -
        titlebar_ptr->title_pos;
    titlebar_ptr->width = width;

    BS_ASSERT(recreate_backgrounds(titlebar_ptr, width));

    if (has_minimize) {
        create_or_update_minimize_button(titlebar_ptr);
        wlmaker_decorations_element_set_position(
            wlmaker_decorations_element_from_button(
                titlebar_ptr->minimize_button_ptr),
            0, 0);
    } else if (NULL != titlebar_ptr->minimize_button_ptr) {
        wlmaker_decorations_button_destroy(titlebar_ptr->minimize_button_ptr);
        titlebar_ptr->minimize_button_ptr = NULL;
    }

    create_or_update_title(titlebar_ptr);
    wlmaker_decorations_element_set_position(
        wlmaker_decorations_element_from_title(titlebar_ptr->title_ptr),
        titlebar_ptr->title_pos, 0);

    if (has_close) {
        create_or_update_close_button(titlebar_ptr);
        wlmaker_decorations_element_set_position(
            wlmaker_decorations_element_from_button(
                titlebar_ptr->close_button_ptr),
            titlebar_ptr->close_pos, 0);
    } else if (NULL != titlebar_ptr->close_button_ptr) {
        wlmaker_decorations_button_destroy(titlebar_ptr->close_button_ptr);
        titlebar_ptr->close_button_ptr = NULL;
    }

    titlebar_ptr->width = width;
}

/* ------------------------------------------------------------------------- */
unsigned wlmaker_decorations_titlebar_get_height(
    __UNUSED__ wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    return wlmaker_decorations_titlebar_height +
        wlmaker_config_theme.window_margin_width;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_titlebar_update_title(
    wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    create_or_update_title(titlebar_ptr);
}

/* == Local (static) methods =============================================== */

/**
 * (Re)creates the backgrounds for the title bar.
 *
 * @param titlebar_ptr
 * @param width
 *
 * @return true on success.
 */
static bool recreate_backgrounds(
    wlmaker_decorations_titlebar_t *titlebar_ptr,
    unsigned width)
{
    bs_gfxbuf_t *focussed_ptr, *blurred_ptr;
    cairo_t *cairo_ptr;

    focussed_ptr = bs_gfxbuf_create(
        width, wlmaker_decorations_titlebar_height);
    if (NULL == focussed_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(focussed_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(focussed_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(
        cairo_ptr,
        &wlmaker_config_theme.titlebar_focussed_fill);
    cairo_destroy(cairo_ptr);

    blurred_ptr = bs_gfxbuf_create(
        width, wlmaker_decorations_titlebar_height);
    if (NULL == blurred_ptr) {
        bs_gfxbuf_destroy(focussed_ptr);
        return false;
    }
    cairo_ptr = cairo_create_from_bs_gfxbuf(blurred_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(focussed_ptr);
        bs_gfxbuf_destroy(blurred_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(
        cairo_ptr,
        &wlmaker_config_theme.titlebar_blurred_fill);
    cairo_destroy(cairo_ptr);

    if (NULL != titlebar_ptr->background_focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->background_focussed_gfxbuf_ptr);
    }
    titlebar_ptr->background_focussed_gfxbuf_ptr = focussed_ptr;
    if (NULL != titlebar_ptr->background_blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->background_blurred_gfxbuf_ptr);
    }
    titlebar_ptr->background_blurred_gfxbuf_ptr = blurred_ptr;

    return true;
}

/* ------------------------------------------------------------------------- */
/** Creates WLR buffers of `buffer_holder_ptr`. */
bool create_wlr_buffers(
    wlr_buffer_holder_t *buffer_holder_ptr,
    unsigned width,
    bool press)
{
    memset(buffer_holder_ptr, 0, sizeof(wlr_buffer_holder_t));

    buffer_holder_ptr->released_wlrbuf_ptr = bs_gfxbuf_create_wlr_buffer(
        width, wlmaker_decorations_titlebar_height);
    if (NULL == buffer_holder_ptr->released_wlrbuf_ptr) {
        drop_wlr_buffers(buffer_holder_ptr);
        return false;
    }

    if (press) {
        buffer_holder_ptr->pressed_wlrbuf_ptr = bs_gfxbuf_create_wlr_buffer(
            width, wlmaker_decorations_titlebar_height);
        if (NULL == buffer_holder_ptr->pressed_wlrbuf_ptr) {
            drop_wlr_buffers(buffer_holder_ptr);
            return false;
        }
    }

    buffer_holder_ptr->blurred_wlrbuf_ptr = bs_gfxbuf_create_wlr_buffer(
        width, wlmaker_decorations_titlebar_height);
    if (NULL == buffer_holder_ptr->blurred_wlrbuf_ptr) {
        drop_wlr_buffers(buffer_holder_ptr);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Drops the WLR buffers of `buffer_holder_ptr`. */
void drop_wlr_buffers(wlr_buffer_holder_t *buffer_holder_ptr)
{
    if (NULL != buffer_holder_ptr->blurred_wlrbuf_ptr) {
        wlr_buffer_drop(buffer_holder_ptr->blurred_wlrbuf_ptr);
        buffer_holder_ptr->blurred_wlrbuf_ptr = NULL;
    }

    if (NULL != buffer_holder_ptr->pressed_wlrbuf_ptr) {
        wlr_buffer_drop(buffer_holder_ptr->pressed_wlrbuf_ptr);
        buffer_holder_ptr->pressed_wlrbuf_ptr = NULL;
    }

    if (NULL != buffer_holder_ptr->released_wlrbuf_ptr) {
        wlr_buffer_drop(buffer_holder_ptr->released_wlrbuf_ptr);
        buffer_holder_ptr->released_wlrbuf_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates (or updates) the "Minimize" button and textures.
 *
 * @param titlebar_ptr
 */
void create_or_update_minimize_button(
    wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    cairo_t *cairo_ptr;
    wlr_buffer_holder_t buf_holder;

    BS_ASSERT(create_wlr_buffers(
                  &buf_holder, wlmaker_decorations_button_width, true));

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.released_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_focussed_gfxbuf_ptr,
        0, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.released_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_minimize_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_focussed_text_color);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.pressed_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_focussed_gfxbuf_ptr,
        0, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.pressed_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, false);
    wlmaker_primitives_draw_minimize_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_focussed_text_color);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_blurred_gfxbuf_ptr,
        0, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_minimize_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_blurred_text_color);
    cairo_destroy(cairo_ptr);

    if (NULL == titlebar_ptr->minimize_button_ptr) {
        titlebar_ptr->minimize_button_ptr = wlmaker_decorations_button_create(
            titlebar_ptr->wlr_scene_tree_ptr,
            titlebar_ptr->view_ptr->server_ptr->cursor_ptr,
            button_minimize_callback,
            titlebar_ptr->view_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.pressed_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr,
            WLR_EDGE_LEFT | WLR_EDGE_TOP);
        BS_ASSERT(NULL != titlebar_ptr->minimize_button_ptr);
    } else {
        wlmaker_decorations_button_set_textures(
            titlebar_ptr->minimize_button_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.pressed_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr);
    }

    drop_wlr_buffers(&buf_holder);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates (or updates) the "Close" button and textures.
 *
 * @param titlebar_ptr
 */
void create_or_update_close_button(
    wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    cairo_t *cairo_ptr;
    wlr_buffer_holder_t buf_holder;

    BS_ASSERT(create_wlr_buffers(
                  &buf_holder, wlmaker_decorations_button_width, true));

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.released_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_focussed_gfxbuf_ptr,
        titlebar_ptr->close_pos, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.released_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_close_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_focussed_text_color);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.pressed_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_focussed_gfxbuf_ptr,
        titlebar_ptr->close_pos, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.pressed_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, false);
    wlmaker_primitives_draw_close_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_focussed_text_color);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_blurred_gfxbuf_ptr,
        titlebar_ptr->close_pos, 0,
        wlmaker_decorations_button_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_close_icon(
        cairo_ptr, wlmaker_config_theme.titlebar_blurred_text_color);
    cairo_destroy(cairo_ptr);

    if (NULL == titlebar_ptr->close_button_ptr) {
        titlebar_ptr->close_button_ptr = wlmaker_decorations_button_create(
            titlebar_ptr->wlr_scene_tree_ptr,
            titlebar_ptr->view_ptr->server_ptr->cursor_ptr,
            button_close_callback,
            titlebar_ptr->view_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.pressed_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr,
            WLR_EDGE_RIGHT | WLR_EDGE_TOP);
        BS_ASSERT(NULL != titlebar_ptr->close_button_ptr);
    } else {
        wlmaker_decorations_button_set_textures(
            titlebar_ptr->close_button_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.pressed_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr);
    }

    drop_wlr_buffers(&buf_holder);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates (or updates) the title element and textures of the title bar.
 *
 * @param titlebar_ptr
 */
void create_or_update_title(wlmaker_decorations_titlebar_t *titlebar_ptr)
{
    cairo_t *cairo_ptr;
    wlr_buffer_holder_t buf_holder;

    BS_ASSERT(create_wlr_buffers(
                  &buf_holder, titlebar_ptr->title_width, false));

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.released_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_focussed_gfxbuf_ptr,
        titlebar_ptr->title_pos, 0,
        titlebar_ptr->title_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.released_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr,
        wlmaker_view_get_title(titlebar_ptr->view_ptr),
        wlmaker_config_theme.titlebar_focussed_text_color);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr),
        0, 0,
        titlebar_ptr->background_blurred_gfxbuf_ptr,
        titlebar_ptr->title_pos, 0,
        titlebar_ptr->title_width, wlmaker_decorations_titlebar_height);
    cairo_ptr = cairo_create_from_wlr_buffer(buf_holder.blurred_wlrbuf_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_primitives_draw_bezel(
        cairo_ptr, wlmaker_decorations_button_bezel_width, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr,
        wlmaker_view_get_title(titlebar_ptr->view_ptr),
        wlmaker_config_theme.titlebar_blurred_text_color);
    cairo_destroy(cairo_ptr);

    if (NULL == titlebar_ptr->title_ptr) {
        titlebar_ptr->title_ptr = wlmaker_decorations_title_create(
            titlebar_ptr->wlr_scene_tree_ptr,
            titlebar_ptr->view_ptr->server_ptr->cursor_ptr,
            titlebar_ptr->view_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr);
        BS_ASSERT(NULL != titlebar_ptr->title_ptr);
    } else {
        wlmaker_decorations_title_set_textures(
            titlebar_ptr->title_ptr,
            buf_holder.released_wlrbuf_ptr,
            buf_holder.blurred_wlrbuf_ptr);
    }

    drop_wlr_buffers(&buf_holder);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for the "minimize" button action.
 *
 * @param interactive_ptr     Points to the interactive that triggered the
 *                            action. Unused.
 * @param data_ptr            This view.
 */
void button_minimize_callback(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr,
    void *data_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)data_ptr;
    wlmaker_view_set_iconified(view_ptr, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for the close button action.
 *
 * @param interactive_ptr     Points to the interactive that triggered the
 *                            action. Unused.
 * @param data_ptr            This view.
 */
void button_close_callback(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr,
    void *data_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)data_ptr;
    view_ptr->send_close_callback(view_ptr);
}

/* == End of titlebar.c ==================================================== */
