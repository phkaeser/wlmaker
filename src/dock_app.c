/* ========================================================================= */
/**
 * @file dock_app.c
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

#include "dock_app.h"

#include "config.h"
#include "decorations.h"
#include "tile.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of the dock-attached application. */
struct _wlmaker_dock_app_t {
    /** Member of `attached_apps` in `wlmaker_dock_t`. */
    bs_dllist_node_t          dlnode;

    /** Back-link to the view this attached app is a member of. */
    wlmaker_view_t            *view_ptr;
    /** Configuration of the app. */
    const wlmaker_dock_app_config_t *config_ptr;

    /** Views that are running from subprocesses of this App (launcher). */
    bs_ptr_set_t              *created_views_ptr;
    /** Views that are mapped from subprocesses of this App (launcher). */
    bs_ptr_set_t              *mapped_views_ptr;

    /** Tile interactive. */
    wlmaker_interactive_t     *tile_interactive_ptr;
    /** Texture of the tile, including the configured icon. */
    struct wlr_buffer         *tile_wlr_buffer_ptr;
};

static bool draw_texture(cairo_t *cairo_ptr, const char *icon_path_ptr);
static void tile_callback(
    wlmaker_interactive_t *interactive_ptr,
    void *data_ptr);
static void handle_terminated(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int state,
    int code);

