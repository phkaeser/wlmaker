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

#include "server.h"
#include "toolkit/toolkit.h"

struct wlr_xdg_toplevel;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a toolkit window with the XDG surface as content.
 *
 * @param wlr_xdg_toplevel_ptr
 * @param server_ptr
 *
 * @return The window, or NULL on error.
 */
wlmtk_window_t *wlmtk_window_create_from_xdg_toplevel(
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    wlmaker_server_t *server_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_TOPLEVEL_H__ */
/* == End of xdg_toplevel.h ================================================ */
