/* ========================================================================= */
/**
 * @file config.c
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
 *
 * Configurables for wlmaker. Currently, this file lists hardcoded entities,
 * and mainly serves as a catalog about which entities should be dynamically
 * configurable.
 */

#include "config.h"

#include <limits.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

#include "default_configuration.h"

/* == Declarations ========================================================= */

static wlmcfg_dict_t *_wlmaker_config_from_plist(const char *fname_ptr);

/* == Data ================================================================= */

/** Name of the xcursor theme. NULL picks the default. */
const char *config_xcursor_theme_name = NULL;

/** Base size for the xcursor theme (when scale==1.0). */
const uint32_t config_xcursor_theme_size = 24;

/** Overall scale of output. */
const float config_output_scale = 1.0;

/** Whether to always request server-side decorations. */
const wlmaker_config_decoration_t config_decoration =
    WLMAKER_CONFIG_DECORATION_SUGGEST_SERVER;

/** Modifiers for moving the window with the cursor. */
const uint32_t wlmaker_config_window_drag_modifiers =
    WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO;

/** Workspaces to configure. So far: Just the titles. */
const wlmaker_config_workspace_t wlmaker_config_workspaces[] = {
    { .name_ptr = "Main", .color = 0xff402020 },
    { .name_ptr = "Other", .color = 0xff182060 },
    { .name_ptr = "Last", .color = 0xff186020 },
    { .name_ptr = NULL } // sentinel.
};

/** Visual theme. */
const wlmaker_config_theme_t  wlmaker_config_theme = {
    .window_margin_color = 0xff000000,  // Pich black, opaque.
    .window_margin_width = 1,

    .tile_fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    },
    .iconified_title_fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xff404040 }}
    },
    .iconified_title_color = 0xffffffff,  // White.
    .menu_fill = {
        .type = WLMTK_STYLE_COLOR_HGRADIENT,
        .param = { .hgradient = { .from = 0xffc2c0c5, .to = 0xff828085 }}
    },
    .menu_margin_color = 0xff000000,  // Pitch black, opaque.
    .menu_margin_width = 1,
    .menu_padding_width = 1,

    .menu_item_enabled_fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0x00000000 }}  // Transparent.
    },
    .menu_item_enabled_text_color = 0xff000000,  // Black, opaque.
    .menu_item_selected_fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xffffffff }}  // White, opaque..
    },
    .menu_item_selected_text_color = 0xff000000,  // Black, opaque.

    .task_list_fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param.solid.color = 0xc0202020  // Dark grey, partly transparent.
    },
    .task_list_text_color = 0xffffffff,
};


/** Lookup paths for the configuration file. */
static const char *_wlmaker_config_fname_ptrs[] = {
    "~/.wlmaker.plist",
    NULL  // Sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmcfg_dict_t *wlmaker_config_load(const char *fname_ptr)
{
    // If a file was provided, we try onl that.
    if (NULL != fname_ptr) {
        return _wlmaker_config_from_plist(fname_ptr);
    }

    for (const char **fname_ptr_ptr = _wlmaker_config_fname_ptrs;
         *fname_ptr_ptr != NULL;
         fname_ptr_ptr++) {
        char full_path[PATH_MAX];
        char *path_ptr = bs_file_resolve_path(*fname_ptr_ptr, full_path);
        if (NULL == path_ptr) {
            bs_log(BS_INFO | BS_ERRNO, "Failed bs_file_resolve_path(%s, %p)",
                   *fname_ptr_ptr, full_path);
            continue;
        }

        // If we get here, there was a resolved item at the path. A load
        // failure indicates an issue with an existing file, and we should
        // fali here.
        bs_log(BS_INFO, "Loading configuration from \"%s\"", *fname_ptr_ptr);
        return _wlmaker_config_from_plist(*fname_ptr_ptr);
    }

    // Hardcoded configuration. Failing to load that is an error.
    bs_log(BS_INFO, "No configuration file found, using embedded default.");
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    return BS_ASSERT_NOTNULL(wlmcfg_dict_from_object(obj_ptr));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Loads a plist dict from fname_ptr. Returns NULL on error. */
wlmcfg_dict_t *_wlmaker_config_from_plist(const char *fname_ptr)
{
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_file(fname_ptr);
    if (NULL == obj_ptr) {
        bs_log(BS_ERROR, "Failed wlmcfg_create_object_from_plist(%s)",
               fname_ptr);
        return NULL;
    }

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(obj_ptr);
    if (NULL == dict_ptr) {
        bs_log(BS_ERROR, "Not a plist dict in %s", fname_ptr);
        wlmcfg_object_unref(obj_ptr);
        return NULL;
    }

    return dict_ptr;
}

/* == Unit tests =========================================================== */

static void test_embedded(bs_test_t *test_ptr);
static void test_file(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_config_test_cases[] = {
    { 1, "embedded", test_embedded },
    { 1, "file", test_file },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Verifies that the embedded config loads. */
void test_embedded(bs_test_t *test_ptr)
{
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that the (example) config file loads. */
void test_file(bs_test_t *test_ptr)
{
#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif
    wlmcfg_dict_t *dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);
}

/* == End of config.c ====================================================== */
