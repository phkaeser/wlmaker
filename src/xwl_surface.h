/* ========================================================================= */
/**
 * @file xwl_surface.h
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
#ifndef __XWL_SURFACE_H__
#define __XWL_SURFACE_H__

#if defined(WLMAKER_HAVE_XWAYLAND)

#include <libbase/libbase.h>

#include "server.h"
#include "xwl.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration. */
struct wlr_xwayland_surface;

/** XWayland window (surface) state. */
typedef struct _wlmaker_xwl_surface_t wlmaker_xwl_surface_t;

/**
 * Creates an XWayland window. Technically, window surface.
 *
 * @param wlr_xwayland_surface_ptr
 * @param xwl_ptr
 * @param server_ptr
 *
 * @return Pointer to a @ref wlmaker_xwl_surface_t.
 */
wlmaker_xwl_surface_t *wlmaker_xwl_surface_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_xwl_t *xwl_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the XWayland window (surface).
 *
 * @param xwl_surface_ptr
 */
void wlmaker_xwl_surface_destroy(wlmaker_xwl_surface_t *xwl_surface_ptr);

/** Unit tests for XWL surface. */
extern const bs_test_case_t wlmaker_xwl_surface_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
#endif /* __XWL_SURFACE_H__ */
/* == End of xwl_surface.h ================================================= */
