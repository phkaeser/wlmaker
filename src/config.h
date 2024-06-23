/* ========================================================================= */
/**
 * @file config.h
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
 * limitations under the iLicense.
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <inttypes.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

#include "conf/plist.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Preference for decoration. */
typedef enum {
    /** Mode NONE will be set to CLIENT; but other modes left unchanged. */
    WLMAKER_CONFIG_DECORATION_SUGGEST_CLIENT,
    /** Mode NONE will be set to SERVER; but other modes left unchanged. */
    WLMAKER_CONFIG_DECORATION_SUGGEST_SERVER,
    /** Will set all windows to CLIENT, no matter what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_CLIENT,
    /** Will set all windows to SERVER, no matter what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_SERVER
} wlmaker_config_decoration_t;

/** Style information. Replaces @ref wlmaker_config_theme_t. */
typedef struct {
    /** The tile. */
    wlmtk_tile_style_t        tile;
    /** Dock optics: Margin. */
    wlmtk_dock_style_t        dock;
    /** Window style. */
    wlmtk_window_style_t      window;
} wlmaker_config_style_t;

/** The theme. */
typedef struct {
    /** Color of the window margin. */
    uint32_t                  window_margin_color;
    /** Width of the window margin, in pixels. */
    uint32_t                  window_margin_width;

    /** Fill style of a tile. */
    wlmtk_style_fill_t        tile_fill;
    /** File style of the title element of an iconified. */
    wlmtk_style_fill_t        iconified_title_fill;
    /** Color of the iconified's title. */
    uint32_t                  iconified_title_color;

    /** Fill style of the menu's background. */
    wlmtk_style_fill_t        menu_fill;
    /** Color of the menu's margin and padding. */
    uint32_t                  menu_margin_color;
    /** Width of the menu's margin. */
    uint32_t                  menu_margin_width;
    /** Width of the paddin between menu elements. In `menu_margin_color. */
    uint32_t                  menu_padding_width;

    /** Fill style of a menu item when enabled. */
    wlmtk_style_fill_t        menu_item_enabled_fill;
    /** Text color of menu item when enabled. */
    uint32_t                  menu_item_enabled_text_color;
    /** Fill style of a menu item when selected. */
    wlmtk_style_fill_t        menu_item_selected_fill;
    /** Text color of menu item when selected. */
    uint32_t                  menu_item_selected_text_color;

    /** Fill style of the task list. */
    wlmtk_style_fill_t        task_list_fill;
    /** Color of the text describing tasks in the task list. */
    uint32_t                  task_list_text_color;
} wlmaker_config_theme_t;

/** Configuration for a workspace. */
typedef struct {
    /** Name of the workspace. NULL indicates this is a sentinel element. */
    const char                *name_ptr;
    /** Workspace's background color, as 8888 RGBA. */
    uint32_t                  color;
} wlmaker_config_workspace_t;

extern const char *config_xcursor_theme_name;
extern const uint32_t config_xcursor_theme_size;

extern const float config_output_scale;

extern const wlmaker_config_decoration_t config_decoration;

extern const uint32_t wlmaker_config_window_drag_modifiers;

extern const wlmaker_config_workspace_t wlmaker_config_workspaces[];
extern const wlmaker_config_theme_t wlmaker_config_theme;

/**
 * Loads the configuration for wlmaker.
 *
 * If `fname_ptr` is give, it will attempt to load the configuration from the
 * specified file. Otherwise, it will attempt to load the files specified in
 * @ref _wlmaker_config_fname_ptrs, or (at last) use the built-in default.
 *
 * @param fname_ptr           Optional: Name of the file to load it from. May
 *                            be NULL.
 *
 * @return A dict object, or NULL on error. Errors will already be logged.
 *     The caller must free the associated resources by calling
 *     @ref wlmcfg_object_unref.
 */
wlmcfg_dict_t *wlmaker_config_load(const char *fname_ptr);

extern const wlmcfg_desc_t wlmaker_config_style_desc[];

/** Unit test cases. */
extern const bs_test_case_t wlmaker_config_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CONFIG_H__ */
/* == End of config.h ====================================================== */
