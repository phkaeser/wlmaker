/* ========================================================================= */
/**
 * @file keyboard.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#ifndef __WLMAKER_INPUT_KEYBOARD_H__
#define __WLMAKER_INPUT_KEYBOARD_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdint.h>

#include "manager.h"
#include "toolkit/toolkit.h"

struct wlr_keyboard;
struct wlr_seat;
struct xkb_keymap;

/** Type of the keyboard handle. */
typedef struct _wlmim_keyboard_t wlmim_keyboard_t;
/** Forward declaration. */
typedef struct _wlmtk_element_t wlmtk_element_t;

/** Options for the keyboard. */
struct wlmim_keyboard_options {
    /** Key repeat options. */
    struct {
        /** Number of repeats per second. */
        uint64_t              rate;
        /** Delay in milliseconds before initiating repeats. */
        uint64_t              delay;
    } repeat;
};

/**
 * Creates a handle for a registered keyboard.
 *
 * @param input_manager_ptr
 * @param xkb_keymap_ptr
 * @param options_ptr
 * @param wlr_keyboard_ptr
 * @param wlr_seat_ptr
 * @param root_element_ptr
 *
 * @return The handle or NULL on error. Free via wlmim_keyboard_destroy().
 */
wlmim_keyboard_t *wlmim_keyboard_create(
    wlmim_t *input_manager_ptr,
    struct xkb_keymap *xkb_keymap_ptr,
    const struct wlmim_keyboard_options *options_ptr,
    struct wlr_keyboard *wlr_keyboard_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_element_t *root_element_ptr);

/**
 * Destroys the keyboard handle.
 *
 * @param keyboard_ptr
 */
void wlmim_keyboard_destroy(wlmim_keyboard_t *keyboard_ptr);

/**
 * Configures the active xkb_keymap and repeat settings.
 *
 * @param keyboard_ptr
 * @param xkb_keymap_ptr
 * @param options_ptr
 */
void wlmim_keyboard_configure(
    wlmim_keyboard_t *keyboard_ptr,
    struct xkb_keymap *xkb_keymap_ptr,
    const struct wlmim_keyboard_options *options_ptr);

/**
 * Creates a `struct xkb_keymap` from the keyboard configuration dict.
 *
 * @param dict_ptr            Plist dict of the `Keyboard` configuration. If
 *                            NULL, NULL is returned.
 *
 * @return Pointer to a created `struct xkb_keymap` (to be released by calling
 *     xkb_keymap_unref()), or NULL on error.
 */
struct xkb_keymap *wlmim_keyboard_xkb_from_config(bspl_dict_t *dict_ptr);

/** Keyboard modifiers, as enum to lookup. */
extern const bspl_enum_desc_t wlmim_keyboard_modifiers[];

/** BSPL descriptor for decoding the keyboard dict. */
extern const bspl_desc_t wlmim_keyboard_options_desc[];

/** Unit test set. */
extern const bs_test_set_t    wlmim_keyboard_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_INPUT_KEYBOARD_H__ */
/* == End of keyboard.h ==================================================== */
