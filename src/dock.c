/* ========================================================================= */
/**
 * @file dock.c
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

#include "dock.h"

#include "config.h"
#include "dock_app.h"
#include "util.h"
#include "view.h"

/* == Declarations ========================================================= */

/** Dock handle. */
struct _wlmaker_dock_t {
    /** Corresponding view. */
    wlmaker_view_t            view;
    /** Backlink to the server. */
    wlmaker_server_t          *server_ptr;

    /** Scene graph subtree holding all layers of the dock. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Attached applications. */
    bs_dllist_t               attached_apps;

    /** Listener for the `workspace_changed` signal by `wlmaker_server_t`. */
    struct wl_listener        workspace_changed_listener;
};

static wlmaker_dock_t *dock_from_view(wlmaker_view_t *view_ptr);
static void dock_get_size(wlmaker_view_t *view_ptr,
                          uint32_t *width_ptr,
                          uint32_t *height_ptr);
static void handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     dock_view_impl = {
    .set_activated = NULL,
    .get_size = dock_get_size
};

/** Hard-coded: Applications attached to the dock. */
wlmaker_dock_app_config_t app_configs[] = {
    {
        .app_id_ptr = "chrome",
        .cmdline_ptr = "/usr/bin/google-chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --user-data-dir=/tmp/chrome-wayland",
        .icon_path_ptr = "chrome-48x48.png"
    }, {
        .app_id_ptr = "foot",
        .cmdline_ptr = "/usr/bin/foot",
        .icon_path_ptr = "terminal-48x48.png"
    }, {
        .app_id_ptr = "firefox",
        .cmdline_ptr = "MOZ_ENABLE_WAYLAND=1 /usr/bin/firefox",
        .icon_path_ptr = "firefox-48x48.png"
    }, {
        .app_id_ptr = NULL,  // Sentinel.
        .cmdline_ptr = NULL,
        .icon_path_ptr = NULL
    }
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_dock_t *wlmaker_dock_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_dock_t *dock_ptr = logged_calloc(1, sizeof(wlmaker_dock_t));
    if (NULL == dock_ptr) return NULL;
    dock_ptr->server_ptr = server_ptr;

    dock_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        &server_ptr->void_wlr_scene_ptr->tree);
    if (NULL == dock_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_tree_create()");
        wlmaker_dock_destroy(dock_ptr);
        return NULL;
    }

    wlmaker_view_init(
        &dock_ptr->view,
        &dock_view_impl,
        server_ptr,
        NULL,  // wlr_surface_ptr.
        dock_ptr->wlr_scene_tree_ptr,
        NULL);  // send_close_callback
    wlmaker_view_set_title(&dock_ptr->view, "WLMaker Dock");

    dock_ptr->view.anchor = WLMAKER_VIEW_ANCHOR_TOP|WLMAKER_VIEW_ANCHOR_RIGHT;

    int pos = 0;
    for (wlmaker_dock_app_config_t *dock_app_config_ptr = &app_configs[0];
         dock_app_config_ptr->app_id_ptr != NULL;
         dock_app_config_ptr++, pos += 64) {
        wlmaker_dock_app_t *dock_app_ptr = wlmaker_dock_app_create(
            &dock_ptr->view,
            dock_ptr->wlr_scene_tree_ptr,
            0, pos,
            dock_app_config_ptr);
        if (NULL == dock_app_ptr) {
            bs_log(BS_ERROR, "Failed wlmaker_dock_app_create() for %s at %d.",
                   dock_app_config_ptr->app_id_ptr, pos);
        } else {
            bs_dllist_push_back(
                &dock_ptr->attached_apps,
                wlmaker_dlnode_from_dock_app(dock_app_ptr));
        }
    }

    wlm_util_connect_listener_signal(
        &server_ptr->workspace_changed,
        &dock_ptr->workspace_changed_listener,
        handle_workspace_changed);

    wlmaker_view_map(
        &dock_ptr->view,
        wlmaker_server_get_current_workspace(dock_ptr->server_ptr),
        WLMAKER_WORKSPACE_LAYER_TOP);
    bs_log(BS_INFO, "Created dock view %p", &dock_ptr->view);
    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_dock_destroy(wlmaker_dock_t *dock_ptr)
{
    wl_list_remove(&dock_ptr->workspace_changed_listener.link);

    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = bs_dllist_pop_front(
                        &dock_ptr->attached_apps))) {
        wlmaker_dock_app_destroy(wlmaker_dock_app_from_dlnode(dlnode_ptr));
    }

    wlmaker_view_fini(&dock_ptr->view);

    free(dock_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Typecast: Retrieves the `wlmaker_dock_t` for the given |view_ptr|.
 *
 * @param view_ptr
 *
 * @return A pointer to the `wlmaker_dock_t` holding |view_ptr|.
 */
wlmaker_dock_t *dock_from_view(wlmaker_view_t *view_ptr)
{
    BS_ASSERT(view_ptr->impl_ptr == &dock_view_impl);
    return BS_CONTAINER_OF(view_ptr, wlmaker_dock_t, view);
}


/* ------------------------------------------------------------------------- */
/**
 * Gets the size of the dock in pixels.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
void dock_get_size(wlmaker_view_t *view_ptr,
                   uint32_t *width_ptr,
                   uint32_t *height_ptr)
{
    wlmaker_dock_t *dock_ptr = dock_from_view(view_ptr);
    if (NULL != width_ptr) *width_ptr = 64;
    if (NULL != height_ptr) {
        *height_ptr = bs_dllist_size(&dock_ptr->attached_apps) * 64;
    }
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
    __UNUSED__ void *data_ptr)
{
    wlmaker_dock_t *dock_ptr = wl_container_of(
        listener_ptr, dock_ptr, workspace_changed_listener);

    // TODO(kaeser@gubbe.ch): Should add a "remap" command.
    wlmaker_view_unmap(&dock_ptr->view);
    wlmaker_view_map(
        &dock_ptr->view,
        wlmaker_server_get_current_workspace(dock_ptr->server_ptr),
        WLMAKER_WORKSPACE_LAYER_TOP);
}

/* == End of dock.c ======================================================== */
