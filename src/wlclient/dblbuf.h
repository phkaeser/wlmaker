/* ========================================================================= */
/**
 * @file dblbuf.h
 *
 * Functions for working with a double buffer on a wayland surface.
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright 2025 Google LLC
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
#ifndef __WLMAKER_WLCLIENT_DBLBUF_H__
#define __WLMAKER_WLCLIENT_DBLBUF_H__

#include <libbase/libbase.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wl_shm;
struct wl_surface;

/** Forward declaration: Double buffer state. */
typedef struct _wlmcl_dblbuf_t wlmcl_dblbuf_t;

/** Callback that indicates the buffer is ready to draw into. */
typedef bool (*wlmcl_dblbuf_ready_callback_t)(
    bs_gfxbuf_t *gfxbuf_ptr,
    void *ud_ptr);

/**
 * Creates a double buffer for the surface with provided dimensions.
 *
 * @param app_id_ptr          Application ID, used to prefix the name of the
 *                            shared memory object. Can be NULL.
 * @param wl_surface_ptr
 * @param wl_shm_ptr
 * @param width
 * @param height
 *
 * @return Pointer to state of the double buffer, or NULL on error. Call
 *     @ref wlmcl_dblbuf_destroy for freeing up the associated resources.
 */
wlmcl_dblbuf_t *wlmcl_dblbuf_create(
    const char *app_id_ptr,
    struct wl_surface *wl_surface_ptr,
    struct wl_shm *wl_shm_ptr,
    unsigned width,
    unsigned height);

/** Destroys the double buffer. */
void wlmcl_dblbuf_destroy(wlmcl_dblbuf_t *dblbuf_ptr);

/**
 * Registers a callback for when a frame can be drawn into the buffer.
 *
 * The frame can be drawn if (1) it is due, and (2) there is a back buffer
 * available ("released") for drawing into. If these conditions hold true,
 * `callback` will be called right away. Otherwise, it will be called once
 * these conditions are fulfilled.
 *
 * The callback will be called only once. If the client wishes further
 * notifications, they must call @ref wlmcl_dblbuf_register_ready_callback
 * again.
 *
 * The callback must be registered only after the surface is ready. Eg. for
 * an XDG toplevel, after it has received & acknowledged `configure`.
 *
 * @param dblbuf_ptr
 * @param callback            The callback function, or NULL to clear the
 *                            callback.
 * @param callback_ud_ptr     Argument to use for `callback`.
 */
void wlmcl_dblbuf_register_ready_callback(
    wlmcl_dblbuf_t *dblbuf_ptr,
    wlmcl_dblbuf_ready_callback_t callback,
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_DBLBUF_H__ */
/* == End of dblbuf.h ====================================================== */
