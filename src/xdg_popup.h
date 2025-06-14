/* ========================================================================= */
/**
 * @file xdg_popup.h
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
#ifndef __XDG_POPUP_H__
#define __XDG_POPUP_H__

#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

struct wlr_seat;
struct wlr_xdg_popup;

struct _wlmaker_xdg_popup_t;
/** Forward declaration: State of the toolkit's XDG popup. */
typedef struct _wlmaker_xdg_popup_t wlmaker_xdg_popup_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of toolkit popup. */
struct _wlmaker_xdg_popup_t {
    /** Super class: popup. */
    wlmtk_popup_t             super_popup;

    /** Seat. */
    struct wlr_seat           *wlr_seat_ptr;

    /** Surface of the popup. */
    wlmtk_surface_t           *surface_ptr;
    /** The WLR popup. */
    struct wlr_xdg_popup      *wlr_xdg_popup_ptr;

    /** Listener for the `reposition` signal of `wlr_xdg_popup::events` */
    struct wl_listener        reposition_listener;
    /** Listener for the `destroy` signal of `wlr_xdg_surface::events`. */
    struct wl_listener        destroy_listener;
    /** Listener for the `new_popup` signal of `wlr_xdg_surface::events`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `commit` signal of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;
};

/**
 * Creates a popup.
 *
 * @return Popup handle or NULL on error.
 */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    struct wlr_seat *wlr_seat_ptr);

/**
 * Destroys the popup.
 *
 * @param wlmaker_xdg_popup_ptr
 */
void wlmaker_xdg_popup_destroy(
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_POPUP_H__ */
/* == End of xdg_popup.h =================================================== */
