/* ========================================================================= */
/**
 * @file icon.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#ifndef __WLMAKER_WLCLIENT_ICON_H__
#define __WLMAKER_WLCLIENT_ICON_H__

#include <stdbool.h>
#include <stdint.h>

#include "wlclient.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration of an icon's state. */
typedef struct _wlmcl_icon_t wlmcl_icon_t;

/**
 * Creates an icon.
 *
 * @param wlclient_ptr
 *
 * @return An icon state or NULL on error. The state must be free'd by calling
 *     @ref wlmcl_icon_destroy.
 */
wlmcl_icon_t *wlmcl_icon_create(
    wlmcl_client_t *wlclient_ptr);

/**
 * Destroys the icon.
 *
 * @param icon_ptr
 */
void wlmcl_icon_destroy(
    wlmcl_icon_t *icon_ptr);

/**
 * Returns whether the icon protocol is supported on the client.
 *
 * @param wlclient_ptr
 */
bool wlmcl_icon_supported(wlmcl_client_t *wlclient_ptr);


/**
 * Returns the underlying Wayland surface of the icon.
 *
 * @param icon_ptr
 *
 * @return The wl_surface pointer.
 */
struct wl_surface *wlmcl_icon_wl_surface(
    wlmcl_icon_t *icon_ptr);

/**
 * Registers the callback to notify when the icon size is determined or updated.
 *
 * @param icon_ptr
 * @param callback
 * @param ud_ptr
 */
void wlmcl_icon_register_configure_callback(
    wlmcl_icon_t *icon_ptr,
    void (*callback)(void *ud_ptr, uint32_t width, uint32_t height),
    void *ud_ptr);

/**
 * Registers the callback to notify the pointer position relative to the
 * icon's surface.
 *
 * @param icon_ptr
 * @param callback
 * @param callback_ud_ptr
 */
void wlmcl_icon_register_position_callback(
    wlmcl_icon_t *icon_ptr,
    void (*callback)(double x, double y, void *ud_ptr),
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_ICON_H__ */
/* == End of icon.h ======================================================== */
