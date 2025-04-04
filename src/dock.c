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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include <wlr/util/edges.h>
#include "toolkit/toolkit.h"

#include "config.h"
#include "launcher.h"
#include "default_state.h"

/* == Declarations ========================================================= */

/** Dock handle. */
struct _wlmaker_dock_t {
    /** Toolkit dock. */
    wlmtk_dock_t              *wlmtk_dock_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

    /** Listener for @ref wlmtk_root_events_t::workspace_changed. */
    struct wl_listener        workspace_changed_listener;
};

static bool _wlmaker_dock_decode_launchers(
    bspl_object_t *object_ptr,
    void *dest_ptr);

static void _wlmaker_dock_handle_workspace_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** TODO: Replace this. */
typedef struct {
    /** Positioning data. */
    wlmtk_dock_positioning_t  positioning;
    /** Launchers. */
    bspl_array_t            *launchers_array_ptr;
} parse_args;

/** Enum descriptor for `enum wlr_edges`. */
static const bspl_enum_desc_t _wlmaker_dock_edges[] = {
    BSPL_ENUM("TOP", WLR_EDGE_TOP),
    BSPL_ENUM("BOTTOM", WLR_EDGE_BOTTOM),
    BSPL_ENUM("LEFT", WLR_EDGE_LEFT),
    BSPL_ENUM("RIGHT", WLR_EDGE_RIGHT),
    BSPL_ENUM_SENTINEL(),
};

/** Descriptor for the dock's plist. */
const bspl_desc_t _wlmaker_dock_desc[] = {
    BSPL_DESC_ENUM("Edge", true, parse_args, positioning.edge,
                     WLR_EDGE_NONE, _wlmaker_dock_edges),
    BSPL_DESC_ENUM("Anchor", true, parse_args, positioning.anchor,
                     WLR_EDGE_NONE, _wlmaker_dock_edges),
    BSPL_DESC_CUSTOM("Launchers", true, parse_args, launchers_array_ptr,
                       _wlmaker_dock_decode_launchers, NULL, NULL),
    BSPL_DESC_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_dock_t *wlmaker_dock_create(
    wlmaker_server_t *server_ptr,
    bspl_dict_t *state_dict_ptr,
    const wlmaker_config_style_t *style_ptr)
{
    wlmaker_dock_t *dock_ptr = logged_calloc(1, sizeof(wlmaker_dock_t));
    if (NULL == dock_ptr) return NULL;
    dock_ptr->server_ptr = server_ptr;

    parse_args args = {};
    bspl_dict_t *dict_ptr = bspl_dict_get_dict(state_dict_ptr, "Dock");
    if (NULL == dict_ptr) {
        bs_log(BS_ERROR, "No 'Dock' dict found in configuration.");
        wlmaker_dock_destroy(dock_ptr);
        return NULL;
    }
    bspl_decode_dict(dict_ptr, _wlmaker_dock_desc, &args);

    dock_ptr->wlmtk_dock_ptr = wlmtk_dock_create(
        &args.positioning, &style_ptr->dock, server_ptr->env_ptr);
    if (NULL == dock_ptr->wlmtk_dock_ptr) {
        wlmaker_dock_destroy(dock_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_dock_element(dock_ptr->wlmtk_dock_ptr),
        true);

    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(server_ptr->root_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);
    if (!wlmtk_layer_add_panel(
            layer_ptr,
            wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr),
            wlmbe_primary_output(
                dock_ptr->server_ptr->wlr_output_layout_ptr))) {
        wlmaker_dock_destroy(dock_ptr);
        return NULL;
    }

    for (size_t i = 0;
         i < bspl_array_size(args.launchers_array_ptr);
         ++i) {
        bspl_dict_t *dict_ptr = bspl_dict_from_object(
            bspl_array_at(args.launchers_array_ptr, i));
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "Elements of 'Launchers' must be dicts.");
            wlmaker_dock_destroy(dock_ptr);
            return NULL;
        }
        wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create_from_plist(
            &style_ptr->tile, dict_ptr,
            server_ptr->monitor_ptr, server_ptr->env_ptr);
        if (NULL == launcher_ptr) {
            wlmaker_dock_destroy(dock_ptr);
            return NULL;
        }
        wlmtk_dock_add_tile(
            dock_ptr->wlmtk_dock_ptr,
            wlmaker_launcher_tile(launcher_ptr));
    }
    // FIXME: This is leaky.
    if (NULL != args.launchers_array_ptr) {
        bspl_array_unref(args.launchers_array_ptr);
        args.launchers_array_ptr = NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlmtk_root_events(server_ptr->root_ptr)->workspace_changed,
        &dock_ptr->workspace_changed_listener,
        _wlmaker_dock_handle_workspace_changed);

    bs_log(BS_INFO, "Created dock %p", dock_ptr);
    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_dock_destroy(wlmaker_dock_t *dock_ptr)
{
    wlmtk_util_disconnect_listener(&dock_ptr->workspace_changed_listener);

    if (NULL != dock_ptr->wlmtk_dock_ptr) {
        if (NULL != wlmtk_panel_get_layer(
                wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr))) {
            wlmtk_layer_remove_panel(
                wlmtk_panel_get_layer(wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr)),
                wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));
        }

        wlmtk_dock_destroy(dock_ptr->wlmtk_dock_ptr);
        dock_ptr->wlmtk_dock_ptr = NULL;
    }

    free(dock_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Decoder for the "Launchers" array. Currently just stores a reference. */
bool _wlmaker_dock_decode_launchers(
    bspl_object_t *object_ptr,
    void *dest_ptr)
{
    bspl_array_t **array_ptr_ptr = dest_ptr;

    *array_ptr_ptr = bspl_array_from_object(object_ptr);
    if (NULL == *array_ptr_ptr) return false;

    bspl_object_ref(bspl_object_from_array(*array_ptr_ptr));
    return true;
}

/* ------------------------------------------------------------------------- */
/** Re-attaches the dock to the new "current" workspace. */
void _wlmaker_dock_handle_workspace_changed(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_dock_t *dock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_dock_t, workspace_changed_listener);
    wlmtk_panel_t *panel_ptr = wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr);

    wlmtk_layer_t *current_layer_ptr = wlmtk_panel_get_layer(panel_ptr);
    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(dock_ptr->server_ptr->root_ptr);
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
                      dock_ptr->server_ptr->wlr_output_layout_ptr)));
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_dock_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests ctor and dtor; to help fix leaks. */
void test_create_destroy(bs_test_t *test_ptr)
{
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    wlmaker_server_t server = {
        .wlr_scene_ptr = wlr_scene_ptr,
        .wl_display_ptr = wl_display_create(),
    };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &output);

    bspl_dict_t *dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_data(
            embedded_binary_default_state_data,
            embedded_binary_default_state_size));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    server.root_ptr = wlmtk_root_create(
        server.wlr_scene_ptr,
        server.wlr_output_layout_ptr,
        NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create("1", &ts, 0);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_root_add_workspace(server.root_ptr, ws_ptr);

    wlmaker_config_style_t style = {};

    wlmaker_dock_t *dock_ptr = wlmaker_dock_create(&server, dict_ptr, &style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dock_ptr);

    wlmaker_dock_destroy(dock_ptr);
    bspl_dict_unref(dict_ptr);
    wlmtk_root_remove_workspace(server.root_ptr, ws_ptr);
    wlmtk_workspace_destroy(ws_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* == End of dock.c ======================================================== */
