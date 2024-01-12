/* ========================================================================= */
/**
 * @file buffer.h
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
#ifndef __WLCLIENT_BUFFER_H__
#define __WLCLIENT_BUFFER_H__

#include "libwlclient.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration of the buffer state. */
typedef struct _wlclient_buffer_t wlclient_buffer_t;

/** Forward declaration of a wayland surface. */
struct wl_surface;

/** Callback to report that a buffer is ready to draw into. */
typedef void (*wlclient_buffer_ready_callback_t)(void *ud_ptr);

/**
 * Creates a wlclient wayland buffer with the given dimensions.
 *
 * @param wlclient_ptr
 * @param width
 * @param height
 * @param ready_callback
 * @param ready_callback_ud_ptr
 *
 * @return A pointer to the created client buffer, or NULL on error. The
 *     buffer must be destroyed by calling @ref wlclient_buffer_destroy.
 */
wlclient_buffer_t *wlclient_buffer_create(
    const wlclient_t *wlclient_ptr,
    unsigned width,
    unsigned height,
    wlclient_buffer_ready_callback_t ready_callback,
    void *ready_callback_ud_ptr);

/**
 * Destroys the wlclient wayland buffer.
 *
 * @param buffer_ptr
 */
void wlclient_buffer_destroy(
    wlclient_buffer_t *buffer_ptr);

/**
 * Attaches the buffer to the surface and commits it.
 *
 * @param buffer_ptr
 * @param wl_surface_ptr
 */
void wlclient_buffer_attach_to_surface_and_commit(
    wlclient_buffer_t *buffer_ptr,
    struct wl_surface *wl_surface_ptr);

/**
 * Returns the`bs_gfxbuf_t` corresponding to the client buffer.
 *
 * @param buffer_ptr
 *
 * @return Pointer to the `bs_gfxbuf_t`. The `bs_gfxbuf_t` remains valid
 *     throughout the lifetime of buffer_ptr, and does not need to be
 *     released by the caller.
 */
bs_gfxbuf_t *bs_gfxbuf_from_wlclient_buffer(
    wlclient_buffer_t *buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLCLIENT_BUFFER_H__ */
/* == End of buffer.h ====================================================== */
