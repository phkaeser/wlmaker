/* ========================================================================= */
/**
 * @file xdg_shell.h
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
#ifndef __XDG_SHELL_H__
#define __XDG_SHELL_H__

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

/** Handle for XDG Shell server handler. */
typedef struct _wlmaker_xdg_shell_t wlmaker_xdg_shell_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Handle for XDG Shell server handler. */
struct _wlmaker_xdg_shell_t {
    /** Back-link to the server this XDG Shell belongs to. */
    wlmaker_server_t          *server_ptr;

    /** XDG Shell handler. */
    struct wlr_xdg_shell      *wlr_xdg_shell_ptr;

    /** Listener for the `new_surface` signal raised by `wlr_xdg_shell`. */
    struct wl_listener        new_surface_listener;

    /** Listener for the `new_toplevel` signal raised by `wlr_xdg_shell`. */
    struct wl_listener        new_toplevel_listener;
    /** Listener for the `new_popup` signal raised by `wlr_xdg_shell`. */
    struct wl_listener        new_popup_listener;
    /** Listener for the `destroy` signal raised by `wlr_xdg_shell`. */
    struct wl_listener        destroy_listener;
};

/**
 * Creates an XDG shell server handler.
 *
 * @param server_ptr
 *
 * @return The XDG Shell server handler or NULL on error.
 */
wlmaker_xdg_shell_t *wlmaker_xdg_shell_create(wlmaker_server_t *server_ptr);

/**
 * Destroys the XDG Shell server handler.
 *
 * @param xdg_shell_ptr
 */
void wlmaker_xdg_shell_destroy(wlmaker_xdg_shell_t *xdg_shell_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_SHELL_H__ */
/* == End of xdg_shell.h =================================================== */
