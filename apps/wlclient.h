/* ========================================================================= */
/**
 * @file wlclient.h
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
#ifndef __WLCLIENT_H__
#define __WLCLIENT_H__

#include <inttypes.h>
#include <stdbool.h>
#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Client state. */
typedef struct _wlclient_t wlclient_t;

/**
 * Creates a wayland client for simple buffer interactions.
 *
 * @return The client state, or NULL on error. The state needs to be free'd
 *     via @ref wlclient_destroy.
 */
wlclient_t *wlclient_create(void);

/**
 * Destroys the wayland client, as created by @ref wlclient_create.
 *
 * @param wlclient_ptr
 */
void wlclient_destroy(wlclient_t *wlclient_ptr);

/**
 * Runs the client's mainloop.
 *
 * @param wlclient_ptr
 */
void wlclient_run(wlclient_t *wlclient_ptr);

/** TODO: Add timer. */
void wlclient_add_timer(
    wlclient_t *wlclient_ptr,
    uint64_t msec,
    void (*callback)(wlclient_t *wlclient_ptr, void *ud_ptr),
    void *ud_ptr);

/** Returns whether the icon protocol is supported on the client. */
bool wlclient_icon_supported(wlclient_t *wlclient_ptr);

/** Returns a `bs_gfxbuf_t` for the icon. */
bs_gfxbuf_t *wlclient_icon_gfxbuf(wlclient_t *wlclient_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLCLIENT_H__ */
/* == End of wlclient.h ==================================================== */
