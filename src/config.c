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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

#include "../share/theme.h"  // IWYU pragma: keep
#include "default_configuration.h"
#include "default_state.h"
#include "input/cursor.h"

/* == Declarations ========================================================= */

static bspl_object_t *_wlmaker_plist_load(
    const char *name_ptr,
    const char *fname_ptr,
    const uint8_t *default_data_ptr,
    size_t default_data_size);
static bspl_dict_t *_wlmaker_config_from_plist(const char *fname_ptr);

/* == Data ================================================================= */

/** Descriptor for decoding the "Clip" dictionary. */
static const bspl_desc_t _wlmaker_clip_style_desc[] = {
    BSPL_DESC_DICT(
        "Font", true, wlmaker_config_clip_style_t, font, font,
        wlmtk_style_font_desc),
    BSPL_DESC_ARGB32(
        "TextColor", true, wlmaker_config_clip_style_t,
        text_color, text_color, 0),
    BSPL_DESC_SENTINEL()
};

/** Desciptor for decoding the style information from a plist. */
const bspl_desc_t wlmaker_config_style_desc[] = {
    BSPL_DESC_ARGB32(
        "BackgroundColor", true, wlmaker_config_style_t, background_color,
        background_color, 0),
    BSPL_DESC_DICT(
        "Tile", true, wlmaker_config_style_t, tile, tile,
        wlmtk_tile_style_desc),
    BSPL_DESC_DICT(
        "Dock", true, wlmaker_config_style_t, dock, dock,
        wlmtk_dock_style_desc),
    BSPL_DESC_CUSTOM(
        "Window", true, wlmaker_config_style_t,
        window_style_ptr, has_window_style,
        wlmtk_window_style_decode, NULL,
        wlmtk_window_style_decode_init,
        wlmtk_window_style_decode_fini),
    BSPL_DESC_CUSTOM(
        "Menu", true, wlmaker_config_style_t,
        menu_style_ptr, has_menu_style,
        wlmtk_menu_style_decode, NULL,
        wlmtk_menu_style_decode_init,
        wlmtk_menu_style_decode_fini),
    BSPL_DESC_DICT(
        "TaskList", true, wlmaker_config_style_t, task_list, task_list,
        wlmaker_task_list_style_desc),
    BSPL_DESC_DICT(
        "Clip", true, wlmaker_config_style_t, clip, clip,
        _wlmaker_clip_style_desc),
    BSPL_DESC_DICT(
        "Cursor", true, wlmaker_config_style_t, cursor, cursor,
        wlmim_cursor_style_desc),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bspl_object_t *wlmaker_config_object_load(
    wlmaker_files_t *files_ptr,
    const char *name_ptr,
    const char *arg_fname_ptr,
    const char *xdg_config_fname_ptr,
    const uint8_t *default_data_ptr,
    size_t default_data_size)
{
    char *fname_ptr = NULL;
    if (NULL == arg_fname_ptr && NULL != files_ptr) {
        fname_ptr = wlmaker_files_xdg_config_find(
            files_ptr, xdg_config_fname_ptr, S_IFREG);
    }

    bspl_object_t *object_ptr = _wlmaker_plist_load(
        name_ptr,
        NULL != arg_fname_ptr ? arg_fname_ptr : fname_ptr,
        default_data_ptr,
        default_data_size);
    if (NULL != fname_ptr) free(fname_ptr);
    return object_ptr;
}

/* ------------------------------------------------------------------------- */
bspl_dict_t *wlmaker_config_load(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr)
{
    return BS_ASSERT_NOTNULL(
        bspl_dict_from_object(
            wlmaker_config_object_load(
                files_ptr,
                "wlmaker config",
                fname_ptr,
                "Config.plist",
                embedded_binary_default_configuration_data,
                embedded_binary_default_configuration_size)));
}

/* ------------------------------------------------------------------------- */
bspl_dict_t *wlmaker_state_load(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr)
{
    return BS_ASSERT_NOTNULL(
        bspl_dict_from_object(
            wlmaker_config_object_load(
                files_ptr,
                "wlmaker state",
                fname_ptr,
                "State.plist",
                embedded_binary_default_state_data,
                embedded_binary_default_state_size)));
}

/* ------------------------------------------------------------------------- */
bool wlmaker_theme_load(
    wlmaker_files_t *files_ptr,
    const char *arg_fname_ptr,
    wlmaker_config_style_t *style_ptr)
{
    char *fname_ptr = NULL;
    if (NULL != arg_fname_ptr && NULL != files_ptr) {
        fname_ptr = wlmaker_files_xdg_data_find(
            files_ptr, "Themes/default.plist", S_IFREG);
    }

    bspl_object_t *o = _wlmaker_plist_load(
        "Theme",
        NULL != arg_fname_ptr ? arg_fname_ptr : fname_ptr,
        embedded_binary_theme_data,
        embedded_binary_theme_size);
    if (NULL != fname_ptr) free(fname_ptr);

    bspl_dict_t *style_dict_ptr = bspl_dict_from_object(o);
    if (NULL == style_dict_ptr) {
        bspl_object_unref(o);
        return NULL;
    }

    wlmaker_config_style_t tmp_style = {};
    bool rv = bspl_decode_dict(
        style_dict_ptr,
        wlmaker_config_style_desc,
        &tmp_style);
    bspl_dict_unref(style_dict_ptr);

    if (!rv || NULL == tmp_style.window_style_ptr) return false;

    bspl_decoded_destroy(wlmaker_config_style_desc, style_ptr);
    *style_ptr = tmp_style;
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Loads a plist object from the file or the default data. */
bspl_object_t *_wlmaker_plist_load(
    const char *name_ptr,
    const char *fname_ptr,
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
                   "Failed bspl_create_object_from_plist(\"%s\") for %s",
                   fname_ptr, name_ptr);
        }
        return object_ptr;
    }

    if (NULL == default_data_ptr) return NULL;

    bs_log(BS_INFO, "Using compiled-in data for %s plist.", name_ptr);
    return bspl_create_object_from_plist_data(
        default_data_ptr, default_data_size);
}

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

/* == Unit tests =========================================================== */

static void test_embedded(bs_test_t *test_ptr);
static void test_file(bs_test_t *test_ptr);
static void test_style_file(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmaker_config_test_cases[] = {
    { true, "embedded", test_embedded },
    { true, "file", test_file },
    { true, "style_file", test_style_file },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_config_test_set = BS_TEST_SET(
    true, "config", wlmaker_config_test_cases);

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
        embedded_binary_theme_data,
        embedded_binary_theme_size);
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
        WLMAKER_SOURCE_DIR "/etc/Config.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/ExampleConfig.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/HomeConfig.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);

    dict_ptr = _wlmaker_config_from_plist(
        WLMAKER_SOURCE_DIR "/etc/State.plist");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    bspl_dict_unref(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Loads and decodes the style file. */
void test_style_file(bs_test_t *test_ptr)
{
    wlmaker_config_style_t cs = {};

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif

    const char *f = WLMAKER_SOURCE_DIR "/share/Themes/Default.plist";
    BS_TEST_VERIFY_TRUE(test_ptr, wlmaker_theme_load(NULL, f, &cs));
    bspl_decoded_destroy(wlmaker_config_style_desc, &cs);

    f = WLMAKER_SOURCE_DIR "/share/Themes/Debian.plist";
    BS_TEST_VERIFY_TRUE(test_ptr, wlmaker_theme_load(NULL, f, &cs));
    bspl_decoded_destroy(wlmaker_config_style_desc, &cs);
}

/* == End of config.c ====================================================== */
