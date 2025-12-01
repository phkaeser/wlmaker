/* ========================================================================= */
/**
 * @file clip.c
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

#include "clip.h"

#include <cairo.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "backend/backend.h"
#include "backend/output_config.h"
#include "files.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Clip handle. */
struct _wlmaker_clip_t {
    /** The clip happens to be derived from a tile. */
    wlmtk_tile_t              super_tile;
    /** Original virtual method table fo the superclass' element. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Backlink to the server. */
    wlmaker_server_t          *server_ptr;

    /** The toolkit dock, holding the clip tile. */
    wlmtk_dock_t              *wlmtk_dock_ptr;

    /** The tile's texture buffer without any buttons pressed */
    struct wlr_buffer         *tile_buffer_ptr;
    /** The tile's texture buffer with the 'Next' buttons pressed. */
    struct wlr_buffer         *next_pressed_tile_buffer_ptr;
    /** The tile's texture buffer with the 'Previous' buttons pressed. */
    struct wlr_buffer         *prev_pressed_tile_buffer_ptr;

    /** Overlay buffer element: Contains the workspace's title and number. */
    wlmtk_buffer_t            overlay_buffer;
    /** Clip image. */
    wlmtk_image_t             *image_ptr;

    /** Description of the desired output, if any. */
    wlmbe_output_description_t output_description;

    /** Whether the pointer is currently inside the 'prev' button. */
    bool                      pointer_inside_prev_button;
    /** Whether the pointer is currently inside the 'next' button. */
    bool                      pointer_inside_next_button;
    /** Whether the 'prev' button had been pressed. */
    bool                      prev_button_pressed;
    /** Whether the 'next' button had been pressed. */
    bool                      next_button_pressed;

    /** Listener for @ref wlmtk_root_events_t::workspace_changed. */
    struct wl_listener        workspace_changed_listener;
    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;
    /** Listener for @ref wlmtk_element_events_t::pointer_motion. */
    struct wl_listener        pointer_motion_listener;
    /** Listener for @ref wlmtk_element_events_t::pointer_leave. */
    struct wl_listener        pointer_leave_listener;

    /** The clip's style. */
    wlmaker_config_clip_style_t style;
};

static bool _wlmaker_clip_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static bool _wlmaker_clip_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmaker_clip_update_buttons(wlmaker_clip_t *clip_ptr);
static void _wlmaker_clip_update_overlay(wlmaker_clip_t *clip_ptr);
static struct wlr_buffer *_wlmaker_clip_create_tile(
    const wlmtk_tile_style_t *style_ptr,
    bool prev_pressed,
    bool next_pressed);

