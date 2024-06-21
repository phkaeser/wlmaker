/* ========================================================================= */
/**
 * @file task_list.c
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

/// for PATH_MAX.
#define _POSIX_C_SOURCE 1

#include "task_list.h"

#include "config.h"
#include "toolkit/toolkit.h"

#include <libbase/libbase.h>
#include <limits.h>

/// Include unstable interfaces of wlroots.
#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the task list. */
struct _wlmaker_task_list_t {
    /** Derived from a toolkit panel. */
    wlmtk_panel_t             super_panel;

    /** Buffer that shows the tasklist's content. */
    wlmtk_buffer_t            buffer;

    /** Backlink to the server. */
    wlmaker_server_t          *server_ptr;

    /** Listener for the `task_list_enabled` signal by `wlmaker_server_t`. */
    struct wl_listener        task_list_enabled_listener;
    /** Listener for the `task_list_disabled` signal by `wlmaker_server_t`. */
    struct wl_listener        task_list_disabled_listener;

    /** Listener for `window_mapped_event` signal by `wlmaker_server_t`. */
    struct wl_listener        window_mapped_listener;
    /** Listener for `window_unmapped_event` signal by `wlmaker_server_t`. */
    struct wl_listener        window_unmapped_listener;

    /** Whether the task list is currently enabled (mapped). */
    bool                      enabled;
};

/** Width of the task list overlay. */
static const uint32_t         task_list_width = 400;
/** Height of the task list overlay. */
static const uint32_t         task_list_height = 200;

static void task_list_refresh(
    wlmaker_task_list_t *task_list_ptr);
static struct wlr_buffer *create_wlr_buffer(
    wlmaker_workspace_t *workspace_ptr);
static void draw_into_cairo(
    cairo_t *cairo_ptr,
    wlmaker_workspace_t *workspace_ptr);
static void draw_window_into_cairo(
    cairo_t *cairo_ptr,
    wlmtk_window_t *window_ptr,
    bool active,
    int pos_y);
static const char *window_name(wlmtk_window_t *window_ptr);

static uint32_t _wlmaker_task_list_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

