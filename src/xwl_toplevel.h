/* ========================================================================= */
/**
 * @file xwl_toplevel.h
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
#ifndef __XWL_TOPLEVEL_H__
#define __XWL_TOPLEVEL_H__
#if defined(WLMAKER_HAVE_XWAYLAND)

#include <stdbool.h>

#include "server.h"
#include "toolkit/toolkit.h"
#include "xwl_content.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration. */
typedef struct _wlmaker_xwl_toplevel_t wlmaker_xwl_toplevel_t;

/**
 * Creates a toplevel XWayland window.
 *
 * @param content_ptr
 * @param server_ptr
 * @param env_ptr
 */
wlmaker_xwl_toplevel_t *wlmaker_xwl_toplevel_create(
    wlmaker_xwl_content_t *content_ptr,
    wlmaker_server_t *server_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the toplevel XWayland window.
 *
 * @param xwl_toplevel_ptr
 */
void wlmaker_xwl_toplevel_destroy(
    wlmaker_xwl_toplevel_t *xwl_toplevel_ptr);

/**
 * Sets decoration for the toplevel window.
 *
 * @param xwl_toplevel_ptr
 * @param decorated
 */
void wlmaker_xwl_toplevel_set_decorations(
    wlmaker_xwl_toplevel_t *xwl_toplevel_ptr,
    bool decorated);

/** Accessor: Exposes @ref wlmtk_window_t. */
wlmtk_window_t *wlmtk_window_from_xwl_toplevel(
    wlmaker_xwl_toplevel_t *xwl_toplevel_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
#endif /* __XWL_TOPLEVEL_H__ */
/* == End of xwl_toplevel.h ================================================== */
