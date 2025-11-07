/* ========================================================================= */
/**
 * @file files.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMAKER_FILES_H__
#define __WLMAKER_FILES_H__

#include <libbase/libbase.h>

/** Handle for files module. */
typedef struct _wlmaker_files_t wlmaker_files_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Create the files module.
 *
 * @param dirname_ptr         Name of a subdirectory, that will be used as a
 *                            component for each of the XDG paths.
 *
 * @return Pointer to the module handle. Must be freed by calling
 *     @ref wlmaker_files_destroy.
 */
wlmaker_files_t *wlmaker_files_create(const char *dirname_ptr);

/**
 * Destroys the file module.
 *
 * @param files_ptr
 */
void wlmaker_files_destroy(wlmaker_files_t *files_ptr);

/**
 * Returns a full path name for a config file.
 *
 * This expands into ${XDG_CONFIG_HOME}/<dirname>/<fname>.
 *
 * @param files_ptr
 * @param fname_ptr
 *
 * @return Full path name, or NULL on error.
 */
char *wlmaker_files_xdg_config_fname(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr);

/** Unit test cases for @ref wlmaker_files_t. */
extern const bs_test_case_t wlmaker_files_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_FILES_H__ */
/* == End of files.h ======================================================= */
