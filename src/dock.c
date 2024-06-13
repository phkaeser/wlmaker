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
#include "launcher.h"
#include "default_dock_state.h"

/* == Declarations ========================================================= */

/** Dock handle. */
struct _wlmaker_dock_t {
    /** Toolkit dock. */
    wlmtk_dock_t              *wlmtk_dock_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
};

static bool _wlmaker_dock_decode_launchers(
    wlmcfg_object_t *object_ptr,
    void *dest_ptr);

/* == Data ================================================================= */

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

    wlmtk_workspace_t *wlmtk_workspace_ptr =
        wlmaker_server_get_current_wlmtk_workspace(server_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        wlmtk_workspace_ptr, WLMTK_WORKSPACE_LAYER_TOP);
    wlmtk_layer_add_panel(
        layer_ptr,
        wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));

    for (size_t i = 0;
         i < wlmcfg_array_size(args.launchers_array_ptr);
         ++i) {
        wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
            wlmcfg_array_at(args.launchers_array_ptr, i));
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "Elements of 'Launchers' must be dicts.");
            wlmaker_dock_destroy(dock_ptr);
            return NULL;
        }
        bs_log(BS_ERROR, "FIXME: Created launcher!");
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
    wlmcfg_object_unref(object_ptr);
    if (NULL != args.launchers_array_ptr) {
        wlmcfg_array_unref(args.launchers_array_ptr);
        args.launchers_array_ptr = NULL;
    }

#if 0
    // FIXME: Changing workspaces needs to be (re)fixed.
    wlmtk_util_connect_listener_signal(
        &server_ptr->workspace_changed,
        &dock_ptr->workspace_changed_listener,
        handle_workspace_changed);
#endif

    bs_log(BS_INFO, "Created dock %p", dock_ptr);
    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_dock_destroy(wlmaker_dock_t *dock_ptr)
{
    if (NULL != dock_ptr->wlmtk_dock_ptr) {
        wlmtk_layer_remove_panel(
            wlmtk_panel_get_layer(wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr)),
            wlmtk_dock_panel(dock_ptr->wlmtk_dock_ptr));

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
    wlmtk_fake_workspace_t *fw_ptr = BS_ASSERT_NOTNULL(
        wlmtk_fake_workspace_create(1024, 768));
    wlmaker_server_t server = { .fake_wlmtk_workspace_ptr = fw_ptr };
    wlmaker_config_style_t style = {};

    wlmaker_dock_t *dock_ptr = wlmaker_dock_create(&server, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dock_ptr);

    wlmaker_dock_destroy(dock_ptr);
    wlmtk_fake_workspace_destroy(fw_ptr);
}

/* == End of dock.c ======================================================== */
