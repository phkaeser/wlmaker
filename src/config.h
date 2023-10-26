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
 * limitations under the License.
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "cairo_util.h"

#include <inttypes.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

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
    /** Will set all windows to CLIENT, no mather what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_CLIENT,
    /** Will set all windows to SERVER, no mather what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_SERVER
} wlmaker_config_decoration_t;

/** The theme. */
typedef struct {
    /** Color of the window margin. */
    uint32_t                  window_margin_color;
    /** Width of the window margin, in pixels. */
    uint32_t                  window_margin_width;

    /** Color of the title text when focussed. */
    uint32_t                  titlebar_focussed_text_color;
    /** Color of the title text when blurred. */
    uint32_t                  titlebar_blurred_text_color;

    /** Fill style of the title bar, when focussed. Including buttons. */
    wlmtk_style_fill_t        titlebar_focussed_fill;
    /** Fill style of the title bar, when blurred. Including buttons. */
    wlmtk_style_fill_t       titlebar_blurred_fill;

    /** Fill style of the resize bar. */
    wlmtk_style_fill_t        resizebar_fill;
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

extern const int32_t config_keyboard_repeat_rate;
extern const int32_t config_keyboard_repeat_delay;
extern const struct xkb_rule_names *config_keyboard_rule_names;

extern const char *config_xcursor_theme_name;
extern const uint32_t config_xcursor_theme_size;

extern const float config_output_scale;

extern const wlmaker_config_decoration_t config_decoration;

extern const uint64_t wlmaker_config_double_click_wait_msec;
extern const uint32_t wlmaker_config_window_drag_modifiers;

extern const wlmaker_config_workspace_t wlmaker_config_workspaces[];
extern const wlmaker_config_theme_t wlmaker_config_theme;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CONFIG_H__ */
/* == End of config.h ====================================================== */