static void handle_task_list_enabled(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_task_list_disabled(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_window_mapped(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_window_unmapped(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Task list positioning: Fixed dimensions, at center of layer. */
static const wlmtk_panel_positioning_t _wlmaker_task_list_positioning = {
    .desired_width = 400,
    .desired_height = 200,
    .anchor = WLR_EDGE_BOTTOM | WLR_EDGE_TOP | WLR_EDGE_LEFT | WLR_EDGE_RIGHT
};

/** Virtual method table for the task list. */
static const wlmtk_panel_vmt_t _wlmaker_task_list_vmt = {
    .request_size = _wlmaker_task_list_request_size
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_task_list_t *wlmaker_task_list_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_task_list_t *task_list_ptr = logged_calloc(
        1, sizeof(wlmaker_task_list_t));
    task_list_ptr->server_ptr = server_ptr;

    if (!wlmtk_panel_init(&task_list_ptr->super_panel,
                          &_wlmaker_task_list_positioning,
                          server_ptr->env_ptr)) {
        wlmaker_task_list_destroy(task_list_ptr);
        return NULL;
    }
    wlmtk_panel_extend(&task_list_ptr->super_panel,
                       &_wlmaker_task_list_vmt);
    wlmtk_element_set_visible(
        wlmtk_panel_element(&task_list_ptr->super_panel), true);

    if (!wlmtk_buffer_init(&task_list_ptr->buffer, server_ptr->env_ptr)) {
        wlmaker_task_list_destroy(task_list_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_buffer_element(&task_list_ptr->buffer), true);
    wlmtk_container_add_element(
        &task_list_ptr->super_panel.super_container,
        wlmtk_buffer_element(&task_list_ptr->buffer));

    wlmtk_util_connect_listener_signal(
        &server_ptr->task_list_enabled_event,
        &task_list_ptr->task_list_enabled_listener,
        handle_task_list_enabled);
    wlmtk_util_connect_listener_signal(
        &server_ptr->task_list_disabled_event,
        &task_list_ptr->task_list_disabled_listener,
        handle_task_list_disabled);

    wlmtk_util_connect_listener_signal(
        &server_ptr->window_mapped_event,
        &task_list_ptr->window_mapped_listener,
        handle_window_mapped);
    wlmtk_util_connect_listener_signal(
        &server_ptr->window_unmapped_event,
        &task_list_ptr->window_unmapped_listener,
        handle_window_unmapped);

    return task_list_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_task_list_destroy(wlmaker_task_list_t *task_list_ptr)
{
    wl_list_remove(&task_list_ptr->window_unmapped_listener.link);
    wl_list_remove(&task_list_ptr->window_mapped_listener.link);
    wl_list_remove(&task_list_ptr->task_list_disabled_listener.link);
    wl_list_remove(&task_list_ptr->task_list_enabled_listener.link);

    if (wlmtk_buffer_element(&task_list_ptr->buffer)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &task_list_ptr->super_panel.super_container,
            wlmtk_buffer_element(&task_list_ptr->buffer));
    }
    wlmtk_buffer_fini(&task_list_ptr->buffer);
    wlmtk_panel_fini(&task_list_ptr->super_panel);

    free(task_list_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Refreshes the task list. Should be done whenever a list is mapped/unmapped.
 *
 * @param task_list_ptr
 */
void task_list_refresh(wlmaker_task_list_t *task_list_ptr)
{
    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        task_list_ptr->server_ptr);

    struct wlr_buffer *wlr_buffer_ptr = create_wlr_buffer(
        workspace_ptr);
    wlmtk_buffer_set(&task_list_ptr->buffer, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a `struct wlr_buffer` with windows of `workspace_ptr` drawn into.
 *
 * @param workspace_ptr
 *
 * @return A pointer to the `struct wlr_buffer` with the list of windows
 *     (tasks), or NULL on error.
 */
struct wlr_buffer *create_wlr_buffer(wlmaker_workspace_t *workspace_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        task_list_width, task_list_height);
    if (NULL == wlr_buffer_ptr) return NULL;

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }
    draw_into_cairo(cairo_ptr, workspace_ptr);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Draws all tasks of `workspace_ptr` into `cairo_ptr`.
 *
 * @param cairo_ptr
 * @param workspace_ptr
 */
void draw_into_cairo(cairo_t *cairo_ptr, wlmaker_workspace_t *workspace_ptr)
{
    wlmaker_primitives_cairo_fill(
        cairo_ptr, &wlmaker_config_theme.task_list_fill);

    // Not tied to a workspace? We're done, all set.
    if (NULL == workspace_ptr) return;

    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        workspace_ptr);

    const bs_dllist_t *windows_ptr = wlmtk_workspace_get_windows_dllist(
        wlmtk_workspace_ptr);
    // No windows at all? Done here.
    if (bs_dllist_empty(windows_ptr)) return;

    // Find node of the active window, for centering the task list.
    bs_dllist_node_t *centered_dlnode_ptr = windows_ptr->head_ptr;
    bs_dllist_node_t *active_dlnode_ptr = windows_ptr->head_ptr;
    while (NULL != active_dlnode_ptr &&
           wlmtk_workspace_get_activated_window(wlmtk_workspace_ptr) !=
           wlmtk_window_from_dlnode(active_dlnode_ptr)) {
        active_dlnode_ptr = active_dlnode_ptr->next_ptr;
    }
    if (NULL != active_dlnode_ptr) centered_dlnode_ptr = active_dlnode_ptr;

    int pos_y = task_list_height / 2 + 10;
    draw_window_into_cairo(
        cairo_ptr,
        wlmtk_window_from_dlnode(centered_dlnode_ptr),
        centered_dlnode_ptr == active_dlnode_ptr,
        pos_y);

    bs_dllist_node_t *dlnode_ptr = centered_dlnode_ptr->prev_ptr;
    for (int further_windows = 1;
         NULL != dlnode_ptr && further_windows <= 3;
         dlnode_ptr = dlnode_ptr->prev_ptr, ++further_windows) {
        draw_window_into_cairo(
            cairo_ptr,
            wlmtk_window_from_dlnode(dlnode_ptr),
            false,
            pos_y - further_windows * 26);
    }

    dlnode_ptr = centered_dlnode_ptr->next_ptr;
    for (int further_windows = 1;
         NULL != dlnode_ptr && further_windows <= 3;
         dlnode_ptr = dlnode_ptr->next_ptr, ++further_windows) {
        draw_window_into_cairo(
            cairo_ptr,
            wlmtk_window_from_dlnode(dlnode_ptr),
            false,
            pos_y + further_windows * 26);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Draws one window (task) into `cairo_ptr`.
 *
 * @param cairo_ptr
 * @param window_ptr
 * @param active              Whether this window is currently active.
 * @param pos_y               Y position within the `cairo_ptr`.
 */
void draw_window_into_cairo(
    cairo_t *cairo_ptr,
    wlmtk_window_t *window_ptr,
    bool active,
    int pos_y)
{
    cairo_set_source_argb8888(
        cairo_ptr,
        wlmaker_config_theme.task_list_text_color);
    cairo_set_font_size(cairo_ptr, 16.0);
    cairo_select_font_face(
        cairo_ptr,
        "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        active ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_move_to(cairo_ptr, 10, pos_y);
    cairo_show_text(cairo_ptr, window_name(window_ptr));
 }

/* ------------------------------------------------------------------------- */
/**
 * Constructs a comprehensive name for the window.
 *
 * @param window_ptr
 *
 * @return Pointer to the constructed name. This is a static buffer that does
 *     not require to be free'd, but will be re-used upon next call to
 *     window_name.
 */
const char *window_name(wlmtk_window_t *window_ptr)
{
    static char               name[256];

    size_t pos = 0;

    const char *title_ptr = wlmtk_window_get_title(window_ptr);
    if (NULL != title_ptr) {
        pos = bs_strappendf(name, sizeof(name), pos, "%s", title_ptr);
    }

    const wlmtk_util_client_t *client_ptr = wlmtk_window_get_client_ptr(
        window_ptr);
    if (NULL != client_ptr && 0 != client_ptr->pid) {
        if (0 < pos) pos = bs_strappendf(name, sizeof(name), pos, " ");
        pos = bs_strappendf(name, sizeof(name), pos, "[%"PRIdMAX,
                            (intmax_t)client_ptr->pid);
        char fname[PATH_MAX], cmdline[PATH_MAX];
        snprintf(fname, sizeof(fname), "/proc/%"PRIdMAX"/cmdline",
                 (intmax_t)client_ptr->pid);
        ssize_t read_bytes = bs_file_read_buffer(
            fname, cmdline, sizeof(cmdline));
        if (0 < read_bytes) {
            pos = bs_strappendf(name, sizeof(name), pos, ": %s", cmdline);
        }
        pos = bs_strappendf(name, sizeof(name), pos, "]");
    }

    if (0 < pos) pos = bs_strappendf(name, sizeof(name), pos, " ");
    pos = bs_strappendf(name, sizeof(name), pos, "(%p)", window_ptr);
    return &name[0];
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_panel_vmt_t::request_size.
 *
 * @param panel_ptr
 * @param width
 * @param height
 *
 * @return 0 always.
 */
uint32_t _wlmaker_task_list_request_size(
    wlmtk_panel_t *panel_ptr,
    __UNUSED__ int width,
    __UNUSED__ int height)
{
    wlmtk_panel_commit(panel_ptr, 0, &_wlmaker_task_list_positioning);
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `task_list_enabled_listener`.
 *
 * Enables the task listener: Creates the task list for the currently-active
 * workspace and enables the task list on that workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_task_list_enabled(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_task_list_t *task_list_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_task_list_t, task_list_enabled_listener);

    task_list_refresh(task_list_ptr);

    if (task_list_ptr->enabled) {
        BS_ASSERT(NULL != wlmtk_panel_get_layer(&task_list_ptr->super_panel));
        return;
    }

    wlmtk_workspace_t *workspace_ptr = wlmaker_workspace_wlmtk(
        wlmaker_server_get_current_workspace(task_list_ptr->server_ptr));
    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_OVERLAY);
    wlmtk_layer_add_panel(layer_ptr, &task_list_ptr->super_panel);
    task_list_ptr->enabled = true;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `task_list_disabled_listener`: Hides the list.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_task_list_disabled(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_task_list_t *task_list_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_task_list_t, task_list_disabled_listener);

    BS_ASSERT(NULL != wlmtk_panel_get_layer(&task_list_ptr->super_panel));
    wlmtk_layer_remove_panel(
        wlmtk_panel_get_layer(&task_list_ptr->super_panel),
        &task_list_ptr->super_panel);
    task_list_ptr->enabled = false;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `window_mapped_listener`: Refreshes the list (if enabled).
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_window_mapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_task_list_t *task_list_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_task_list_t, window_mapped_listener);
    if (task_list_ptr->enabled) {
        task_list_refresh(task_list_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `window_unmapped_listener`: Refreshes the list (if enabled).
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_window_unmapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_task_list_t *task_list_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_task_list_t, window_unmapped_listener);
    if (task_list_ptr->enabled) {
        task_list_refresh(task_list_ptr);
    }
}

/* == End of task_list.c =================================================== */
