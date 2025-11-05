/* ========================================================================= */
/**
 * @file xdg_toplevel.h
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
#ifndef __XDG_TOPLEVEL_H__
#define __XDG_TOPLEVEL_H__

#include <stdbool.h>

#include "server.h"

struct wlr_xdg_toplevel;
struct wlmaker_xdg_toplevel;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates an XDG toplevel.
 *
 * @param wlr_xdg_toplevel_ptr
 * @param server_ptr
 *
 * @return wlmaker's toplevel handle or NULL on error.
 */
struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_create(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr);

/** Dtor for the XDG toplevel. */
void wlmaker_xdg_toplevel_destroy(
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr);

/** En-/Disables server-side decoration for the XDG toplevel. */
void wlmaker_xdg_toplevel_set_server_side_decorated(
    struct wlmaker_xdg_toplevel *wlmaker_xdg_toplevel_ptr,
    bool server_side_decorated);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_TOPLEVEL_H__ */
/* == End of xdg_toplevel.h ================================================ */
