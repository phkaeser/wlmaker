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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Repeat rate, per second. */
const int32_t config_keyboard_repeat_rate = 25;

/** Repeat delay, in ms. */
const int32_t config_keyboard_repeat_delay = 300;

/** XKB Keymap to use. NULL identifies the default ('us'). */
const struct xkb_rule_names *config_keyboard_rule_names = NULL;

/** Name of the xcursor theme. NULL picks the default. */
const char *config_xcursor_theme_name = NULL;

/** Base size for the xcursor theme (when scale==1.0). */
const uint32_t config_xcursor_theme_size = 24;

/** Overall scale of output. */
const float config_output_scale = 1.0;

/** Whether to always request server-side decorations. */
const wlmaker_config_decoration_t config_decoration =
    WLMAKER_CONFIG_DECORATION_SUGGEST_SERVER;

/** Time interval within two clicks need to happen to count as double-click. */
const uint64_t wlmaker_config_double_click_wait_msec = 250ull;

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

/* == End of config.c ====================================================== */
