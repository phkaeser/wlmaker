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
#include "default_state.h"
#include "../etc/style.h"

/* == Declarations ========================================================= */

static bspl_dict_t *_wlmaker_config_from_plist(const char *fname_ptr);

static bool _wlmaker_config_decode_fill_style(
    bspl_object_t *object_ptr,
    void *dest_ptr);

/* == Data ================================================================= */

/** Overall scale of output. */
const float config_output_scale = 1.0;

/** Plist decoding descriptor of the fill type. */
static const bspl_enum_desc_t _wlmaker_config_fill_type_desc[] = {
    BSPL_ENUM("SOLID", WLMTK_STYLE_COLOR_SOLID),
    BSPL_ENUM("HGRADIENT", WLMTK_STYLE_COLOR_HGRADIENT),
    BSPL_ENUM("VGRADIENT", WLMTK_STYLE_COLOR_VGRADIENT),
    BSPL_ENUM("DGRADIENT", WLMTK_STYLE_COLOR_DGRADIENT),
    BSPL_ENUM("ADGRADIENT", WLMTK_STYLE_COLOR_ADGRADIENT),
    BSPL_ENUM_SENTINEL()
};

/** Plist decoding descriptor for font weight. */
static const bspl_enum_desc_t _wlmaker_config_font_weight_desc[] = {
    BSPL_ENUM("Normal", WLMTK_FONT_WEIGHT_NORMAL),
    BSPL_ENUM("Bold", WLMTK_FONT_WEIGHT_BOLD),
    BSPL_ENUM_SENTINEL()
};

