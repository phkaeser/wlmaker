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
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stddef.h>

#include "files.h"
#include "input/cursor.h"
#include "task_list.h"  // IWYU pragma: keep
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

/** Style of the clip's configurables. */
typedef struct {
    /** Font to use. */
    wlmtk_style_font_t        font;
    /** Text color for tasks listed. */
    uint32_t                  text_color;
} wlmaker_config_clip_style_t;

/** Style information. */
typedef struct {
    /** Background color, unless overriden in "Workspace" state. */
    uint32_t                  background_color;
    /** The tile. */
    wlmtk_tile_style_t        tile;
    /** Dock optics: Margin. */
    wlmtk_dock_style_t        dock;
    /** Window style. */
    struct wlmtk_window_style *window_style_ptr;
    /** Presence flag for window style. */
    bool                      has_window_style;
    /** Menu style. */
    struct wlmtk_menu_style *menu_style_ptr;
    /** Presence flag for menu style. */
    bool                      has_menu_style;
    /** Clip style. */
    wlmaker_config_clip_style_t clip;
    /** Task list style. */
    struct wlmaker_task_list_style task_list;
    /** Cursor style. */
    struct wlmim_cursor_style cursor;
} wlmaker_config_style_t;

/**
 * Loads a plist object from the given config file or data.
 *
 * Useful to load configuration files from the provided name in `fname_ptr` or
 * an in-memory buffer, as a compiled-in fallback option.
 *
 * @param files_ptr
 * @param name_ptr            Name to use when logging about the plist.
 * @param arg_fname_ptr       Explicit filename to use for loading the file,
 *                            eg. from the commandline. Or NULL.
 * @param xdg_config_fname_ptr File name relative to XDG config home. See
 *                            @ref wlmaker_files_xdg_config_find.
 * @param default_data_ptr    Points to in-memory plist data, or NULL. Will be
 *                            used if fname_ptr was NULL.
 * @param default_data_size   The size of the in-memory plist data.
 *
 * @returns a bspl_object_t on success, or NULL if none of the options had
 *     data, or if there was a file or parsing error.
 */
bspl_object_t *wlmaker_config_object_load(
    wlmaker_files_t *files_ptr,
    const char *name_ptr,
    const char *arg_fname_ptr,
    const char *xdg_config_fname_ptr,
    const uint8_t *default_data_ptr,
    size_t default_data_size);

/**
 * Loads the configuration for wlmaker.
 *
 * @param files_ptr
 * @param fname_ptr           Optional: Name of the file to load it from. May
 *                            be NULL.
 *
 * @return A dict object, or NULL on error. Errors will already be logged.
 *     The caller must free the associated resources by calling
 *      bspl_object_unref().
 */
bspl_dict_t *wlmaker_config_load(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr);

/**
 * Loads the state for wlmaker.
 *
 * Behaviour is similar to @ref wlmaker_config_load.
 *
 * @param files_ptr
 * @param fname_ptr
 *
 * @return A dict object or NULL on error.
 */
bspl_dict_t *wlmaker_state_load(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr);

/**
 * Loads the theme file for wlmaker, stores into `style_ptr`.
 *
 * @param files_ptr
 * @param fname_ptr           File to load from. If NULL, will load the defualt
 *                            style.
 * @param style_ptr           Points to the style. If loading and decoding
 *                            succeeds, resources at `style_ptr` will be freed.
 *                            `style_ptr` is untouched on failure.
 *
 * @return true on success.
 */
bool wlmaker_theme_load(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr,
    wlmaker_config_style_t *style_ptr);

/** Releases resources associated with the style. */
void wlmaker_config_style_release(wlmaker_config_style_t *style_ptr);

extern const bspl_desc_t wlmaker_config_style_desc[];

/** Unit test set. */
extern const bs_test_set_t wlmaker_config_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CONFIG_H__ */
/* == End of config.h ====================================================== */
