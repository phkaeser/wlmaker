/* ========================================================================= */
/**
 * @file icon.h
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
#ifndef __LIBWLCLIENT_ICON_H__
#define __LIBWLCLIENT_ICON_H__

#include "libwlclient.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration of an icon's state. */
typedef struct _wlclient_icon_t wlclient_icon_t;

/**
 * Creates an icon.
 *
 * @param wlclient_ptr
 *
 * @return An icon state or NULL on error. The state must be free'd by calling
 *     @ref wlclient_icon_destroy.
 */
wlclient_icon_t *wlclient_icon_create(
    wlclient_t *wlclient_ptr);

/**
 * Destroys the icon.
 *
 * @param icon_ptr
 */
void wlclient_icon_destroy(
    wlclient_icon_t *icon_ptr);

/**
 * Returns whether the icon protocol is supported on the client.
 *
 * @param wlclient_ptr
 */
bool wlclient_icon_supported(wlclient_t *wlclient_ptr);

/**
 * Sets a callback to invoke when the background buffer is ready for drawing.
 *
 * @see wlcl_dblbuf_register_ready_callback.
 *
 * @param icon_ptr
 * @param callback
 * @param ud_ptr
 */
void wlclient_icon_register_ready_callback(
    wlclient_icon_t *icon_ptr,
    bool (*callback)(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr),
    void *ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LIBWLCLIENT_ICON_H__ */
/* == End of icon.h ======================================================== */