/** Plist decoding descriptor of the fill style. */
static const bspl_desc_t _wlmaker_config_fill_style_desc[] = {
    BSPL_DESC_ENUM("Type", true, wlmtk_style_fill_t, type,
                     WLMTK_STYLE_COLOR_SOLID,
                     _wlmaker_config_fill_type_desc),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of the solid color. */
static const bspl_desc_t _wlmaker_config_style_color_solid_desc[] = {
    BSPL_DESC_ARGB32(
        "Color", true, wlmtk_style_color_solid_data_t, color, 0),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of a color gradient. */
static const bspl_desc_t _wlmaker_config_style_color_gradient_desc[] = {
    BSPL_DESC_ARGB32(
        "From", true, wlmtk_style_color_gradient_data_t, from, 0),
    BSPL_DESC_ARGB32(
        "To", true, wlmtk_style_color_gradient_data_t, to, 0),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of a tile style. */
static const bspl_desc_t _wlmaker_config_tile_style_desc[] = {
    BSPL_DESC_UINT64(
        "Size", true, wlmtk_tile_style_t, size, 64),
    BSPL_DESC_UINT64(
        "ContentSize", true, wlmtk_tile_style_t, content_size, 48),
    BSPL_DESC_UINT64(
        "BezelWidth", true, wlmtk_tile_style_t, bezel_width, 2),
    BSPL_DESC_CUSTOM(
        "Fill", true, wlmtk_tile_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of a margin's style. */
static const bspl_desc_t _wlmaker_config_margin_style_desc[] = {
    BSPL_DESC_UINT64(
        "Width", true, wlmtk_margin_style_t, width, 0),
    BSPL_DESC_ARGB32(
        "Color", true, wlmtk_margin_style_t, color, 0xff000000),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of the dock's style. */
static const bspl_desc_t _wlmaker_config_dock_style_desc[] = {
    BSPL_DESC_DICT(
        "Margin", true, wlmtk_dock_style_t, margin,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding "Font" sections. */
static const bspl_desc_t _wlmaker_config_font_style_desc[] = {
    BSPL_DESC_CHARBUF(
        "Face", true, wlmtk_style_font_t, face,
        WLMTK_STYLE_FONT_FACE_LENGTH, NULL),
    BSPL_DESC_ENUM(
        "Weight", true, wlmtk_style_font_t, weight,
        WLMTK_FONT_WEIGHT_NORMAL, _wlmaker_config_font_weight_desc),
    BSPL_DESC_UINT64(
        "Size", true, wlmtk_style_font_t, size, 10),
    BSPL_DESC_SENTINEL()
};

/** Descroptor for decoding the "TitleBar" dict below "Window". */
static const bspl_desc_t _wlmaker_config_window_titlebar_style_desc[] = {
    BSPL_DESC_CUSTOM(
        "FocussedFill", true, wlmtk_titlebar_style_t, focussed_fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_ARGB32(
        "FocussedTextColor", true, wlmtk_titlebar_style_t,
        focussed_text_color, 0),
    BSPL_DESC_CUSTOM(
        "BlurredFill", true, wlmtk_titlebar_style_t, blurred_fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_ARGB32(
        "BlurredTextColor", true, wlmtk_titlebar_style_t,
        blurred_text_color, 0),
    BSPL_DESC_UINT64(
        "Height", true, wlmtk_titlebar_style_t, height, 22),
    BSPL_DESC_UINT64(
        "BezelWidth", true, wlmtk_titlebar_style_t, bezel_width, 1),
    BSPL_DESC_DICT(
        "Margin", true, wlmtk_titlebar_style_t, margin,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_DICT(
        "Font", true, wlmtk_titlebar_style_t, font,
        _wlmaker_config_font_style_desc),
    BSPL_DESC_SENTINEL()
 };

/** Descroptor for decoding the "TitleBar" dict below "Window". */
static const bspl_desc_t _wlmaker_config_window_resize_style_desc[] = {
    BSPL_DESC_CUSTOM(
        "Fill", true, wlmtk_resizebar_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_UINT64(
        "Height", true, wlmtk_resizebar_style_t, height, 7),
    BSPL_DESC_UINT64(
        "BezelWidth", true, wlmtk_resizebar_style_t, bezel_width, 1),
    BSPL_DESC_UINT64(
        "CornerWidth", true, wlmtk_resizebar_style_t, corner_width, 1),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "Window" dictionary. */
static const bspl_desc_t _wlmaker_config_window_style_desc[] = {
    BSPL_DESC_DICT(
        "TitleBar", true, wlmtk_window_style_t, titlebar,
        _wlmaker_config_window_titlebar_style_desc),
    BSPL_DESC_DICT(
        "ResizeBar", true, wlmtk_window_style_t, resizebar,
        _wlmaker_config_window_resize_style_desc),
    BSPL_DESC_DICT(
        "Border", true, wlmtk_window_style_t, border,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_DICT(
        "Margin", true, wlmtk_window_style_t, margin,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "Item" dictionary. */
static const bspl_desc_t _wlmaker_config_menu_item_style_desc[] = {
    BSPL_DESC_CUSTOM(
        "Fill", true, wlmtk_menu_item_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_CUSTOM(
        "HighlightedFill", true, wlmtk_menu_item_style_t, highlighted_fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_DICT(
        "Font", true, wlmtk_menu_item_style_t, font,
        _wlmaker_config_font_style_desc),
    BSPL_DESC_ARGB32(
        "EnabledTextColor", true, wlmtk_menu_item_style_t,
        enabled_text_color, 0),
    BSPL_DESC_ARGB32(
        "HighlightedTextColor", true, wlmtk_menu_item_style_t,
        highlighted_text_color, 0),
    BSPL_DESC_ARGB32(
        "DisabledTextColor", true, wlmtk_menu_item_style_t,
        disabled_text_color, 0),
    BSPL_DESC_UINT64(
        "Height", true, wlmtk_menu_item_style_t, height, 20),
    BSPL_DESC_UINT64(
        "BezelWidth", true, wlmtk_menu_item_style_t, bezel_width, 1),
    BSPL_DESC_UINT64(
        "Width", true, wlmtk_menu_item_style_t, width, 80),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "Menu" dictionary. */
static const bspl_desc_t _wlmaker_config_menu_style_desc[] = {
    BSPL_DESC_DICT(
        "Item", true, wlmtk_menu_style_t, item,
        _wlmaker_config_menu_item_style_desc),
    BSPL_DESC_DICT(
        "Margin", true, wlmtk_menu_style_t, margin,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_DICT(
        "Border", true, wlmtk_menu_style_t, border,
        _wlmaker_config_margin_style_desc),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "TaskList" dictionary. */
static const bspl_desc_t _wlmaker_task_list_style_desc[] = {
    BSPL_DESC_CUSTOM(
        "Fill", true, wlmaker_config_task_list_style_t, fill,
        _wlmaker_config_decode_fill_style, NULL, NULL),
    BSPL_DESC_DICT(
        "Font", true, wlmaker_config_task_list_style_t, font,
        _wlmaker_config_font_style_desc),
    BSPL_DESC_ARGB32(
        "TextColor", true, wlmaker_config_task_list_style_t,
        text_color, 0),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "Clip" dictionary. */
static const bspl_desc_t _wlmaker_clip_style_desc[] = {
    BSPL_DESC_DICT(
        "Font", true, wlmaker_config_clip_style_t, font,
        _wlmaker_config_font_style_desc),
    BSPL_DESC_ARGB32(
        "TextColor", true, wlmaker_config_clip_style_t,
        text_color, 0),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for decoding the "Cursor" dictionary. */
static const bspl_desc_t _wlmaker_cursor_style_desc[] = {
    BSPL_DESC_STRING(
        "Name", true, wlmaker_config_cursor_style_t, name_ptr, "default"),
    BSPL_DESC_UINT64(
        "Size", true, wlmaker_config_cursor_style_t, size, 24),
    BSPL_DESC_SENTINEL()
};

/** Desciptor for decoding the style information from a plist. */
const bspl_desc_t wlmaker_config_style_desc[] = {
    BSPL_DESC_ARGB32(
        "BackgroundColor", true, wlmaker_config_style_t, background_color, 0),
    BSPL_DESC_DICT(
        "Tile", true, wlmaker_config_style_t, tile,
        _wlmaker_config_tile_style_desc),
    BSPL_DESC_DICT(
        "Dock", true, wlmaker_config_style_t, dock,
        _wlmaker_config_dock_style_desc),
    BSPL_DESC_DICT(
        "Window", true, wlmaker_config_style_t, window,
        _wlmaker_config_window_style_desc),
    BSPL_DESC_DICT(
        "Menu", true, wlmaker_config_style_t, menu,
        _wlmaker_config_menu_style_desc),
    BSPL_DESC_DICT(
        "TaskList", true, wlmaker_config_style_t, task_list,
        _wlmaker_task_list_style_desc),
    BSPL_DESC_DICT(
        "Clip", true, wlmaker_config_style_t, clip,
        _wlmaker_clip_style_desc),
    BSPL_DESC_DICT(
        "Cursor", true, wlmaker_config_style_t, cursor,
        _wlmaker_cursor_style_desc),
    BSPL_DESC_SENTINEL()
};

/** Lookup paths for the configuration file. */
static const char *_wlmaker_config_fname_ptrs[] = {
    "~/.wlmaker.plist",
    "/usr/share/wlmaker/wlmaker.plist",
    NULL  // Sentinel.
};

/** Lookup paths for the configuration file. */
static const char *_wlmaker_state_fname_ptrs[] = {
    "~/.wlmaker-state.plist",
    "/usr/share/wlmaker/state.plist",
    NULL  // Sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bspl_object_t *wlmaker_plist_load(
    const char *name_ptr,
    const char *fname_ptr,
    const char **fname_defaults,
    const uint8_t *default_data_ptr,
    size_t default_data_size)
{
    if (NULL != fname_ptr) {
        bs_log(BS_INFO, "Loading %s plist from file \"%s\"",
               name_ptr, fname_ptr);
        bspl_object_t *object_ptr = bspl_create_object_from_plist_file(
            fname_ptr);
        if (NULL == object_ptr) {
            bs_log(BS_ERROR,
                   "Failed bspl_create_object_from_plist(%s) for %s",
                   fname_ptr, name_ptr);
        }
        return object_ptr;
    }

    if (NULL != fname_defaults) {
        for (; *fname_defaults != NULL; ++fname_defaults) {
            char full_path[PATH_MAX];
            char *path_ptr = bs_file_resolve_path(*fname_defaults, full_path);
            if (NULL == path_ptr) {
                bs_log(BS_DEBUG | BS_ERRNO,
                       "Failed bs_file_resolve_path(%s, %p) for %s",
                       *fname_defaults, full_path, name_ptr);
                continue;
            }

            // If we get here, there was a resolved item at the path. A load
            // failure indicates an issue with an existing file, and we should
            // fail here.
            bs_log(BS_INFO, "Loading %s plist from file \"%s\"",
                   name_ptr, path_ptr);
            return bspl_create_object_from_plist_file(path_ptr);
        }
    }

    if (NULL == default_data_ptr) return NULL;
    bs_log(BS_INFO, "Using compiled-in data for %s plist.", name_ptr);
    return bspl_create_object_from_plist_data(
        default_data_ptr, default_data_size);
}

/* ------------------------------------------------------------------------- */
bspl_dict_t *wlmaker_config_load(const char *fname_ptr)
{
    bspl_object_t *object_ptr = wlmaker_plist_load(
        "wlmaker config",
        fname_ptr,
        _wlmaker_config_fname_ptrs,
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    return BS_ASSERT_NOTNULL(bspl_dict_from_object(object_ptr));
}

/* ------------------------------------------------------------------------- */
bspl_dict_t *wlmaker_state_load(const char *fname_ptr)
{
    bspl_object_t *object_ptr = wlmaker_plist_load(
        "wlmaker state",
        fname_ptr,
        _wlmaker_state_fname_ptrs,
        embedded_binary_default_state_data,
        embedded_binary_default_state_size);
    return BS_ASSERT_NOTNULL(bspl_dict_from_object(object_ptr));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Loads a plist dict from fname_ptr. Returns NULL on error. */
bspl_dict_t *_wlmaker_config_from_plist(const char *fname_ptr)
{
    bspl_object_t *obj_ptr = bspl_create_object_from_plist_file(fname_ptr);
    if (NULL == obj_ptr) {
        bs_log(BS_ERROR, "Failed bspl_create_object_from_plist(%s)",
               fname_ptr);
        return NULL;
    }

    bspl_dict_t *dict_ptr = bspl_dict_from_object(obj_ptr);
    if (NULL == dict_ptr) {
        bs_log(BS_ERROR, "Not a plist dict in %s", fname_ptr);
        bspl_object_unref(obj_ptr);
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
    bspl_object_t *object_ptr,
    void *dest_ptr)
{
    wlmtk_style_fill_t *fill_ptr = dest_ptr;
    bspl_dict_t *dict_ptr = bspl_dict_from_object(object_ptr);
    if (NULL == dict_ptr) return false;

    if (!bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_fill_style_desc,
            fill_ptr)) return false;

    switch (fill_ptr->type) {
    case WLMTK_STYLE_COLOR_SOLID:
        return bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_solid_desc,
            &fill_ptr->param.solid);
    case WLMTK_STYLE_COLOR_HGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.hgradient);
    case WLMTK_STYLE_COLOR_VGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.vgradient);
    case WLMTK_STYLE_COLOR_DGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.dgradient);
    case WLMTK_STYLE_COLOR_ADGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmaker_config_style_color_gradient_desc,
            &fill_ptr->param.adgradient);
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
    bspl_object_t *obj_ptr;

    obj_ptr = bspl_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, bspl_dict_from_object(obj_ptr));
    bspl_object_unref(obj_ptr);

    obj_ptr = bspl_create_object_from_plist_data(
        embedded_binary_default_state_data,
        embedded_binary_default_state_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, bspl_dict_from_object(obj_ptr));
    bspl_object_unref(obj_ptr);

    obj_ptr = bspl_create_object_from_plist_data(
        embedded_binary_style_data,
        embedded_binary_style_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, bspl_dict_from_object(obj_ptr));
    bspl_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that the (example) config files are loading. */
// TODO(kaeser@gubbe.ch): For now, this just verifies that the configuration
// file **parses**. There is no further verification of the dict contents.
// Would be great to extend this.
void test_file(bs_test_t *test_ptr)
{
    bspl_dict_t *dict_ptr;

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif
    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker-home.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/wlmaker-state.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Loads and decodes the style file. */
void test_style_file(bs_test_t *test_ptr)
{
    bspl_dict_t *dict_ptr;
    wlmaker_config_style_t config_style = {};

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/style.plist");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_decode_dict(
            dict_ptr, wlmaker_config_style_desc, &config_style));
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/style-debian.plist");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_decode_dict(
            dict_ptr, wlmaker_config_style_desc, &config_style));
    bspl_dict_unref(dict_ptr);
    free(config_style.cursor.name_ptr);
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
    bspl_object_t *object_ptr;

    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_DGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, fill.param.dgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x0204080c, fill.param.dgradient.to);
    bspl_object_unref(object_ptr);

    s = ("{"
         "Type = HGRADIENT;"
         "From = \"argb32:0x04030201\";"
         "To = \"argb32:0x40302010\""
         "}");
    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_HGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x04030201, fill.param.hgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x40302010, fill.param.hgradient.to);
    bspl_object_unref(object_ptr);

    s = ("{"
         "Type = SOLID;"
         "Color = \"argb32:0x11223344\""
         "}");
    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmaker_config_decode_fill_style(object_ptr, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_SOLID, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x11223344, fill.param.solid.color);
    bspl_object_unref(object_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the decoder for a font descriptor. */
void test_decode_font(bs_test_t *test_ptr)
{
    bspl_object_t *object_ptr;
    const char *s = ("{"
                     "Face = Helvetica;"
                     "Weight = Bold;"
                     "Size = 12;"
                     "}");

    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    bspl_dict_t *dict_ptr = bspl_dict_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    wlmtk_style_font_t font = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_decode_dict(dict_ptr, _wlmaker_config_font_style_desc, &font));

    BS_TEST_VERIFY_STREQ(test_ptr, "Helvetica", font.face);
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_FONT_WEIGHT_BOLD, font.weight);
    BS_TEST_VERIFY_EQ(test_ptr, 12, font.size);

    bspl_object_unref(object_ptr);
}

/* == End of config.c ====================================================== */