static void _wlmaker_clip_handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_clip_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_clip_handle_pointer_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_clip_handle_pointer_leave(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** The clip's extension to @ref wlmtk_element_t virtual method table. */
static const wlmtk_element_vmt_t _wlmaker_clip_element_vmt = {
    .pointer_axis = _wlmaker_clip_pointer_axis,
    .pointer_button = _wlmaker_clip_pointer_button,
};

/** TODO: Replace this. */
typedef struct {
    /** Positioning data. */
    wlmtk_dock_positioning_t  positioning;
} parse_args;

/** Enum descriptor for `enum wlr_edges`. */
static const bspl_enum_desc_t _wlmaker_clip_edges[] = {
    BSPL_ENUM("TOP", WLR_EDGE_TOP),
    BSPL_ENUM("BOTTOM", WLR_EDGE_BOTTOM),
    BSPL_ENUM("LEFT", WLR_EDGE_LEFT),
    BSPL_ENUM("RIGHT", WLR_EDGE_RIGHT),
    BSPL_ENUM_SENTINEL(),
};

/** Descriptor for the clip's plist. */
const bspl_desc_t _wlmaker_clip_desc[] = {
    BSPL_DESC_ENUM("Edge", true, parse_args,
                   positioning.edge, positioning.edge,
                   WLR_EDGE_NONE, _wlmaker_clip_edges),
    BSPL_DESC_ENUM("Anchor", true, parse_args,
                   positioning.anchor, positioning.anchor,
                   WLR_EDGE_NONE, _wlmaker_clip_edges),
    BSPL_DESC_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_clip_t *wlmaker_clip_create(
    wlmaker_server_t *server_ptr,
    bspl_dict_t *state_dict_ptr,
    const wlmaker_config_style_t *style_ptr)
{
    wlmaker_clip_t *clip_ptr = logged_calloc(1, sizeof(wlmaker_clip_t));
    if (NULL == clip_ptr) return NULL;
    clip_ptr->server_ptr = server_ptr;
    clip_ptr->style = style_ptr->clip;

    clip_ptr->tile_buffer_ptr = _wlmaker_clip_create_tile(
        &style_ptr->tile, false, false);
    clip_ptr->prev_pressed_tile_buffer_ptr = _wlmaker_clip_create_tile(
        &style_ptr->tile, true, false);
    clip_ptr->next_pressed_tile_buffer_ptr = _wlmaker_clip_create_tile(
        &style_ptr->tile, false, true);
    if (NULL == clip_ptr->tile_buffer_ptr ||
        NULL == clip_ptr->prev_pressed_tile_buffer_ptr ||
        NULL == clip_ptr->next_pressed_tile_buffer_ptr) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }

    parse_args args = {};
    bspl_dict_t *dict_ptr = bspl_dict_get_dict(state_dict_ptr, "Clip");
    if (NULL == dict_ptr) {
        bs_log(BS_ERROR, "No 'Clip' dict found in state.");
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    bspl_decode_dict(dict_ptr, _wlmaker_clip_desc, &args);
    bspl_dict_t *output_dict_ptr = bspl_dict_get_dict(dict_ptr, "Output");
    if (NULL != output_dict_ptr) {
        if (!wlmbe_output_description_init_from_plist(
                &clip_ptr->output_description, output_dict_ptr)) {
            wlmaker_clip_destroy(clip_ptr);
            return NULL;
        }
    }

    clip_ptr->wlmtk_dock_ptr = wlmtk_dock_create(
        &args.positioning, &style_ptr->dock);
    wlmtk_element_set_visible(
        wlmtk_dock_element(clip_ptr->wlmtk_dock_ptr),
        true);

    if (!wlmtk_tile_init(
            &clip_ptr->super_tile,
            &style_ptr->tile)) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    clip_ptr->orig_super_element_vmt = wlmtk_element_extend(
        wlmtk_tile_element(&clip_ptr->super_tile),
        &_wlmaker_clip_element_vmt);
    wlmtk_util_connect_listener_signal(
        &wlmtk_tile_element(&clip_ptr->super_tile)->events.pointer_motion,
        &clip_ptr->pointer_motion_listener,
        _wlmaker_clip_handle_pointer_motion);
    wlmtk_util_connect_listener_signal(
        &wlmtk_tile_element(&clip_ptr->super_tile)->events.pointer_leave,
        &clip_ptr->pointer_leave_listener,
        _wlmaker_clip_handle_pointer_leave);

    wlmtk_element_set_visible(
        wlmtk_tile_element(&clip_ptr->super_tile), true);
    wlmtk_tile_set_background_buffer(
        &clip_ptr->super_tile, clip_ptr->tile_buffer_ptr);
    wlmtk_dock_add_tile(clip_ptr->wlmtk_dock_ptr, &clip_ptr->super_tile);

    if (!wlmtk_buffer_init(&clip_ptr->overlay_buffer)) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_buffer_element(&clip_ptr->overlay_buffer), true);

    struct wlr_output *wlr_output_ptr = wlmbe_output_description_first_fnmatch(
        &clip_ptr->output_description, server_ptr->wlr_output_layout_ptr);
    if (NULL == wlr_output_ptr) {
        wlr_output_ptr = wlmbe_primary_output(
            server_ptr->wlr_output_layout_ptr);
    }

    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(server_ptr->root_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);
    if (!wlmtk_layer_add_panel(
            layer_ptr,
            wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr),
            wlr_output_ptr)) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }

    // Resolves to a full path, and verifies the icon file exists.
    char *path_ptr = wlmaker_files_xdg_data_find(
        server_ptr->files_ptr, "icons/clip-48x48.png", S_IFREG);
    if (NULL == path_ptr) {
        bs_log(
            BS_ERROR,
            "Failed to locate ${XDG_DATA_DIRS}/wlmaker/icons/clip-48x48.png");
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    clip_ptr->image_ptr = wlmtk_image_create_scaled(
        path_ptr,
        clip_ptr->super_tile.style.content_size,
        clip_ptr->super_tile.style.content_size);
    free(path_ptr);
    if (NULL == clip_ptr->image_ptr) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_image_element(clip_ptr->image_ptr), true);
    wlmtk_tile_set_content(
        &clip_ptr->super_tile,
        wlmtk_image_element(clip_ptr->image_ptr));

    _wlmaker_clip_update_overlay(clip_ptr);
    wlmtk_tile_set_overlay(
        &clip_ptr->super_tile,
        wlmtk_buffer_element(&clip_ptr->overlay_buffer));

    wlmtk_util_connect_listener_signal(
        &wlmtk_root_events(server_ptr->root_ptr)->workspace_changed,
        &clip_ptr->workspace_changed_listener,
        _wlmaker_clip_handle_workspace_changed);

    // TODO(kaeser@gubbe.ch): This is a very hacky way of updating the output
    // before the layer's handler removes all associated panels. Should be
    // a native method of wlmtk_dock_t or wlmtk_panel_t.
    clip_ptr->output_layout_change_listener.notify =
        _wlmaker_clip_handle_output_layout_change;
    wl_list_insert(
        server_ptr->wlr_output_layout_ptr->events.change.listener_list.next,
        &clip_ptr->output_layout_change_listener.link);

    server_ptr->clip_dock_ptr = clip_ptr->wlmtk_dock_ptr;
    bs_log(BS_INFO, "Created clip %p", clip_ptr);
    return clip_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_clip_destroy(wlmaker_clip_t *clip_ptr)
{
    if (NULL != clip_ptr->server_ptr) {
        clip_ptr->server_ptr->clip_dock_ptr = NULL;
    }

    wlmtk_util_disconnect_listener(&clip_ptr->output_layout_change_listener);
    wlmtk_util_disconnect_listener(&clip_ptr->workspace_changed_listener);

    if (wlmtk_tile_element(&clip_ptr->super_tile)->parent_container_ptr) {
        wlmtk_tile_set_content(&clip_ptr->super_tile, NULL);
        wlmtk_tile_set_overlay(&clip_ptr->super_tile, NULL);
        wlmtk_dock_remove_tile(
            clip_ptr->wlmtk_dock_ptr,
            &clip_ptr->super_tile);
    }
    wlmtk_util_disconnect_listener(&clip_ptr->pointer_leave_listener);
    wlmtk_util_disconnect_listener(&clip_ptr->pointer_motion_listener);
    wlmtk_tile_fini(&clip_ptr->super_tile);
    wlmtk_buffer_fini(&clip_ptr->overlay_buffer);

    if (NULL != clip_ptr->image_ptr) {
        wlmtk_image_destroy(clip_ptr->image_ptr);
        clip_ptr->image_ptr = NULL;
    }

    if (NULL != clip_ptr->wlmtk_dock_ptr) {
        if (NULL != wlmtk_panel_get_layer(
                wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr))) {
            wlmtk_layer_remove_panel(
                wlmtk_panel_get_layer(wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr)),
                wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr));
        }
        wlmtk_dock_destroy(clip_ptr->wlmtk_dock_ptr);
        clip_ptr->wlmtk_dock_ptr = NULL;
    }

    if (NULL != clip_ptr->tile_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->tile_buffer_ptr);
        clip_ptr->tile_buffer_ptr = NULL;
    }
    if (NULL != clip_ptr->prev_pressed_tile_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->prev_pressed_tile_buffer_ptr);
        clip_ptr->prev_pressed_tile_buffer_ptr = NULL;
    }
    if (NULL != clip_ptr->next_pressed_tile_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->next_pressed_tile_buffer_ptr);
        clip_ptr->next_pressed_tile_buffer_ptr = NULL;
    }

    wlmbe_output_description_fini(&clip_ptr->output_description);
    free(clip_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_axis.
 *
 * Moves to the next or previous workspace, depending on the axis (scroll-
 * wheel) direction.
 *
 * @param element_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return true
 */
bool _wlmaker_clip_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_clip_t,
        super_tile.super_container.super_element);

    if (0 > wlr_pointer_axis_event_ptr->delta) {
        // Scroll wheel "up" -> next.
        wlmtk_root_switch_to_next_workspace(clip_ptr->server_ptr->root_ptr);
    } else if (0 < wlr_pointer_axis_event_ptr->delta) {
        // Scroll wheel "down" -> next.
        wlmtk_root_switch_to_previous_workspace(clip_ptr->server_ptr->root_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_button.
 *
 * Checks if the button press is on either 'next' or 'prev' button area,
 * updates visualization if pressed, and switches workspace if needed.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true.
 */
bool _wlmaker_clip_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_clip_t,
        super_tile.super_container.super_element);

    if (BTN_LEFT != button_event_ptr->button) return true;

    switch (button_event_ptr->type) {
    case WLMTK_BUTTON_DOWN:
        // Pointer button tressed. Translate to button press if in area.
        if (clip_ptr->pointer_inside_next_button ||
            clip_ptr->pointer_inside_next_button) {
            clip_ptr->next_button_pressed = true;
            clip_ptr->prev_button_pressed = false;
        } else if (clip_ptr->pointer_inside_prev_button ||
                   clip_ptr->pointer_inside_prev_button) {
            clip_ptr->next_button_pressed = false;
            clip_ptr->prev_button_pressed = true;
        }
        break;

    case WLMTK_BUTTON_UP:
        // Button is released (closed the click). If we're within the area of
        // the pressed button: Trigger the action.
        if ((clip_ptr->pointer_inside_next_button ||
             clip_ptr->pointer_inside_next_button) &&
            clip_ptr->next_button_pressed) {
            clip_ptr->next_button_pressed = false;
            wlmtk_root_switch_to_next_workspace(
                clip_ptr->server_ptr->root_ptr);
        } else if ((clip_ptr->pointer_inside_prev_button ||
                    clip_ptr->pointer_inside_prev_button) &&
                   clip_ptr->prev_button_pressed) {
            clip_ptr->prev_button_pressed = false;
            wlmtk_root_switch_to_previous_workspace(
                clip_ptr->server_ptr->root_ptr);
        }
        break;

    case WLMTK_BUTTON_CLICK:
    default:
        break;
    }

    _wlmaker_clip_update_buttons(clip_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates the button textures, based on current state what's pressed. */
static void _wlmaker_clip_update_buttons(wlmaker_clip_t *clip_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = clip_ptr->tile_buffer_ptr;
    if ((clip_ptr->pointer_inside_next_button ||
         clip_ptr->pointer_inside_next_button)&&
        clip_ptr->next_button_pressed) {
        wlr_buffer_ptr = clip_ptr->next_pressed_tile_buffer_ptr;
    } else if ((clip_ptr->pointer_inside_prev_button ||
                clip_ptr->pointer_inside_prev_button) &&
               clip_ptr->prev_button_pressed) {
        wlr_buffer_ptr = clip_ptr->prev_pressed_tile_buffer_ptr;
    }
    wlmtk_tile_set_background_buffer(&clip_ptr->super_tile, wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/** Updates the overlay buffer's content with workspace name and index. */
void _wlmaker_clip_update_overlay(wlmaker_clip_t *clip_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        clip_ptr->super_tile.style.size, clip_ptr->super_tile.style.size);
    if (NULL == wlr_buffer_ptr) return;

    int index = 0;
    const char *name_ptr = NULL;
    wlmtk_workspace_get_details(
        wlmtk_root_get_current_workspace(clip_ptr->server_ptr->root_ptr),
        &name_ptr, &index);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return;
    }

    cairo_select_font_face(
        cairo_ptr,
        clip_ptr->style.font.face,
        CAIRO_FONT_SLANT_NORMAL,
        wlmtk_style_font_weight_cairo_from_wlmtk(clip_ptr->style.font.weight));
    cairo_set_font_size(cairo_ptr, clip_ptr->style.font.size);
    cairo_set_source_argb8888(cairo_ptr, clip_ptr->style.text_color);
    cairo_move_to(
        cairo_ptr,
        clip_ptr->style.font.size * 4 / 12,
        clip_ptr->style.font.size * 2 / 12 + clip_ptr->style.font.size);
    cairo_show_text(cairo_ptr, name_ptr);

    cairo_move_to(
        cairo_ptr,
        clip_ptr->super_tile.style.size - clip_ptr->style.font.size * 14 / 12,
        clip_ptr->super_tile.style.size - clip_ptr->style.font.size * 8 / 12);
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", index);
    cairo_show_text(cairo_ptr, buf);

    cairo_destroy(cairo_ptr);

    wlmtk_buffer_set(&clip_ptr->overlay_buffer, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a wlr_buffer with texture suitable to show the 'next' and 'prev'
 * buttons in each raised or pressed state.
 *
 * @param style_ptr
 * @param prev_pressed
 * @param next_pressed
 *
 * @return A wlr buffer.
 */
struct wlr_buffer *_wlmaker_clip_create_tile(
    const wlmtk_tile_style_t *style_ptr,
    bool prev_pressed,
    bool next_pressed)
{
    struct wlr_buffer* wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        style_ptr->size, style_ptr->size);
    if (NULL == wlr_buffer_ptr) return NULL;

    double tsize = style_ptr->size;
    double bsize = 22.0 / 64.0 * style_ptr->size;
    double margin = style_ptr->bezel_width;

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }

    wlmaker_primitives_cairo_fill(cairo_ptr, &style_ptr->fill);

    // Northern + Western sides. Drawn clock-wise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, 0, 0);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize - bsize, margin);
    cairo_line_to(cairo_ptr, margin, margin);
    cairo_line_to(cairo_ptr, margin, tsize - bsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize);
    cairo_line_to(cairo_ptr, 0, 0);
    cairo_fill(cairo_ptr);

    // Southern + Eastern sides. Also drawn Also clock-wise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, tsize, tsize);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, bsize, tsize - margin);
    cairo_line_to(cairo_ptr, tsize - margin, tsize - margin);
    cairo_line_to(cairo_ptr, tsize - margin, bsize);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize, tsize);
    cairo_fill(cairo_ptr);

    // Diagonal at the north-eastern corner. Drawn clockwise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize - margin, bsize);
    cairo_line_to(cairo_ptr, tsize - bsize, margin);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_fill(cairo_ptr);

    // Diagonal at south-western corner. Drawn clockwise.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, 0, tsize - bsize);
    cairo_line_to(cairo_ptr, margin, tsize - bsize);
    cairo_line_to(cairo_ptr, bsize, tsize - margin);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize);
    cairo_fill(cairo_ptr);

    // The "Next" button, north-eastern corner.
    // Northern edge, illuminated when raised
    wlmaker_primitives_set_bezel_color(cairo_ptr, !next_pressed);
    cairo_move_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize, 0);
    cairo_line_to(cairo_ptr, tsize - margin, margin);
    cairo_line_to(cairo_ptr, tsize - bsize + 2 * margin, margin);
    cairo_line_to(cairo_ptr, tsize - bsize , 0);
    cairo_fill(cairo_ptr);

    // Eastern edge, illuminated when pressed
    wlmaker_primitives_set_bezel_color(cairo_ptr, next_pressed);
    cairo_move_to(cairo_ptr, tsize, 0);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize - margin, bsize - 2 * margin);
    cairo_line_to(cairo_ptr, tsize - margin,  margin);
    cairo_line_to(cairo_ptr, tsize, 0);
    cairo_fill(cairo_ptr);

    // Diagonal, illuminated when pressed.
    wlmaker_primitives_set_bezel_color(cairo_ptr, next_pressed);
    cairo_move_to(cairo_ptr, tsize - bsize, 0);
    cairo_line_to(cairo_ptr, tsize - bsize + 2 * margin, margin);
    cairo_line_to(cairo_ptr, tsize - margin, bsize - 2 *margin);
    cairo_line_to(cairo_ptr, tsize, bsize);
    cairo_line_to(cairo_ptr, tsize - bsize, 0);
    cairo_fill(cairo_ptr);

    // The black triangle. Use relative sizes.
    double tpad = bsize * 5.0 / 22.0;
    double trsize = bsize * 7.0 / 22.0;
    double tmargin = bsize * 1.0 / 22.0;
    cairo_set_source_rgba(cairo_ptr, 0, 0, 0, 1.0);
    cairo_move_to(cairo_ptr, tsize - tpad, tpad);
    cairo_line_to(cairo_ptr, tsize - tpad, trsize + tpad);
    cairo_line_to(cairo_ptr, tsize - tpad - trsize, tpad);
    cairo_line_to(cairo_ptr, tsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    // Northern edge of triangle, not illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, tsize - tpad, tpad);
    cairo_line_to(cairo_ptr, tsize - tpad - trsize, tpad);
    cairo_line_to(cairo_ptr, tsize - tpad - trsize - tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, tsize - tpad + tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, tsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    // Eastern side of triangle, illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, tsize - tpad, tpad);
    cairo_line_to(cairo_ptr, tsize - tpad + tmargin, tpad - tmargin);
    cairo_line_to(cairo_ptr, tsize - tpad + tmargin, tpad + trsize + tmargin);
    cairo_line_to(cairo_ptr, tsize - tpad, tpad + trsize);
    cairo_line_to(cairo_ptr, tsize - tpad, tpad);
    cairo_fill(cairo_ptr);

    // The "Prev" button, south-western corner.
    // Southern edge, illuminated when pressed.
    wlmaker_primitives_set_bezel_color(cairo_ptr, prev_pressed);
    cairo_move_to(cairo_ptr, 0, tsize);
    cairo_line_to(cairo_ptr, margin, tsize - margin);
    cairo_line_to(cairo_ptr, bsize - 2 * margin, tsize - margin);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, 0, tsize);
    cairo_fill(cairo_ptr);

    // Western edge, illuminated when raised.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !prev_pressed);
    cairo_move_to(cairo_ptr, 0, tsize);
    cairo_line_to(cairo_ptr, 0, tsize - bsize + 0);
    cairo_line_to(cairo_ptr, margin, tsize - bsize + 2 * margin);
    cairo_line_to(cairo_ptr, margin, tsize - margin);
    cairo_line_to(cairo_ptr, 0, tsize);
    cairo_fill(cairo_ptr);

    // Diagonal, illuminated when raised.
    wlmaker_primitives_set_bezel_color(cairo_ptr, !prev_pressed);
    cairo_move_to(cairo_ptr, 0, tsize - bsize + 0);
    cairo_line_to(cairo_ptr, bsize, tsize);
    cairo_line_to(cairo_ptr, bsize - 2 * margin, tsize - margin);
    cairo_line_to(cairo_ptr, margin, tsize - bsize + 2 * margin);
    cairo_line_to(cairo_ptr, 0, tsize - bsize + 0);
    cairo_fill(cairo_ptr);

    // The black triangle. Use relative sizes.
    cairo_set_source_rgba(cairo_ptr, 0, 0, 0, 1.0);
    cairo_move_to(cairo_ptr, tpad, tsize - tpad);
    cairo_line_to(cairo_ptr, tpad, tsize - trsize - tpad);
    cairo_line_to(cairo_ptr, tpad + trsize, tsize - tpad);
    cairo_line_to(cairo_ptr, tpad, tsize - tpad);
    cairo_fill(cairo_ptr);

    // Southern edge of triangle, illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, true);
    cairo_move_to(cairo_ptr, tpad, tsize - tpad);
    cairo_line_to(cairo_ptr, tpad + trsize, tsize - tpad);
    cairo_line_to(cairo_ptr, tpad + trsize + tmargin, tsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad - tmargin, tsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad, tsize - tpad);
    cairo_fill(cairo_ptr);

    // Eastern side of triangle, not illuminated.
    wlmaker_primitives_set_bezel_color(cairo_ptr, false);
    cairo_move_to(cairo_ptr, tpad, tsize - tpad);
    cairo_line_to(cairo_ptr, tpad - tmargin, tsize - tpad + tmargin);
    cairo_line_to(cairo_ptr, tpad - tmargin, tsize - tpad - trsize - tmargin);
    cairo_line_to(cairo_ptr, tpad, tsize - tpad - trsize);
    cairo_line_to(cairo_ptr, tpad, tsize - tpad);
    cairo_fill(cairo_ptr);

    cairo_destroy(cairo_ptr);
    return wlr_buffer_ptr;
}