static void handle_view_created(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr);
static void handle_view_mapped(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr);
static void handle_view_unmapped(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr);
static void handle_view_destroyed(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_dock_app_t *wlmaker_dock_app_create(
    wlmaker_view_t *view_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    int x, int y,
    wlmaker_dock_app_config_t *dock_app_config_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = logged_calloc(
        1, sizeof(wlmaker_dock_app_t));
    if (NULL == dock_app_ptr) return NULL;
    dock_app_ptr->config_ptr = dock_app_config_ptr;
    dock_app_ptr->view_ptr = view_ptr;

    dock_app_ptr->created_views_ptr = bs_ptr_set_create();
    if (NULL == dock_app_ptr->created_views_ptr) {
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }
    dock_app_ptr->mapped_views_ptr = bs_ptr_set_create();
    if (NULL == dock_app_ptr->mapped_views_ptr) {
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }

    dock_app_ptr->tile_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    if (NULL == dock_app_ptr->tile_wlr_buffer_ptr) {
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(
        dock_app_ptr->tile_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }
    bool drawn = draw_texture(cairo_ptr, dock_app_config_ptr->icon_path_ptr);
    cairo_destroy(cairo_ptr);
    if (!drawn) {
        bs_log(BS_ERROR, "Failed draw_texture().");
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }

    struct wlr_scene_buffer *buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, dock_app_ptr->tile_wlr_buffer_ptr);
    if (NULL == buffer_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_buffer_create()");
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }
    buffer_ptr->node.data = view_ptr;

    dock_app_ptr->tile_interactive_ptr = wlmaker_tile_create(
        buffer_ptr,
        view_ptr->server_ptr->cursor_ptr,
        tile_callback,
        dock_app_ptr,
        dock_app_ptr->tile_wlr_buffer_ptr);
    wlr_scene_node_set_position(&buffer_ptr->node, x, y);
    wlr_scene_node_set_enabled(&buffer_ptr->node, true);
    if (NULL == dock_app_ptr->tile_interactive_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_tile_create.");
        wlmaker_dock_app_destroy(dock_app_ptr);
        return NULL;
    }
    bs_avltree_insert(
        view_ptr->interactive_tree_ptr,
        &buffer_ptr->node,
        &dock_app_ptr->tile_interactive_ptr->avlnode,
        false);

    return dock_app_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_dock_app_destroy(wlmaker_dock_app_t *dock_app_ptr)
{
    if (NULL != dock_app_ptr->tile_interactive_ptr) {
        // Attempt to remove the node from the tree. OK if it's not found.
        bs_avltree_delete(
            dock_app_ptr->view_ptr->interactive_tree_ptr,
            &dock_app_ptr->tile_interactive_ptr->wlr_scene_buffer_ptr->node);
        // And call the interactive's dtor.
        wlmaker_interactive_node_destroy(
            &dock_app_ptr->tile_interactive_ptr->avlnode);
    }

    if (NULL != dock_app_ptr->tile_wlr_buffer_ptr) {
        wlr_buffer_drop(dock_app_ptr->tile_wlr_buffer_ptr);
        dock_app_ptr->tile_wlr_buffer_ptr = NULL;
    }

    if (NULL != dock_app_ptr->mapped_views_ptr) {
        bs_ptr_set_destroy(dock_app_ptr->mapped_views_ptr);
        dock_app_ptr->mapped_views_ptr = NULL;
    }
    if (NULL != dock_app_ptr->created_views_ptr) {
        bs_ptr_set_destroy(dock_app_ptr->created_views_ptr);
        dock_app_ptr->created_views_ptr = NULL;
    }
    free(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_dock_app_t *wlmaker_dock_app_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmaker_dock_app_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_dock_app(
    wlmaker_dock_app_t *dock_app_ptr)
{
    return &dock_app_ptr->dlnode;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Redraws the tile and shows app status ("running", "created").
 *
 * @param dock_app_ptr
 */
void redraw_tile(wlmaker_dock_app_t *dock_app_ptr)
{
    const char *status_ptr = NULL;
    if (!bs_ptr_set_empty(dock_app_ptr->mapped_views_ptr)) {
        status_ptr = "Running";
    } else if (!bs_ptr_set_empty(dock_app_ptr->created_views_ptr)) {
        status_ptr = "Started";
    }

    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    BS_ASSERT(NULL != wlr_buffer_ptr);
    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    BS_ASSERT(NULL != cairo_ptr);
    bool rv = draw_texture(cairo_ptr, dock_app_ptr->config_ptr->icon_path_ptr);
    BS_ASSERT(rv);

    if (NULL != status_ptr) {
        float r, g, b, alpha;
        bs_gfxbuf_argb8888_to_floats(0xff12905a, &r, &g, &b, &alpha);
        cairo_pattern_t *cairo_pattern_ptr = cairo_pattern_create_rgba(
            r, g, b, alpha);
        cairo_set_source(cairo_ptr, cairo_pattern_ptr);
        cairo_pattern_destroy(cairo_pattern_ptr);
        cairo_rectangle(cairo_ptr, 0, 52, 64, 12);
        cairo_fill(cairo_ptr);
        cairo_stroke(cairo_ptr);

        cairo_select_font_face(cairo_ptr, "Helvetica",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cairo_ptr, 10.0);
        cairo_set_source_argb8888(cairo_ptr, 0xffffffff);
        cairo_move_to(cairo_ptr, 4, 62);
        cairo_show_text(cairo_ptr, status_ptr);
    }
    cairo_destroy(cairo_ptr);

    wlmaker_tile_set_texture(
        dock_app_ptr->tile_interactive_ptr, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Draws the tile background and icon as texture into |cairo_ptr|.
 *
 * @param cairo_ptr
 * @param icon_path_ptr
 *
 * @return true on success.
 */
bool draw_texture(cairo_t *cairo_ptr, const char *icon_path_ptr)
{
    wlmaker_decorations_draw_tile(
        cairo_ptr, &wlmaker_config_theme.tile_fill, false);
    return wlmaker_decorations_draw_tile_icon(cairo_ptr, icon_path_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for when the tile is triggered (clicked).
 *
 * Not implemented yet, supposed to launch the configured application.
 *
 * @param interactive_ptr
 * @param data_ptr
 */
void tile_callback(
    wlmaker_interactive_t *interactive_ptr,
    void *data_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = (wlmaker_dock_app_t*)data_ptr;

    BS_ASSERT(dock_app_ptr->tile_interactive_ptr == interactive_ptr);

    bs_subprocess_t *subprocess_ptr = bs_subprocess_create_cmdline(
        dock_app_ptr->config_ptr->cmdline_ptr);
    if (NULL == subprocess_ptr) {
        bs_log(BS_ERROR, "Failed bs_subprocess_create_cmdline(%s)",
               dock_app_ptr->config_ptr->cmdline_ptr);
        return;
    }

    if (!bs_subprocess_start(subprocess_ptr)) {
        bs_log(BS_ERROR, "Failed bs_subprocess_start for %s",
               dock_app_ptr->config_ptr->cmdline_ptr);
        bs_subprocess_destroy(subprocess_ptr);
        return;
    }

    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr;
    subprocess_handle_ptr = wlmaker_subprocess_monitor_entrust(
        dock_app_ptr->view_ptr->server_ptr->monitor_ptr,
        subprocess_ptr,
        handle_terminated,
        dock_app_ptr,
        handle_view_created,
        handle_view_mapped,
        handle_view_unmapped,
        handle_view_destroyed);

    // TODO(kaeser@gubbe.ch): Store the handle, as this is useful for showing
    // error status and permitting to kill the subprocess.
    // Note: There may be more than 1 subprocess for the launcher (possibly
    // depending on configuration.
}

/* ------------------------------------------------------------------------- */
/**
 * Callback handler for when the registered subprocess terminates.
 *
 * @param userdata_ptr        Points to @ref wlmaker_dock_app_t.
 * @param subprocess_handle_ptr
 * @param exit_status
 * @param signal_number
 */
void handle_terminated(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int exit_status,
    int signal_number)
{
    wlmaker_dock_app_t *dock_app_ptr = userdata_ptr;
    const char *format_ptr;
    int code;

    if (0 == signal_number) {
        format_ptr = "App '%s' (%p) terminated, status code %d.";
        code = exit_status;
    } else {
        format_ptr = "App '%s' (%p) killed by signal %d.";
        code = signal_number;
    }



    bs_log(BS_INFO, format_ptr,
           dock_app_ptr->config_ptr->app_id_ptr,
           dock_app_ptr,
           code);
    // TODO(kaeser@gubbe.ch): Keep exit status and latest output available
    // for visualization.
    wlmaker_subprocess_monitor_cede(
        dock_app_ptr->view_ptr->server_ptr->monitor_ptr,
        subprocess_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a view from the launched subprocess is created.
 *
 * Registers the view as "created", and will then redraw the launcher tile
 * to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_dock_app_t.
 * @param subprocess_handle_ptr
 * @param view_ptr
 */
void handle_view_created(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = userdata_ptr;

    bool rv = bs_ptr_set_insert(dock_app_ptr->created_views_ptr, view_ptr);
    if (!rv) bs_log(BS_ERROR, "Failed bs_ptr_set_insert(%p)", view_ptr);

    redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a view from the launched subprocess is mapped.
 *
 * Registers the view as "mapped", and will then redraw the launcher tile
 * to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_dock_app_t.
 * @param subprocess_handle_ptr
 * @param view_ptr
 */
void handle_view_mapped(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = userdata_ptr;

    // TODO(kaeser@gubbe.ch): Appears we do encounter this scenario. File this
    // as a bug and fix it.
    // BS_ASSERT(bs_ptr_set_contains(dock_app_ptr->created_views_ptr, view_ptr));

    bool rv = bs_ptr_set_insert(dock_app_ptr->mapped_views_ptr, view_ptr);
    if (!rv) bs_log(BS_ERROR, "Failed bs_ptr_set_insert(%p)", view_ptr);

    redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a view from the launched subprocess is unmapped.
 *
 * Removes the view from the set of "mapped" views, , and will then redraw the
 * launcher tile to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_dock_app_t.
 * @param subprocess_handle_ptr
 * @param view_ptr
 */
void handle_view_unmapped(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = userdata_ptr;

    bs_ptr_set_erase(dock_app_ptr->mapped_views_ptr, view_ptr);

    redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a view from the launched subprocess is destroyed.
 *
 * Removes the view from the set of "created" views, , and will then redraw the
 * launcher tile to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_dock_app_t.
 * @param subprocess_handle_ptr
 * @param view_ptr
 */
void handle_view_destroyed(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmaker_view_t *view_ptr)
{
    wlmaker_dock_app_t *dock_app_ptr = userdata_ptr;

    bs_ptr_set_erase(dock_app_ptr->created_views_ptr, view_ptr);

    redraw_tile(dock_app_ptr);
}

/* == End of dock_app.c ==================================================== */
