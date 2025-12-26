/* ========================================================================= */
/**
 * @file gen_menu.h
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
#ifndef __WLMAKER_GEN_MENU_H__
#define __WLMAKER_GEN_MENU_H__

#include <libbase/plist.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Generates a menu and returns the plist structure for it.
 *
 * @param locale_ptr          The locale for LC_MESSAGES, or NULL.
 * @param path_ptr
 *
 * @return The plist array, or NULL on error.
 */
bspl_array_t *wlmaker_menu_generate(
    const char *locale_ptr,
    const char *path_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_GEN_MENU_H__
/* == End of gen_menu.h ==================================================== */