/* -------------------------------------------------------------------------- */
/**
 * Handler for the `workspace_changed` signal of `wlmaker_server_t`.
 *
 * Will redraw the clip contents with the current workspace, and re-map the
 * clip to the new workspace.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the new `wlmtk_workspace_t`.
 */
void _wlmaker_clip_handle_workspace_changed(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_clip_t, workspace_changed_listener);
    wlmtk_panel_t *panel_ptr = wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr);

    wlmtk_layer_t *current_layer_ptr = wlmtk_panel_get_layer(panel_ptr);
    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(clip_ptr->server_ptr->root_ptr);
    wlmtk_layer_t *new_layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);

    if (current_layer_ptr == new_layer_ptr) return;

    if (NULL != current_layer_ptr) {
        wlmtk_layer_remove_panel(current_layer_ptr, panel_ptr);
    }
    BS_ASSERT(wlmtk_layer_add_panel(
                  new_layer_ptr,
                  panel_ptr,
                  wlmbe_primary_output(
                      clip_ptr->server_ptr->wlr_output_layout_ptr)));

    _wlmaker_clip_update_overlay(clip_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles when output layout changes; Re-computes the output to attach. */
void _wlmaker_clip_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_clip_t, output_layout_change_listener);

    struct wlr_output *wlr_output_ptr = wlmbe_output_description_first_fnmatch(
        &clip_ptr->output_description, clip_ptr->server_ptr->wlr_output_layout_ptr);
    if (NULL == wlr_output_ptr) {
        wlr_output_ptr = wlmbe_primary_output(
            clip_ptr->server_ptr->wlr_output_layout_ptr);
    }
    wlmtk_layer_t *layer_ptr = wlmtk_panel_get_layer(
        wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr));
    wlmtk_layer_remove_panel(layer_ptr, wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr));
    if (NULL != wlr_output_ptr) {
        BS_ASSERT(wlmtk_layer_add_panel(
                      layer_ptr,
                      wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr),
                      wlr_output_ptr));
    }
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_element_events_t::pointer_leave. Resets buttons. */
void _wlmaker_clip_handle_pointer_leave(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_clip_t, pointer_leave_listener);

    clip_ptr->pointer_inside_prev_button = false;
    clip_ptr->pointer_inside_next_button = false;
    _wlmaker_clip_update_buttons(clip_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_element_events_t::pointer_motion. */
void _wlmaker_clip_handle_pointer_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_clip_t, pointer_motion_listener);
    wlmtk_pointer_motion_event_t *motion_event_ptr = data_ptr;

    clip_ptr->pointer_inside_prev_button = false;
    clip_ptr->pointer_inside_next_button = false;

    double tile_size = clip_ptr->super_tile.style.size;
    double button_size = (22.0 / 64.0) * tile_size;
    if (motion_event_ptr->x >= tile_size - button_size &&
        motion_event_ptr->x < tile_size &&
        motion_event_ptr->y >= 0 &&
        motion_event_ptr->y < button_size) {
        // Next button.
        clip_ptr->pointer_inside_next_button = true;
    } else if (motion_event_ptr->x >= 0 &&
               motion_event_ptr->x < button_size &&
               motion_event_ptr->y >= tile_size - button_size &&
               motion_event_ptr->y < tile_size) {
        // Prev button.
        clip_ptr->pointer_inside_prev_button = true;
    }

    _wlmaker_clip_update_buttons(clip_ptr);
}

