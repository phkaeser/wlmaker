/* ========================================================================= */
/**
 * @file jetty.h
 *
 * Wayland Maker's "dock" application offers to organize docked tiles (such
 * as Dock Apps, iconified windoiws, launchers) on multiple areas on
 * configurable positions on the screen.
 * Sticking with marine terminology, we call one such area a "jetty".
 *
 * Towards the compositor, each jetty is a layer surface. Towards a Dock App,
 * each jetty is a separate output. These outputs are organized in disjoint
 * output layouts.
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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
#ifndef __WLMAKER_WLMDOCK_JETTY_H__
#define __WLMAKER_WLMDOCK_JETTY_H__

#include <toolkit/toolkit.h>

#include "wlclient/wlclient.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmdock_jetty;

struct wl_display;
struct wlmim_cursor_style;
struct wlr_allocator;
struct wlr_backend;
struct wlr_backend;
struct wlr_renderer;
struct zwlr_layer_shell_v1;

struct wlmdock_jetty *wlmdock_jetty_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    wlmcl_client_t *client_ptr,
    struct zwlr_layer_shell_v1 *zwlr_layer_shell_ptr,
    const struct wlmtk_dock_style *dock_style_ptr,
    struct wlmim_cursor_style *cursor_style_ptr,
    const wlmtk_dock_positioning_t *positioning_ptr);

void wlmdock_jetty_destroy(struct wlmdock_jetty *jetty_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_WLMDOCK_JETTY_H__
/* == End of jetty.h ======================================================= */
