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

#include "server.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

/** Type of the keyboard handle. */
typedef struct _wlmaker_keyboard_t wlmaker_keyboard_t;
/** A key binding. */
typedef struct _wlmaker_keybinding_t wlmaker_keybinding_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Specifies the key + modifier to bind. */
struct _wlmaker_keybinding_t {
    /** Modifiers expected for this keybinding. */
    uint32_t                  modifiers;
    /** Modifier mask: Only masked modifiers are considered. */
    uint32_t                  modifiers_mask;
    /** XKB Keysym to trigger on. */
    xkb_keysym_t              keysym;
    /** Whether to ignore case when matching. */
    bool                      ignore_case;
};

/**
 * Callback for a key binding.
 *
 * @param b                   The keybinding that triggered the callback.
 *
 * @return true if the key can be considered "consumed".
 */
typedef bool (*wlmaker_keybinding_callback_t)(const wlmaker_keybinding_t *b);


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

/**
 * Binds a particular key to a callback.
 *
 * @param keyboard_ptr
 * @param binding_ptr
 * @param callback
 *
 * @return true on success.
 */
bool wlmaker_keyboard_bind(
    wlmaker_keyboard_t *keyboard_ptr,
    const wlmaker_keybinding_t *binding_ptr,
    wlmaker_keybinding_callback_t callback);

/**
 * Releases a key binding. @see wlmaker_keyboard_bind.
 *
 * @param keyboard_ptr
 * @param binding_ptr
 */
void wlmaker_keyboard_release(
    wlmaker_keyboard_t *keyboard_ptr,
    const wlmaker_keybinding_t *binding_ptr);

/** Unit test cases. */
extern const bs_test_case_t   wlmaker_keyboard_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_KEYBOARD_H__ */
/* == End of keyboard.h ==================================================== */
