/* ========================================================================= */
/**
 * @file xdg_toplevel.h
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
#ifndef __WLMAKER_WLCLIENT_XDG_TOPLEVEL_H__
#define __WLMAKER_WLCLIENT_XDG_TOPLEVEL_H__

#include <stdbool.h>
#include <stdint.h>

#include "wlclient.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of the toplevel. */
typedef struct _wlmcl_xdg_toplevel_t wlmcl_xdg_toplevel_t;

/**
 * Creates a XDG toplevel.
 *
 * @param wlclient_ptr
 * @param title_ptr
 * @param width
 * @param height
 *
 * @return State of the toplevel or NULL on error.
 */
wlmcl_xdg_toplevel_t *wlmcl_xdg_toplevel_create(
    wlmcl_client_t *wlclient_ptr,
    const char *title_ptr,
    unsigned width,
    unsigned height);

/**
 * Destroys the XDG toplevel.
 *
 * @param toplevel_ptr
 */
void wlmcl_xdg_toplevel_destroy(wlmcl_xdg_toplevel_t *toplevel_ptr);

/**
 * Returns whether the XDG shell protocol is supported on the client.
 *
 * @param wlclient_ptr
 */
bool wlmcl_xdg_supported(wlmcl_client_t *wlclient_ptr);

/**
 * Sets XDG decoration mode to "server side".
 *
 * @param toplevel_ptr
 * @param enabled             Whether to enable server-side decoration. If
 *                            false, will set client-side decoration.
 *
 * @return true if the XDG decoration protocol is supported.
 */
bool wlmcl_xdg_decoration_set_server_side(
    wlmcl_xdg_toplevel_t *toplevel_ptr,
    bool enabled);

/**
 * Returns the underlying Wayland surface of the XDG toplevel.
 *
 * @param toplevel_ptr
 *
 * @return The wl_surface pointer.
 */
struct wl_surface *wlmcl_xdg_toplevel_wl_surface(
    wlmcl_xdg_toplevel_t *toplevel_ptr);

/**
 * Registers the callback to notify when the toplevel's layout/size changes.
 *
 * @param toplevel_ptr
 * @param callback
 * @param ud_ptr
 */
void wlmcl_xdg_toplevel_register_configure_callback(
    wlmcl_xdg_toplevel_t *toplevel_ptr,
    void (*callback)(void *ud_ptr, uint32_t width, uint32_t height),
    void *ud_ptr);

/**
 * Registers the callback to notify the pointer position relative to the
 * toplevel's surface.
 *
 * @param toplevel_ptr
 * @param callback
 * @param callback_ud_ptr
 */
void wlmcl_xdg_toplevel_register_position_callback(
    wlmcl_xdg_toplevel_t *toplevel_ptr,
    void (*callback)(double x, double y, void *ud_ptr),
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_XDG_TOPLEVEL_H__ */
/* == End of xdg_toplevel.h ================================================== */
