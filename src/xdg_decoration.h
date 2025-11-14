/* ========================================================================= */
/**
 * @file xdg_decoration.h
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
 */
#ifndef __XDG_DECORATION_H__
#define __XDG_DECORATION_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>

struct wl_display;

/** The decoration manager handle. */
typedef struct _wlmaker_xdg_decoration_manager_t wlmaker_xdg_decoration_manager_t;

#include "server.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a new XDG decoration manager.
 *
 * @param wl_display_ptr
 * @param config_dict_ptr
 *
 * @return A decoration manager handle or NULL on error.
 */
wlmaker_xdg_decoration_manager_t *wlmaker_xdg_decoration_manager_create(
    struct wl_display *wl_display_ptr,
    bspl_dict_t *config_dict_ptr);

/**
 * Destroys the XDG decoration manager.
 *
 * @param decoration_manager_ptr
 */
void wlmaker_xdg_decoration_manager_destroy(
    wlmaker_xdg_decoration_manager_t *decoration_manager_ptr);

/** Unit test set. */
extern const bs_test_set_t wlmaker_xdg_decoration_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_DECORATION_H__ */
/* == End of xdg_decoration.h ============================================== */
