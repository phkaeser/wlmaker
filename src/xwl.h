/* ========================================================================= */
/**
 * @file xwl.h
 *
 * Interface layer to Wlroots XWayland.
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
#ifndef __XWL_H__
#define __XWL_H__

/** Forward declaration: XWayland interface. */
typedef struct _wlmaker_xwl_t wlmaker_xwl_t;

#include "server.h"

#define WLR_USE_UNSTABLE
#include <wlr/xwayland.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** XCB Atom identifiers. */
typedef enum {
    NET_WM_WINDOW_TYPE_NORMAL,
    NET_WM_WINDOW_TYPE_DIALOG,
    NET_WM_WINDOW_TYPE_UTILITY,
    NET_WM_WINDOW_TYPE_TOOLBAR,
    NET_WM_WINDOW_TYPE_SPLASH,
    NET_WM_WINDOW_TYPE_MENU,
    NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    NET_WM_WINDOW_TYPE_POPUP_MENU,
    NET_WM_WINDOW_TYPE_TOOLTIP,
    NET_WM_WINDOW_TYPE_NOTIFICATION,

    // Sentinel element.
    XWL_MAX_ATOM_ID
} xwl_atom_identifier_t;

/**
 * Creates the XWayland interface.
 *
 * @param server_ptr
 *
 * @return NULL on error, or a pointer to a @ref wlmaker_xwl_t. Must be free-d
 *     by calling @ref wlmaker_xwl_destroy.
 */
wlmaker_xwl_t *wlmaker_xwl_create(wlmaker_server_t *server_ptr);

/**
 * Destroys the XWayland interface.
 *
 * @param xwl_ptr
 */
void wlmaker_xwl_destroy(wlmaker_xwl_t *xwl_ptr);

/**
 * Returns whether the XWayland surface has any of the window types.
 *
 * @param xwl_ptr
 * @param wlr_xwayland_surface_ptr
 * @param atom_identifiers    NULL-terminated set of window type we're looking
 *                            for.
 *
 * @return Whether `atom_identifiers` is in any of the window types.
 */
bool xwl_is_window_type(
    wlmaker_xwl_t *xwl_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    const xwl_atom_identifier_t *atom_identifiers);

/** Returns a human-readable name for the atom. */
const char *xwl_atom_name(
    wlmaker_xwl_t *xwl_ptr,
    xcb_atom_t atom);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XWL_H__ */
/* == End of xwl.h ========================================================= */
