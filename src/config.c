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
#include "default_dock_state.h"
#include "style_default.h"

/* == Declarations ========================================================= */

static wlmcfg_dict_t *_wlmaker_config_from_plist(const char *fname_ptr);

static bool _wlmaker_config_decode_fill_style(
    wlmcfg_object_t *object_ptr,
    void *dest_ptr);

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
};

/** Plist decoding descriptor of the fill type. */
static const wlmcfg_enum_desc_t _wlmaker_config_fill_type_desc[] = {
    WLMCFG_ENUM("SOLID", WLMTK_STYLE_COLOR_SOLID),
    WLMCFG_ENUM("HGRADIENT", WLMTK_STYLE_COLOR_HGRADIENT),
    WLMCFG_ENUM("DGRADIENT", WLMTK_STYLE_COLOR_DGRADIENT),
    WLMCFG_ENUM_SENTINEL()
};

/** Plist decoding descriptor for font weight. */
static const wlmcfg_enum_desc_t _wlmaker_config_font_weight_desc[] = {
    WLMCFG_ENUM("Normal", WLMTK_FONT_WEIGHT_NORMAL),
    WLMCFG_ENUM("Bold", WLMTK_FONT_WEIGHT_BOLD),
    WLMCFG_ENUM_SENTINEL()
};

/** Plist decoding descriptor of the fill style. */
static const wlmcfg_desc_t _wlmaker_config_fill_style_desc[] = {
    WLMCFG_DESC_ENUM("Type", true, wlmtk_style_fill_t, type,
                     WLMTK_STYLE_COLOR_SOLID,
                     _wlmaker_config_fill_type_desc),
    WLMCFG_DESC_SENTINEL()
};

/** Plist decoding descriptor of the solid color. */
static const wlmcfg_desc_t _wlmaker_config_style_color_solid_desc[] = {
    WLMCFG_DESC_ARGB32(
        "Color", true, wlmtk_style_color_solid_data_t, color, 0),
    WLMCFG_DESC_SENTINEL()
};

/** Plist decoding descriptor of a color gradient. */
static const wlmcfg_desc_t _wlmaker_config_style_color_gradient_desc[] = {
    WLMCFG_DESC_ARGB32(
        "From", true, wlmtk_style_color_gradient_data_t, from, 0),
    WLMCFG_DESC_ARGB32(
        "To", true, wlmtk_style_color_gradient_data_t, to, 0),
    WLMCFG_DESC_SENTINEL()
};

