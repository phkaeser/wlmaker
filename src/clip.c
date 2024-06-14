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

#include <limits.h>

#include "button.h"
#include "config.h"
#include "decorations.h"
#include "toolkit/toolkit.h"
#include "default_dock_state.h"

/* == Declarations ========================================================= */

/** Clip handle. */
struct _wlmaker_clip_t {
    /** The clip happens to be derived from a tile. */
    wlmtk_tile_t              super_tile;
    /** Original virtual method table fo the element. */
    wlmtk_element_vmt_t       orig_element_vmt;

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




    /** Corresponding view. */
    wlmaker_view_t            view;
    /** Backlink to the server. */
    wlmaker_server_t          *server_ptr;

    /** Scene graph subtree holding all layers of the clip. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;
    /** Texture buffer for the tile, as a `wlr_scene_buffer`. */
    struct wlr_buffer         *tile_wlr_buffer_ptr;

    /** Scene graph node holding the principal tile. */
    struct wlr_scene_buffer   *tile_scene_buffer_ptr;
    /** Scene graph node holding the "previous workspace" button. */
    struct wlr_scene_buffer   *prev_scene_buffer_ptr;
    /** Scene graph node holding the "next workspace' button. */
    struct wlr_scene_buffer   *next_scene_buffer_ptr;

    /** Texture of the "previous workspace" button, released state. */
    struct wlr_buffer         *prev_button_released_buffer_ptr;
    /** Texture of the "previous workspace" button, pressed state. */
    struct wlr_buffer         *prev_button_pressed_buffer_ptr;
    /** Texture of the "next workspace" button, released state. */
    struct wlr_buffer         *next_button_released_buffer_ptr;
    /** Texture of the "next workspace" button, pressed state. */
    struct wlr_buffer         *next_button_pressed_buffer_ptr;

    /** Interactive holding the "previuos workspace" button. */
    wlmaker_interactive_t     *prev_interactive_ptr;
    /** Interactive holding the "next workspace" button. */
    wlmaker_interactive_t     *next_interactive_ptr;

    /** Listener for the `workspace_changed` signal by `wlmaker_server_t`. */
    struct wl_listener        workspace_changed_listener;
};

static wlmaker_clip_t *clip_from_view(wlmaker_view_t *view_ptr);
static void clip_get_size(wlmaker_view_t *view_ptr,
                          uint32_t *width_ptr,
                          uint32_t *height_ptr);

static void draw_workspace(cairo_t *cairo_ptr, int num, const char *name_ptr);

static void callback_prev(wlmaker_interactive_t *interactive_ptr,
                          void *data_ptr);
static void callback_next(wlmaker_interactive_t *interactive_ptr,
                          void *data_ptr);

static void handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_axis(
    wlmaker_view_t *view_ptr,
    struct wlr_pointer_axis_event *event_ptr);

static bool _wlmaker_clip_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static bool _wlmaker_clip_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmaker_clip_update_overlay(wlmaker_clip_t *clip_ptr);
static struct wlr_buffer *_wlmaker_clip_create_tile(
    const wlmtk_tile_style_t *style_ptr,
    bool prev_pressed,
    bool next_pressed);

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
static const wlmcfg_enum_desc_t _wlmaker_clip_edges[] = {
    WLMCFG_ENUM("TOP", WLR_EDGE_TOP),
    WLMCFG_ENUM("BOTTOM", WLR_EDGE_BOTTOM),
    WLMCFG_ENUM("LEFT", WLR_EDGE_LEFT),
    WLMCFG_ENUM("RIGHT", WLR_EDGE_RIGHT),
    WLMCFG_ENUM_SENTINEL(),
};

/** Descriptor for the clip's plist. */
const wlmcfg_desc_t _wlmaker_clip_desc[] = {
    WLMCFG_DESC_ENUM("Edge", true, parse_args, positioning.edge,
                     WLR_EDGE_NONE, _wlmaker_clip_edges),
    WLMCFG_DESC_ENUM("Anchor", true, parse_args, positioning.anchor,
                     WLR_EDGE_NONE, _wlmaker_clip_edges),
    WLMCFG_DESC_SENTINEL(),
};

/** View implementor methods. */
const wlmaker_view_impl_t     clip_view_impl = {
    .set_activated = NULL,
    .get_size = clip_get_size,
    .handle_axis = handle_axis
};