/* == Unit tests =========================================================== */

static void test_draw_tile(bs_test_t *test_ptr);

/** Test cases. */
static const bs_test_case_t wlmaker_clip_test_cases[] = {
    { true, "draw_tile", test_draw_tile },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_clip_test_set = BS_TEST_SET(
    true, "clip", wlmaker_clip_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests that the clip tile is drawn correctly. */
void test_draw_tile(bs_test_t *test_ptr)
{
    static const wlmtk_tile_style_t style = {
        .fill = {
            .type = WLMTK_STYLE_COLOR_DGRADIENT,
            .param = { .dgradient = { .from = 0xffa6a6b6, .to = 0xff515561 } }
        },
        .bezel_width = 2,
        .size = 64
    };
    struct wlr_buffer* wlr_buffer_ptr;
    bs_gfxbuf_t *gfxbuf_ptr;

    wlr_buffer_ptr = _wlmaker_clip_create_tile(&style, false, false);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlr_buffer_ptr);
    gfxbuf_ptr = bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "clip_raised.png");
    wlr_buffer_drop(wlr_buffer_ptr);

    wlr_buffer_ptr = _wlmaker_clip_create_tile(&style, true, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlr_buffer_ptr);
    gfxbuf_ptr = bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "clip_pressed.png");
    wlr_buffer_drop(wlr_buffer_ptr);
}

/* == End of clip.c ======================================================== */