/** Plist decoding descriptor of a tile style. */
static const wlmcfg_desc_t _wlmaker_config_tile_style_desc[] = {
    WLMCFG_DESC_UINT64(
        "Size", true, wlmtk_tile_style_t, size, 64),
    WLMCFG_DESC_UINT64(
        "BezelWidth", true, wlmtk_tile_style_t, bezel_width, 2),
    WLMCFG_DESC_CUSTOM(
        "Fill", true, wlmtk_tile_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    WLMCFG_DESC_SENTINEL()
};

/** Plist decoding descriptor of a margin's style. */
static const wlmcfg_desc_t _wlmaker_config_margin_style_desc[] = {
    WLMCFG_DESC_UINT64(
        "Width", true, wlmtk_margin_style_t, width, 0),
    WLMCFG_DESC_ARGB32(
        "Color", true, wlmtk_margin_style_t, color, 0xff000000),
    WLMCFG_DESC_SENTINEL()
};

/** Plist decoding descriptor of the dock's style. */
static const wlmcfg_desc_t _wlmaker_config_dock_style_desc[] = {
    WLMCFG_DESC_DICT(
        "Margin", true, wlmtk_dock_style_t, margin,
        _wlmaker_config_margin_style_desc),
    WLMCFG_DESC_SENTINEL()
};

/** Descriptor for decoding "Font" sections. */
static const wlmcfg_desc_t _wlmaker_config_font_style_desc[] = {
    WLMCFG_DESC_CHARBUF(
        "Face", true, wlmtk_style_font_t, face,
        WLMTK_STYLE_FONT_FACE_LENGTH, NULL),
    WLMCFG_DESC_ENUM(
        "Weight", true, wlmtk_style_font_t, weight,
        WLMTK_FONT_WEIGHT_NORMAL, _wlmaker_config_font_weight_desc),
    WLMCFG_DESC_UINT64(
        "Size", true, wlmtk_style_font_t, size, 10),
    WLMCFG_DESC_SENTINEL()
};

/** Descroptor for decoding the "TitleBar" dict below "Window". */
static const wlmcfg_desc_t _wlmaker_config_window_titlebar_style_desc[] = {
    WLMCFG_DESC_CUSTOM(
        "FocussedFill", true, wlmtk_titlebar_style_t, focussed_fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    WLMCFG_DESC_ARGB32(
        "FocussedTextColor", true, wlmtk_titlebar_style_t,
        focussed_text_color, 0),
    WLMCFG_DESC_CUSTOM(
        "BlurredFill", true, wlmtk_titlebar_style_t, blurred_fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    WLMCFG_DESC_ARGB32(
        "BlurredTextColor", true, wlmtk_titlebar_style_t,
        blurred_text_color, 0),
    WLMCFG_DESC_UINT64(
        "Height", true, wlmtk_titlebar_style_t, height, 22),
    WLMCFG_DESC_UINT64(
        "BezelWidth", true, wlmtk_titlebar_style_t, bezel_width, 1),
    WLMCFG_DESC_DICT(
        "Margin", true, wlmtk_titlebar_style_t, margin,
        _wlmaker_config_margin_style_desc),
    WLMCFG_DESC_DICT(
        "Font", true, wlmtk_titlebar_style_t, font,
        _wlmaker_config_font_style_desc),
    WLMCFG_DESC_SENTINEL()
 };

/** Descroptor for decoding the "TitleBar" dict below "Window". */
static const wlmcfg_desc_t _wlmaker_config_window_resize_style_desc[] = {
    WLMCFG_DESC_CUSTOM(
        "Fill", true, wlmtk_resizebar_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    WLMCFG_DESC_UINT64(
        "Height", true, wlmtk_resizebar_style_t, height, 7),
    WLMCFG_DESC_UINT64(
        "BezelWidth", true, wlmtk_resizebar_style_t, bezel_width, 1),
    WLMCFG_DESC_UINT64(
        "CornerWidth", true, wlmtk_resizebar_style_t, corner_width, 1),
    WLMCFG_DESC_SENTINEL()
 };

/** Descriptor for decoding the "Window" dictionary. */
static const wlmcfg_desc_t _wlmaker_config_window_style_desc[] = {
    WLMCFG_DESC_DICT(
        "TitleBar", true, wlmtk_window_style_t, titlebar,
        _wlmaker_config_window_titlebar_style_desc),
    WLMCFG_DESC_DICT(
        "ResizeBar", true, wlmtk_window_style_t, resizebar,
        _wlmaker_config_window_resize_style_desc),
    WLMCFG_DESC_DICT(
        "Border", true, wlmtk_window_style_t, border,
        _wlmaker_config_margin_style_desc),
    WLMCFG_DESC_DICT(
        "Margin", true, wlmtk_window_style_t, margin,
        _wlmaker_config_margin_style_desc),
    WLMCFG_DESC_SENTINEL()
};

/** Descriptor for decoding the "TaskList" dictionary. */
static const wlmcfg_desc_t _wlmaker_task_list_style_desc[] = {
    WLMCFG_DESC_CUSTOM(
        "Fill", true, wlmaker_config_task_list_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    WLMCFG_DESC_DICT(
        "Font", true, wlmaker_config_task_list_style_t, font,
        _wlmaker_config_font_style_desc),
    WLMCFG_DESC_ARGB32(
        "TextColor", true, wlmaker_config_task_list_style_t,
        text_color, 0),
    WLMCFG_DESC_SENTINEL()
};

/** Descriptor for decoding the "Clip" dictionary. */
static const wlmcfg_desc_t _wlmaker_clip_style_desc[] = {
    WLMCFG_DESC_DICT(
        "Font", true, wlmaker_config_clip_style_t, font,
        _wlmaker_config_font_style_desc),
    WLMCFG_DESC_ARGB32(
        "TextColor", true, wlmaker_config_clip_style_t,
        text_color, 0),
    WLMCFG_DESC_SENTINEL()
};

/** Desciptor for decoding the style information from a plist. */
const wlmcfg_desc_t wlmaker_config_style_desc[] = {
    WLMCFG_DESC_DICT(
        "Tile", true, wlmaker_config_style_t, tile,
        _wlmaker_config_tile_style_desc),
    WLMCFG_DESC_DICT(
        "Dock", true, wlmaker_config_style_t, dock,
        _wlmaker_config_dock_style_desc),
    WLMCFG_DESC_DICT(
        "Window", true, wlmaker_config_style_t, window,
        _wlmaker_config_window_style_desc),
    WLMCFG_DESC_DICT(
        "TaskList", true, wlmaker_config_style_t, task_list,
        _wlmaker_task_list_style_desc),
    WLMCFG_DESC_DICT(
        "Clip", true, wlmaker_config_style_t, clip,
        _wlmaker_clip_style_desc),
    WLMCFG_DESC_SENTINEL()
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

/* ------------------------------------------------------------------------- */
/**
 * Custom decoder for fill style struct from a plist dict.
 *
 * @param object_ptr
 * @param dest_ptr
 *
 * @return true on success.
 */
bool _wlmaker_config_decode_fill_style(
    wlmcfg_object_t *object_ptr,
    void *dest_ptr)
{
    wlmtk_style_fill_t *fill_ptr = dest_ptr;
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(object_ptr);
    if (NULL == dict_ptr) return false;

    if (!wlmcfg_decode_dict(
            dict_ptr,
            _wlmaker_config_fill_style_desc,
            fill_ptr)) return false;

    switch (fill_ptr->type) {
    case WLMTK_STYLE_COLOR_SOLID:
        return wlmcfg_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_solid_desc,
            &fill_ptr->param.solid);
    case WLMTK_STYLE_COLOR_DGRADIENT:
        return wlmcfg_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.dgradient);
    case WLMTK_STYLE_COLOR_HGRADIENT:
        return wlmcfg_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.hgradient);
    default:
        bs_log(BS_ERROR, "Unhandled fill type %d", fill_ptr->type);
        return false;
    }
    bs_log(BS_FATAL, "Uh... no idea how this got here.");
    return false;
}

/* == Unit tests =========================================================== */

static void test_embedded(bs_test_t *test_ptr);
static void test_file(bs_test_t *test_ptr);
static void test_style_file(bs_test_t *test_ptr);
static void test_decode_fill(bs_test_t *test_ptr);
static void test_decode_font(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_config_test_cases[] = {
    { 1, "embedded", test_embedded },
    { 1, "file", test_file },
    { 1, "style_file", test_style_file },
    { 1, "decode_fill", test_decode_fill },
    { 1, "decode_font", test_decode_font },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Verifies that the embedded config loads. */
// TODO(kaeser@gubbe.ch): For now, this just verifies that the configuration
// **parses**. There is no further verification of the dict contents. Would
// be great to extend this.
void test_embedded(bs_test_t *test_ptr)
{
    wlmcfg_object_t *obj_ptr;

    obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_dock_state_data,
        embedded_binary_default_dock_state_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_style_default_data,
        embedded_binary_style_default_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that the (example) config files are loading. */
// TODO(kaeser@gubbe.ch): For now, this just verifies that the configuration
// file **parses**. There is no further verification of the dict contents.
// Would be great to extend this.
void test_file(bs_test_t *test_ptr)
{
    wlmcfg_dict_t *dict_ptr;

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif
    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker-home.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/dock.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Loads and decodes the style file. */
void test_style_file(bs_test_t *test_ptr)
{
    wlmcfg_dict_t *dict_ptr;
    wlmaker_config_style_t config_style;

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/style-default.plist");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_decode_dict(
            dict_ptr, wlmaker_config_style_desc, &config_style));
    wlmcfg_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/style-debian.plist");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_decode_dict(
            dict_ptr, wlmaker_config_style_desc, &config_style));
    wlmcfg_dict_unref(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the decoder for the fill style. */
void test_decode_fill(bs_test_t *test_ptr)
{
    const char *s = ("{"
                     "Type = DGRADIENT;"
                     "From = \"argb32:0x01020304\";"
                     "To = \"argb32:0x0204080c\""
                     "}");
    wlmtk_style_fill_t fill;
    wlmcfg_object_t *object_ptr;

    object_ptr = wlmcfg_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_DGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, fill.param.dgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x0204080c, fill.param.dgradient.to);
    wlmcfg_object_unref(object_ptr);

    s = ("{"
         "Type = HGRADIENT;"
         "From = \"argb32:0x04030201\";"
         "To = \"argb32:0x40302010\""
         "}");
    object_ptr = wlmcfg_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_HGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x04030201, fill.param.hgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x40302010, fill.param.hgradient.to);
    wlmcfg_object_unref(object_ptr);

    s = ("{"
         "Type = SOLID;"
         "Color = \"argb32:0x11223344\""
         "}");
    object_ptr = wlmcfg_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_SOLID, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x11223344, fill.param.solid.color);
    wlmcfg_object_unref(object_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the decoder for a font descriptor. */
void test_decode_font(bs_test_t *test_ptr)
{
    wlmcfg_object_t *object_ptr;
    const char *s = ("{"
                     "Face = Helvetica;"
                     "Weight = Bold;"
                     "Size = 12;"
                     "}");

    object_ptr = wlmcfg_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    wlmtk_style_font_t font = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_decode_dict(dict_ptr, _wlmaker_config_font_style_desc, &font));

    BS_TEST_VERIFY_STREQ(test_ptr, "Helvetica", font.face);
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_FONT_WEIGHT_BOLD, font.weight);
    BS_TEST_VERIFY_EQ(test_ptr, 12, font.size);

    wlmcfg_object_unref(object_ptr);
}

/* == End of config.c ====================================================== */