/** Lookup paths for icons -- FIXME: de-duplicate this! */
static const char *lookup_paths[] = {
    "/usr/share/icons/wlmaker",
    "/usr/local/share/icons/wlmaker",
#if defined(WLMAKER_SOURCE_DIR)
    WLMAKER_SOURCE_DIR "/icons",
#endif  // WLMAKER_SOURCE_DIR
#if defined(WLMAKER_ICON_DATA_DIR)
    WLMAKER_ICON_DATA_DIR,
#endif  // WLMAKER_ICON_DATA_DIR
    NULL
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_clip_t *wlmaker_clip_create(
    wlmaker_server_t *server_ptr,
    const wlmaker_config_style_t *style_ptr)
{
    wlmaker_clip_t *clip_ptr = logged_calloc(1, sizeof(wlmaker_clip_t));
    if (NULL == clip_ptr) return NULL;
    clip_ptr->server_ptr = server_ptr;

    if (true) {

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
        wlmcfg_object_t *object_ptr = wlmcfg_create_object_from_plist_data(
            embedded_binary_default_dock_state_data,
            embedded_binary_default_dock_state_size);
        BS_ASSERT(NULL != object_ptr);
        wlmcfg_dict_t *dict_ptr = wlmcfg_dict_get_dict(
            wlmcfg_dict_from_object(object_ptr), "Clip");
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "No 'Clip' dict found in configuration.");
            wlmaker_clip_destroy(clip_ptr);
            return NULL;
        }
        wlmcfg_decode_dict(dict_ptr, _wlmaker_clip_desc, &args);
        wlmcfg_object_unref(object_ptr);

        clip_ptr->wlmtk_dock_ptr = wlmtk_dock_create(
            &args.positioning, &style_ptr->dock, server_ptr->env_ptr);
        wlmtk_element_set_visible(
            wlmtk_dock_element(clip_ptr->wlmtk_dock_ptr),
            true);

        if (!wlmtk_tile_init(
                &clip_ptr->super_tile,
                &style_ptr->tile,
                server_ptr->env_ptr)) {
            wlmaker_clip_destroy(clip_ptr);
            return NULL;
        }
        clip_ptr->orig_element_vmt = wlmtk_element_extend(
            wlmtk_tile_element(&clip_ptr->super_tile),
            &_wlmaker_clip_element_vmt);
        wlmtk_element_set_visible(
            wlmtk_tile_element(&clip_ptr->super_tile), true);
        wlmtk_tile_set_background_buffer(
            &clip_ptr->super_tile, clip_ptr->tile_buffer_ptr);
        wlmtk_dock_add_tile(clip_ptr->wlmtk_dock_ptr, &clip_ptr->super_tile);

        if (!wlmtk_buffer_init(
                &clip_ptr->overlay_buffer, server_ptr->env_ptr)) {
            wlmaker_clip_destroy(clip_ptr);
            return NULL;
        }
        wlmtk_element_set_visible(
            wlmtk_buffer_element(&clip_ptr->overlay_buffer), true);

        wlmtk_workspace_t *wlmtk_workspace_ptr =
            wlmaker_server_get_current_wlmtk_workspace(server_ptr);
        wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
            wlmtk_workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);
        wlmtk_layer_add_panel(
            layer_ptr,
            wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr));

        char full_path[PATH_MAX];
        char *path_ptr = bs_file_resolve_and_lookup_from_paths(
            "clip-48x48.png", lookup_paths, 0, full_path);
        if (NULL == path_ptr) {
            bs_log(BS_ERROR | BS_ERRNO,
                   "Failed bs_file_resolve_and_lookup_from_paths(\"clip-48x48.png\" ...)");
            wlmaker_clip_destroy(clip_ptr);
            return NULL;
        }
        clip_ptr->image_ptr = wlmtk_image_create(path_ptr, server_ptr->env_ptr);
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
    }

    clip_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        &server_ptr->void_wlr_scene_ptr->tree);
    if (NULL == clip_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_tree_create()");
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }

    /**
     * TODO(kaeser@gubbe.ch): Cleanup the code here - move the drawing
     * functions to a separate function and make sure the return values
     * are all respected.
     */
    clip_ptr->tile_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    if (NULL == clip_ptr->tile_wlr_buffer_ptr) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(
        clip_ptr->tile_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    bool drawn = wlmaker_decorations_draw_clip(
        cairo_ptr,
        &wlmaker_config_theme.tile_fill,
        false);
    int index = 0;
    const char *name_ptr = NULL;
    wlmaker_workspace_get_details(
        wlmaker_server_get_current_workspace(server_ptr),
        &index, &name_ptr);
    draw_workspace(cairo_ptr, index, name_ptr);
    cairo_destroy(cairo_ptr);

    if (!drawn) {
        bs_log(BS_ERROR, "Failed draw_texture().");
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }

    clip_ptr->tile_scene_buffer_ptr = wlr_scene_buffer_create(
        clip_ptr->wlr_scene_tree_ptr, clip_ptr->tile_wlr_buffer_ptr);
    if (NULL == clip_ptr->tile_scene_buffer_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_buffer_create()");
        wlmaker_clip_destroy(clip_ptr);
        return NULL;
    }
    clip_ptr->tile_scene_buffer_ptr->node.data = &clip_ptr->view;

    clip_ptr->prev_scene_buffer_ptr = wlr_scene_buffer_create(
        clip_ptr->wlr_scene_tree_ptr, NULL);
    wlr_scene_node_set_position(
        &clip_ptr->prev_scene_buffer_ptr->node,
        0,
        wlmaker_decorations_tile_size - wlmaker_decorations_clip_button_size);

    clip_ptr->next_scene_buffer_ptr = wlr_scene_buffer_create(
        clip_ptr->wlr_scene_tree_ptr, NULL);
    wlr_scene_node_set_position(
        &clip_ptr->next_scene_buffer_ptr->node,
        wlmaker_decorations_tile_size - wlmaker_decorations_clip_button_size,
        0);

    clip_ptr->prev_button_released_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    cairo_ptr = cairo_create_from_wlr_buffer(
        clip_ptr->prev_button_released_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_decorations_draw_clip_button_prev(
        cairo_ptr,  &wlmaker_config_theme.tile_fill, false);
    cairo_destroy(cairo_ptr);

    clip_ptr->prev_button_pressed_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    cairo_ptr = cairo_create_from_wlr_buffer(
        clip_ptr->prev_button_pressed_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_decorations_draw_clip_button_prev(
        cairo_ptr,  &wlmaker_config_theme.tile_fill, true);
    cairo_destroy(cairo_ptr);

    clip_ptr->prev_interactive_ptr = wlmaker_button_create(
        clip_ptr->prev_scene_buffer_ptr,
        server_ptr->cursor_ptr,
        callback_prev,
        clip_ptr,
        clip_ptr->prev_button_released_buffer_ptr,
        clip_ptr->prev_button_pressed_buffer_ptr,
        clip_ptr->prev_button_released_buffer_ptr);

    clip_ptr->next_button_released_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    cairo_ptr = cairo_create_from_wlr_buffer(
        clip_ptr->next_button_released_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_decorations_draw_clip_button_next(
        cairo_ptr, &wlmaker_config_theme.tile_fill, false);
    cairo_destroy(cairo_ptr);
    clip_ptr->next_button_pressed_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        wlmaker_decorations_clip_button_size,
        wlmaker_decorations_clip_button_size);
    cairo_ptr = cairo_create_from_wlr_buffer(
        clip_ptr->next_button_pressed_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_decorations_draw_clip_button_next(
        cairo_ptr, &wlmaker_config_theme.tile_fill, true);
    cairo_destroy(cairo_ptr);

    clip_ptr->next_interactive_ptr = wlmaker_button_create(
        clip_ptr->next_scene_buffer_ptr,
        server_ptr->cursor_ptr,
        callback_next,
        clip_ptr,
        clip_ptr->next_button_released_buffer_ptr,
        clip_ptr->next_button_pressed_buffer_ptr,
        clip_ptr->next_button_released_buffer_ptr);

    clip_ptr->prev_scene_buffer_ptr->node.data = &clip_ptr->view;
    clip_ptr->next_scene_buffer_ptr->node.data = &clip_ptr->view;

    wlmaker_view_init(
        &clip_ptr->view,
        &clip_view_impl,
        server_ptr,
        NULL,  // wlr_surface_ptr.
        clip_ptr->wlr_scene_tree_ptr,
        NULL);  // send_close_callback
    wlmaker_view_set_title(&clip_ptr->view, "WLMaker Clip");

    bs_avltree_insert(
        clip_ptr->view.interactive_tree_ptr,
        &clip_ptr->prev_scene_buffer_ptr->node,
        &clip_ptr->prev_interactive_ptr->avlnode,
        false);
    bs_avltree_insert(
        clip_ptr->view.interactive_tree_ptr,
        &clip_ptr->next_scene_buffer_ptr->node,
        &clip_ptr->next_interactive_ptr->avlnode,
        false);

    clip_ptr->view.anchor =
        WLMAKER_VIEW_ANCHOR_BOTTOM | WLMAKER_VIEW_ANCHOR_RIGHT;

    wlmtk_util_connect_listener_signal(
        &server_ptr->workspace_changed,
        &clip_ptr->workspace_changed_listener,
        handle_workspace_changed);

    wlmaker_view_map(
        &clip_ptr->view,
        wlmaker_server_get_current_workspace(clip_ptr->server_ptr),
        WLMAKER_WORKSPACE_LAYER_TOP);
    bs_log(BS_INFO, "Created clip view %p", &clip_ptr->view);
    return clip_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_clip_destroy(wlmaker_clip_t *clip_ptr)
{
    wl_list_remove(&clip_ptr->workspace_changed_listener.link);

    wlmaker_view_unmap(&clip_ptr->view);
    wlmaker_view_fini(&clip_ptr->view);

    // TODO(kaeser@gubbe.ch): Find a means of cleaning wlr_scene_tree_ptr.

    if (NULL != clip_ptr->tile_wlr_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->tile_wlr_buffer_ptr);
        clip_ptr->tile_wlr_buffer_ptr = NULL;
    }

    if (NULL != clip_ptr->prev_button_released_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->prev_button_released_buffer_ptr);
        clip_ptr->prev_button_released_buffer_ptr = NULL;
    }
    if (NULL != clip_ptr->prev_button_pressed_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->prev_button_pressed_buffer_ptr);
        clip_ptr->prev_button_pressed_buffer_ptr = NULL;
    }
    if (NULL != clip_ptr->next_button_released_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->next_button_released_buffer_ptr);
        clip_ptr->next_button_released_buffer_ptr = NULL;
    }
    if (NULL != clip_ptr->next_button_pressed_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->next_button_pressed_buffer_ptr);
        clip_ptr->next_button_pressed_buffer_ptr = NULL;
    }







    if (wlmtk_tile_element(&clip_ptr->super_tile)->parent_container_ptr) {
        wlmtk_tile_set_content(&clip_ptr->super_tile, NULL);
        wlmtk_tile_set_overlay(&clip_ptr->super_tile, NULL);
        wlmtk_dock_remove_tile(
            clip_ptr->wlmtk_dock_ptr,
            &clip_ptr->super_tile);
    }
    wlmtk_tile_fini(&clip_ptr->super_tile);
    wlmtk_buffer_fini(&clip_ptr->overlay_buffer);

    if (NULL != clip_ptr->image_ptr) {
        wlmtk_image_destroy(clip_ptr->image_ptr);
        clip_ptr->image_ptr = NULL;
    }

    if (NULL != clip_ptr->wlmtk_dock_ptr) {
        wlmtk_layer_remove_panel(
            wlmtk_panel_get_layer(wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr)),
            wlmtk_dock_panel(clip_ptr->wlmtk_dock_ptr));

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

    free(clip_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Typecast: Retrieves the `wlmaker_clip_t` for the given |view_ptr|.
 *
 * @param view_ptr
 *
 * @return A pointer to the `wlmaker_clip_t` holding |view_ptr|.
 */
wlmaker_clip_t *clip_from_view(wlmaker_view_t *view_ptr)
{
    BS_ASSERT(view_ptr->impl_ptr == &clip_view_impl);
    return BS_CONTAINER_OF(view_ptr, wlmaker_clip_t, view);
}

/* ------------------------------------------------------------------------- */
/**
 * Gets the size of the clip in pixels.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
void clip_get_size(__UNUSED__ wlmaker_view_t *view_ptr,
                   uint32_t *width_ptr,
                   uint32_t *height_ptr)
{
    if (NULL != width_ptr) *width_ptr = 64;
    if (NULL != height_ptr) *height_ptr = 64;
}

/* ------------------------------------------------------------------------- */
/**
 * Draws workspace details onto the |cairo_ptr|.
 *
 * @param cairo_ptr
 * @param num                 Number (index) of the workspace.
 * @param name_ptr            Name of the workspace.
 */
void draw_workspace(cairo_t *cairo_ptr, int num, const char *name_ptr)
{
    cairo_save(cairo_ptr);

    cairo_select_font_face(
        cairo_ptr, "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 12.0);
    cairo_set_source_argb8888(cairo_ptr, 0xff000000);
    cairo_move_to(cairo_ptr, 4, 14);
    cairo_show_text(cairo_ptr, name_ptr);

    cairo_move_to(cairo_ptr, 50, 56);
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", num);
    cairo_show_text(cairo_ptr, buf);

    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Called when the "previous" button is clicked on the clip.
 *
 * @param interactive_ptr
 * @param data_ptr            points to the `wlmaker_clip_t`.
 */
void callback_prev(__UNUSED__ wlmaker_interactive_t *interactive_ptr,
                   void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = (wlmaker_clip_t*)data_ptr;

    wlmaker_server_switch_to_previous_workspace(clip_ptr->server_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Called when the "next" button is clicked on the clip.
 *
 * @param interactive_ptr
 * @param data_ptr            points to the `wlmaker_clip_t`.
 */
void callback_next(__UNUSED__ wlmaker_interactive_t *interactive_ptr,
                   void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = (wlmaker_clip_t*)data_ptr;

    wlmaker_server_switch_to_next_workspace(clip_ptr->server_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `workspace_changed` signal of `wlmaker_server_t`.
 *
 * Will redraw the clip contents with the current workspace, and re-map the
 * clip to the new workspace.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the new `wlmaker_workspace_t`.
 */
void handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_clip_t *clip_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_clip_t, workspace_changed_listener);
    wlmaker_workspace_t *workspace_ptr = data_ptr;

    // TODO(kaeser@gubbe.ch): Should be part of that code cleanup...
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    BS_ASSERT(NULL != wlr_buffer_ptr);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    wlmaker_decorations_draw_clip(
        cairo_ptr, &wlmaker_config_theme.tile_fill, false);
    int index = 0;
    const char *name_ptr = NULL;
    wlmaker_workspace_get_details(workspace_ptr, &index, &name_ptr);
    draw_workspace(cairo_ptr, index, name_ptr);
    cairo_destroy(cairo_ptr);

    if (NULL != clip_ptr->tile_wlr_buffer_ptr) {
        wlr_buffer_drop(clip_ptr->tile_wlr_buffer_ptr);
    }
    clip_ptr->tile_wlr_buffer_ptr = wlr_buffer_ptr;

    wlr_scene_buffer_set_buffer(
        clip_ptr->tile_scene_buffer_ptr,
        clip_ptr->tile_wlr_buffer_ptr);

    // TODO(kaeser@gubbe.ch): Should add a "remap" command.
    wlmaker_view_unmap(&clip_ptr->view);
    wlmaker_view_map(
        &clip_ptr->view,
        wlmaker_server_get_current_workspace(clip_ptr->server_ptr),
        WLMAKER_WORKSPACE_LAYER_TOP);
}

/* -------------------------------------------------------------------------- */
/**
 * Handles axis events: Move to previous, respectively next workspace.
 *
 * @param view_ptr
 * @param event_ptr
 */
void handle_axis(
    wlmaker_view_t *view_ptr,
    struct wlr_pointer_axis_event *event_ptr)
{
    wlmaker_clip_t *clip_ptr = clip_from_view(view_ptr);

    if (0 > event_ptr->delta_discrete) {
        // Scroll wheel "up" -> next.
        wlmaker_server_switch_to_next_workspace(clip_ptr->server_ptr);
    } else if (0 < event_ptr->delta_discrete) {
        // Scroll wheel "down" -> next.
        wlmaker_server_switch_to_previous_workspace(clip_ptr->server_ptr);
    }
}

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

    if (0 > wlr_pointer_axis_event_ptr->delta_discrete) {
        // Scroll wheel "up" -> next.
        wlmaker_server_switch_to_next_workspace(clip_ptr->server_ptr);
    } else if (0 < wlr_pointer_axis_event_ptr->delta_discrete) {
        // Scroll wheel "down" -> next.
        wlmaker_server_switch_to_previous_workspace(clip_ptr->server_ptr);
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

    bs_log(BS_ERROR, "FIXME: Button pressed for clip %p!", clip_ptr);
    return true;
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
    wlmaker_workspace_get_details(
        wlmaker_server_get_current_workspace(clip_ptr->server_ptr),
        &index, &name_ptr);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return;
    }

    cairo_select_font_face(
        cairo_ptr, "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 12.0);
    cairo_set_source_argb8888(cairo_ptr, 0xff000000);
    cairo_move_to(cairo_ptr, 4, 14);
    cairo_show_text(cairo_ptr, name_ptr);

    cairo_move_to(cairo_ptr, 50, 56);
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

/* == Unit tests =========================================================== */

static void test_draw_tile(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_clip_test_cases[] = {
    { 1, "draw_tile", test_draw_tile },
    { 0, NULL, NULL }
};

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
