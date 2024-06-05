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

#include <wlr/util/edges.h>
#include <toolkit/toolkit.h>

#include "config.h"
#include "dock_app.h"
#include "launcher.h"
#include "view.h"
#include "default_dock_state.h"

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

    /** Toolkit dock. */
    wlmtk_dock_t              *wlmtk_dock_ptr;
};

static wlmaker_dock_t *dock_from_view(wlmaker_view_t *view_ptr);
static void dock_get_size(wlmaker_view_t *view_ptr,
                          uint32_t *width_ptr,
                          uint32_t *height_ptr);
static void handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmaker_dock_decode_launchers(
    wlmcfg_object_t *object_ptr,
    void *dest_ptr);

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

/** TODO: Replace this. */
typedef struct {
    /** Positioning data. */
    wlmtk_dock_positioning_t  positioning;
    /** Launchers. */
    wlmcfg_array_t            *launchers_array_ptr;
} parse_args;

/** Enum descriptor for `enum wlr_edges`. */
static const wlmcfg_enum_desc_t _wlmaker_dock_edges[] = {
    WLMCFG_ENUM("TOP", WLR_EDGE_TOP),
    WLMCFG_ENUM("BOTTOM", WLR_EDGE_BOTTOM),
    WLMCFG_ENUM("LEFT", WLR_EDGE_LEFT),
    WLMCFG_ENUM("RIGHT", WLR_EDGE_RIGHT),
    WLMCFG_ENUM_SENTINEL(),
};

/** Descriptor for the dock's plist. */
const wlmcfg_desc_t _wlmaker_dock_desc[] = {
    WLMCFG_DESC_ENUM("Edge", true, parse_args, positioning.edge,
                     WLR_EDGE_NONE, _wlmaker_dock_edges),
    WLMCFG_DESC_ENUM("Anchor", true, parse_args, positioning.anchor,
                     WLR_EDGE_NONE, _wlmaker_dock_edges),
    WLMCFG_DESC_CUSTOM("Launchers", true, parse_args, launchers_array_ptr,
                       _wlmaker_dock_decode_launchers, NULL, NULL),
    WLMCFG_DESC_SENTINEL(),
};



/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_dock_t *wlmaker_dock_create(
    wlmaker_server_t *server_ptr,
    const wlmaker_config_style_t *style_ptr)
{
    wlmaker_dock_t *dock_ptr = logged_calloc(1, sizeof(wlmaker_dock_t));
    if (NULL == dock_ptr) return NULL;
    dock_ptr->server_ptr = server_ptr;

    if (true) {

        parse_args args = {};
        wlmcfg_object_t *object_ptr = wlmcfg_create_object_from_plist_data(
            embedded_binary_default_dock_state_data,
            embedded_binary_default_dock_state_size);
        BS_ASSERT(NULL != object_ptr);
        wlmcfg_decode_dict(
            wlmcfg_dict_from_object(object_ptr),
            _wlmaker_dock_desc,
            &args);

        dock_ptr->wlmtk_dock_ptr = wlmtk_dock_create(
            &args.positioning, &style_ptr->dock, server_ptr->env_ptr);
        if (NULL == dock_ptr->wlmtk_dock_ptr) {
            wlmaker_dock_destroy(dock_ptr);
            return NULL;
        }
        wlmtk_element_set_visible(
            wlmtk_dock_element(dock_ptr->wlmtk_dock_ptr),
            true);

        wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
            wlmaker_server_get_current_workspace(server_ptr));
        wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
            wlmtk_workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);
        wlmtk_layer_add_panel(
            layer_ptr,
            wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));

        wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create(
            server_ptr, &style_ptr->tile);
        wlmtk_dock_add_tile(
            dock_ptr->wlmtk_dock_ptr,
            wlmaker_launcher_tile(launcher_ptr));

        launcher_ptr = wlmaker_launcher_create(
            server_ptr, &style_ptr->tile);
        wlmtk_dock_add_tile(
            dock_ptr->wlmtk_dock_ptr,
            wlmaker_launcher_tile(launcher_ptr));
    }

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

    wlmtk_util_connect_listener_signal(
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

    if (NULL != dock_ptr->wlmtk_dock_ptr) {

        wlmtk_layer_t *layer_ptr = wlmtk_panel_get_layer(
            wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));
        if (NULL != layer_ptr) {
            wlmtk_layer_remove_panel(
                layer_ptr,
                wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));
        }

        wlmtk_dock_destroy(dock_ptr->wlmtk_dock_ptr);
        dock_ptr->wlmtk_dock_ptr = NULL;
    }

    free(dock_ptr);
}

/* ------------------------------------------------------------------------- */
/** Decoder for the "Launchers" array. Currently just stores a reference. */
bool _wlmaker_dock_decode_launchers(
    wlmcfg_object_t *object_ptr,
    void *dest_ptr)
{
    wlmcfg_array_t **array_ptr_ptr = dest_ptr;

    *array_ptr_ptr = wlmcfg_array_from_object(object_ptr);
    if (NULL == *array_ptr_ptr) return false;

    wlmcfg_object_ref(wlmcfg_object_from_array(*array_ptr_ptr));
    return true;
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
    wlmaker_dock_t *dock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_dock_t, workspace_changed_listener);

    // TODO(kaeser@gubbe.ch): Should add a "remap" command.
    wlmaker_view_unmap(&dock_ptr->view);
    wlmaker_view_map(
        &dock_ptr->view,
        wlmaker_server_get_current_workspace(dock_ptr->server_ptr),
        WLMAKER_WORKSPACE_LAYER_TOP);
}

/* == End of dock.c ======================================================== */
