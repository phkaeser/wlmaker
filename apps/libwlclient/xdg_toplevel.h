/* ========================================================================= */
/**
 * @file xdg_toplevel.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __LIBWLCLIENT_XDG_TOPLEVEL_H__
#define __LIBWLCLIENT_XDG_TOPLEVEL_H__

#include <libbase/libbase.h>
#include <stdbool.h>

#include "libwlclient.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of the toplevel. */
typedef struct _wlclient_xdg_toplevel_t wlclient_xdg_toplevel_t;

/**
 * Creates a XDG toplevel.
 *
 * @param wlclient_ptr
 * @param title_ptr
 * @param width
 * @param height
 *
 * @return State of the toplevel or NULL on error.
 */
wlclient_xdg_toplevel_t *wlclient_xdg_toplevel_create(
    wlclient_t *wlclient_ptr,
    const char *title_ptr,
    unsigned width,
    unsigned height);

/**
 * Destroys the XDG toplevel.
 *
 * @param toplevel_ptr
 */
void wlclient_xdg_toplevel_destroy(wlclient_xdg_toplevel_t *toplevel_ptr);

/**
 * Returns whether the XDG shell protocol is supported on the client.
 *
 * @param wlclient_ptr
 */
bool wlclient_xdg_supported(wlclient_t *wlclient_ptr);

/**
 * Registers the callback to notify when the buffer is ready to draw into.
 *
 * @param toplevel_ptr
 * @param callback
 * @param callback_ud_ptr
 */
void wlclient_xdg_toplevel_register_ready_callback(
    wlclient_xdg_toplevel_t *toplevel_ptr,
    bool (*callback)(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr),
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LIBWLCLIENT_XDG_TOPLEVEL_H__ */
/* == End of xdg_toplevel.h ================================================== */
