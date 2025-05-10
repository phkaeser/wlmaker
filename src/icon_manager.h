/* ========================================================================= */
/**
 * @file icon_manager.h
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
#ifndef __ICON_MANAGER_H__
#define __ICON_MANAGER_H__

struct wl_display;

/** Forward declaration: Icon Manager handle. */
typedef struct _wlmaker_icon_manager_t wlmaker_icon_manager_t;

/** Forward declaration: Toplevel icon handle. */
typedef struct _wlmaker_toplevel_icon_t wlmaker_toplevel_icon_t;

#include "server.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates an icon manager.
 *
 * @param wl_display_ptr
 * @param server_ptr
 *
 * @return The handle of the icon manager or NULL on error. Must be destroyed
 *     by calling @ref wlmaker_icon_manager_destroy.
 */
wlmaker_icon_manager_t *wlmaker_icon_manager_create(
    struct wl_display *wl_display_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the Icon Manager.
 *
 * @param icon_manager_ptr
 */
void wlmaker_icon_manager_destroy(
    wlmaker_icon_manager_t *icon_manager_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ICON_MANAGER_H__ */
/* == End of icon_manager.h ================================================ */
