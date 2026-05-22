/* ========================================================================= */
/**
 * @file launcher.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#ifndef __WLMAKER_DOCK_LAUNCHER_H__
#define __WLMAKER_DOCK_LAUNCHER_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>

#include "toolkit/toolkit.h"
#include "util/files.h"
#include "util/subprocess_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Launcher handle. */
typedef struct _wlmdock_launcher_t wlmdock_launcher_t;

/**
 * Creates an application launcher, configured from a plist dict.
 *
 * @param style_ptr
 * @param dict_ptr
 * @param monitor_ptr
 * @param files_ptr
 *
 * @return Pointer to the launcher handle or NULL on error.
 */
wlmdock_launcher_t *wlmdock_launcher_create_from_plist(
    const struct wlmtk_tile_style *style_ptr,
    bspl_dict_t *dict_ptr,
    wlm_util_subprocess_monitor_t *monitor_ptr,
    wlm_util_files_t *files_ptr);

/**
 * Destroys the application launcher.
 *
 * @param launcher_ptr
 */
void wlmdock_launcher_destroy(wlmdock_launcher_t *launcher_ptr);

/** @return A pointer to the @ref wlmtk_tile_t superclass of `launcher_ptr`. */
wlmtk_tile_t *wlmdock_launcher_tile(wlmdock_launcher_t *launcher_ptr);

/** Unit test set. */
extern const bs_test_set_t wlmdock_launcher_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_DOCK_LAUNCHER_H__ */
/* == End of launcher.h ==================================================== */
