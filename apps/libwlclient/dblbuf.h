/* ========================================================================= */
/**
 * @file dblbuf.h
 *
 * Functions for working with a double buffer on a wayland surface.
 *
 * @copyright
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
#ifndef __WLCL_DBLBUF_H__
#define __WLCL_DBLBUF_H__

#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wl_buffer;
struct wl_shm;
struct wl_surface;

/** Forward declaration: Double buffer state. */
typedef struct _wlcl_dblbuf_t wlcl_dblbuf_t;

/** Callback that indicates the buffer is ready to draw into. */
typedef bool (*wlcl_dblbuf_ready_callback_t)(
    bs_gfxbuf_t *gfxbuf_ptr,
    void *ud_ptr);

/**
 * Creates a double buffer for the surface with provided dimensions.
 *
 * @param wl_surface_ptr
 * @param wl_shm_ptr
 * @param width
 * @param height
 *
 * @return Pointer to state of the double buffer, or NULL on error. Call
 *     @ref wlcl_dblbuf_destroy for freeing up the associated resources.
 */
wlcl_dblbuf_t *wlcl_dblbuf_create(
    struct wl_surface *wl_surface_ptr,
    struct wl_shm *wl_shm_ptr,
    unsigned width,
    unsigned height);

/** Destroys the double buffer. */
void wlcl_dblbuf_destroy(wlcl_dblbuf_t *dblbuf_ptr);

/**
 * Registers a callback for when a frame can be drawn into the buffer.
 *
 * The frame can be drawn if (1) it is due, and (2) there is a back buffer
 * available ("released") for drawing into. If these conditions hold true,
 * `callback` will be called right away. Otherwise, it will be called once
 * these conditions are fulfilled.
 *
 * The callback will be called only once. If the client wishes further
 * notifications, they must call @ref wlcl_dblbuf_register_ready_callback
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
void wlcl_dblbuf_register_ready_callback(
    wlcl_dblbuf_t *dblbuf_ptr,
    wlcl_dblbuf_ready_callback_t callback,
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLCL_DBLBUF_H__ */
/* == End of dblbuf_buffer.h =============================================== */
