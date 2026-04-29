/* ========================================================================= */
/**
 * @file menu.h
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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
#ifndef __WLMAKER_MENU_H__
#define __WLMAKER_MENU_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Generates a menu for the Themes files found at XDG data directories.
 *
 * @param path_ptr            Optional: Path to read from. If NULL, use the XDG
 *                            data directories.
 *
 * @return A Plist array, or NULL on error.
 */
bspl_array_t *wlmtool_menu_generate_appearance(const char *path_ptr);

/** Unit tests for the menu generator. */
extern const bs_test_set_t wlmtool_menu_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_MENU_H__
/* == End of menu.h ======================================================== */
