/* ========================================================================= */
/**
 * @file launcher.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __LAUNCHER_H__
#define __LAUNCHER_H__

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Launcher handle. */
typedef struct _wlmaker_launcher_t wlmaker_launcher_t;

/**
 * Creates an application launcher.
 *
 * @param server_ptr
 * @param style_ptr
 *
 * @return Pointer to the launcher handle or NULL on error.
 */
wlmaker_launcher_t *wlmaker_launcher_create(
    wlmaker_server_t *server_ptr,
    const wlmtk_tile_style_t *style_ptr);

/**
 * Creates an application launcher, configured from a plist dict.
 *
 * @param server_ptr
 * @param style_ptr
 * @param dict_ptr
 *
 * @return Pointer to the launcher handle or NULL on error.
 */
wlmaker_launcher_t *wlmaker_launcher_create_from_plist(
    wlmaker_server_t *server_ptr,
    const wlmtk_tile_style_t *style_ptr,
    wlmcfg_dict_t *dict_ptr);

/**
 * Destroys the application launcher.
 *
 * @param launcher_ptr
 */
void wlmaker_launcher_destroy(wlmaker_launcher_t *launcher_ptr);

/** @return A pointer to the @ref wlmtk_tile_t superclass of `launcher_ptr`. */
wlmtk_tile_t *wlmaker_launcher_tile(wlmaker_launcher_t *launcher_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmaker_launcher_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LAUNCHER_H__ */
/* == End of launcher.h ==================================================== */
