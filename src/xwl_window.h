/* ========================================================================= */
/**
 * @file xwl_window.h
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
#ifndef __XWL_WINDOW_H__
#define __XWL_WINDOW_H__

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration. */
struct wlr_xwayland_surface;

/** XWayland window (content) state. */
typedef struct _wlmaker_xwl_window_t wlmaker_xwl_window_t;

/**
 * Creates an XWayland window. Technically, window content.
 *
 * @param wlr_xwayland_surface_ptr
 * @param server_ptr
 *
 * @return Pointer to a @ref wlmaker_xwl_window_t.
 */
wlmaker_xwl_window_t *wlmaker_xwl_window_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the XWayland window (content).
 *
 * @param xwl_window_ptr
 */
void wlmaker_xwl_window_destroy(wlmaker_xwl_window_t *xwl_window_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XWL_WINDOW_H__ */
/* == End of xwl_window.h ================================================== */
