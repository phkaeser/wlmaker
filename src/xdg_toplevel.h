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

#include "xdg_shell.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Type of XDG toplevel surface state. */
typedef struct _wlmaker_xdg_toplevel_t wlmaker_xdg_toplevel_t;

/**
 * Creates a XDG toplevel state.
 *
 * @param xdg_shell_ptr
 * @param wlr_xdg_surface_ptr
 *
 * @return the XDG surface state or NULL on error.
 */
wlmaker_xdg_toplevel_t *wlmaker_xdg_toplevel_create(
    wlmaker_xdg_shell_t *xdg_shell_ptr,
    struct wlr_xdg_surface *wlr_xdg_surface_ptr);

/**
 * Destroys the XDG surface state.
 *
 * @param xdg_toplevel_ptr
 */
void wlmaker_xdg_toplevel_destroy(wlmaker_xdg_toplevel_t *xdg_toplevel_ptr);

/** Content for XDG toplvel. */
typedef struct _wlmtk_xdg_toplevel_content_t wlmtk_xdg_toplevel_content_t;

/**
 * Creates a `wlmtk_content` for the given XDG surface.
 *
 * @param wlr_xdg_surface_ptr
 * @param server_ptr
 *
 * @return Pointer to the content.
 */
wlmtk_xdg_toplevel_content_t *wlmtk_xdg_toplevel_content_create(
    struct wlr_xdg_surface *wlr_xdg_surface_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the toplevel content.
 *
 * @param xdg_tl_content_ptr
 */
void wlmtk_xdg_toplevel_content_destroy(
    wlmtk_xdg_toplevel_content_t *xdg_tl_content_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_TOPLEVEL_H__ */
/* == End of xdg_toplevel.h ================================================= */
