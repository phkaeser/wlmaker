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
#include <libbase/plist.h>
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
    /** Will set all windows to CLIENT, no matter what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_CLIENT,
    /** Will set all windows to SERVER, no matter what they requested. */
    WLMAKER_CONFIG_DECORATION_ENFORCE_SERVER
} wlmaker_config_decoration_t;

/** Style of the task list overlay. */
typedef struct {
    /** Fill style. */
    wlmtk_style_fill_t        fill;
    /** Font to use. */
    wlmtk_style_font_t        font;
    /** Text color for tasks listed. */
    uint32_t                  text_color;
} wlmaker_config_task_list_style_t;

/** Style of the clip's configurables. */
typedef struct {
    /** Font to use. */
    wlmtk_style_font_t        font;
    /** Text color for tasks listed. */
    uint32_t                  text_color;
} wlmaker_config_clip_style_t;

/** Style of the cursor. */
typedef struct {
    /** Name of the XCursor theme to use. */
    char                      *name_ptr;
    /** Size, when non-scaled. */
    uint64_t                  size;
} wlmaker_config_cursor_style_t;

/** Style information. */
typedef struct {
    /** Background color, unless overriden in "Workspace" state. */
    uint32_t                  background_color;
    /** The tile. */
    wlmtk_tile_style_t        tile;
    /** Dock optics: Margin. */
    wlmtk_dock_style_t        dock;
    /** Window style. */
    wlmtk_window_style_t      window;
    /** Menu style. */
    wlmtk_menu_style_t        menu;
    /** Clip style. */
    wlmaker_config_clip_style_t clip;
    /** Task list style. */
    wlmaker_config_task_list_style_t task_list;
    /** Cursor style. */
    wlmaker_config_cursor_style_t cursor;
} wlmaker_config_style_t;

extern const float config_output_scale;

/**
 * Loads a plist object from the given file(s) or data.
 *
 * Useful to load configuration files from (1) the expclitly provided name in
 * `fname_ptr`, (2) any of the files listed in `fname_defaults`, or (3)
 * an in-memory buffer, as a compiled-in fallback option.
 *
 * @param name_ptr            Name to use when logging about the plist.
 * @param fname_ptr           Explicit filename to use for loading the file,
 *                            eg. from the commandline. Or NULL.
 * @param fname_defaults      NULL-terminated list of pointers to default
 *                            paths & filenames to use for searching plist
 *                            file(s).
 * @param default_data_ptr    Points to in-memory plist data, or NULL.
 *                            Will be used if fname_ptr was NULL and no
 *                            file was found at fname_defaults.
 * @param default_data_size   The size of the in-memory plist data.
 *
 * @returns a bspl_object_t on success, or NULL if none of the options had
 *     data, or if there was a file or parsing error.
 */
bspl_object_t *wlmaker_plist_load(
    const char *name_ptr,
    const char *fname_ptr,
    const char **fname_defaults,
    const uint8_t *default_data_ptr,
    size_t default_data_size);

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
 *      bspl_object_unref().
 */
bspl_dict_t *wlmaker_config_load(const char *fname_ptr);

/**
 * Loads the state for wlmaker.
 *
 * Behaviour is similar to @ref wlmaker_config_load.
 *
 * @param fname_ptr
 *
 * @return A dict object or NULL on error.
 */
bspl_dict_t *wlmaker_state_load(const char *fname_ptr);

extern const bspl_desc_t wlmaker_config_style_desc[];

/** Unit test cases. */
extern const bs_test_case_t wlmaker_config_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CONFIG_H__ */
/* == End of config.h ====================================================== */
