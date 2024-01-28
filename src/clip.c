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

#include "button.h"
#include "config.h"
#include "decorations.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Clip handle. */
struct _wlmaker_clip_t {
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

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     clip_view_impl = {
    .set_activated = NULL,
    .get_size = clip_get_size,
    .handle_axis = handle_axis
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_clip_t *wlmaker_clip_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_clip_t *clip_ptr = logged_calloc(1, sizeof(wlmaker_clip_t));
    if (NULL == clip_ptr) return NULL;
    clip_ptr->server_ptr = server_ptr;

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
    wlmaker_clip_t *clip_ptr = wl_container_of(
        listener_ptr, clip_ptr, workspace_changed_listener);
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

/* == End of clip.c ======================================================== */
