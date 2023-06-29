/* ========================================================================= */
/**
 * @file toplevel_icon_manager.h
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
#ifndef __TOPLEVEL_ICON_MANAGER_H__
#define __TOPLEVEL_ICON_MANAGER_H__

#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward definition: Toplevel Icon Manager handle. */
typedef struct _wlmaker_toplevel_icon_manager_t
wlmaker_toplevel_icon_manager_t;

/**
 * Creates a toplevel icon manager.
 *
 * @param wl_display_ptr
 *
 * @return The handle of the toplevel icon manager or NULL on error. Must be
 *     destroyed by calling @ref wlmaker_toplevel_icon_manager_destroy.
 */
wlmaker_toplevel_icon_manager_t *wlmaker_toplevel_icon_manager_create(
    struct wl_display *wl_display_ptr);

/**
 * Destroys the Toplevel Icon Manager.
 *
 * @param toplevel_icon_manager_ptr
 */
void wlmaker_toplevel_icon_manager_destroy(
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TOPLEVEL_ICON_MANAGER_H__ */
/* == End of toplevel_icon_manager.h ======================================= */
