/* ========================================================================= */
/**
 * @file keyboard.h
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
#ifndef __WLMAKER_KEYBOARD_H__
#define __WLMAKER_KEYBOARD_H__

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

#include "server.h"  // IWYU pragma: keep

struct wlr_keyboard;

/** Type of the keyboard handle. */
typedef struct _wlmaker_keyboard_t wlmaker_keyboard_t;

/**
 * Creates a handle for a registered keyboard.
 *
 * @param server_ptr
 * @param wlr_keyboard_ptr
 * @param wlr_seat_ptr
 *
 * @return The handle or NULL on error. Free via wlmaker_keyboard_destroy().
 */
wlmaker_keyboard_t *wlmaker_keyboard_create(
    wlmaker_server_t *server_ptr,
    struct wlr_keyboard *wlr_keyboard_ptr,
    struct wlr_seat *wlr_seat_ptr);

/**
 * Destroys the keyboard handle.
 *
 * @param keyboard_ptr
 */
void wlmaker_keyboard_destroy(wlmaker_keyboard_t *keyboard_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_KEYBOARD_H__ */
/* == End of keyboard.h ==================================================== */
