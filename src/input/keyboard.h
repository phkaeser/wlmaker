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
#ifndef __WLMAKER_INPUT_KEYBOARD_H__
#define __WLMAKER_INPUT_KEYBOARD_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>

#include "manager.h"
#include "toolkit/toolkit.h"

struct wlr_keyboard;
struct wlr_seat;

/** Type of the keyboard handle. */
typedef struct _wlmim_keyboard_t wlmim_keyboard_t;
/** Forward declaration. */
typedef struct _wlmtk_element_t wlmtk_element_t;

/**
 * Creates a handle for a registered keyboard.
 *
 * @param input_manager_ptr
 * @param wlmaker_config_dict_ptr
 * @param wlr_keyboard_ptr
 * @param wlr_seat_ptr
 * @param root_element_ptr
 *
 * @return The handle or NULL on error. Free via wlmim_keyboard_destroy().
 */
wlmim_keyboard_t *wlmim_keyboard_create(
    wlmim_t *input_manager_ptr,
    bspl_dict_t *wlmaker_config_dict_ptr,
    struct wlr_keyboard *wlr_keyboard_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_element_t *root_element_ptr);

/**
 * Destroys the keyboard handle.
 *
 * @param keyboard_ptr
 */
void wlmim_keyboard_destroy(wlmim_keyboard_t *keyboard_ptr);

/** Unit test set for @ref wlmim_keyboard_t. */
extern const bs_test_set_t wlmim_keyboard_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_INPUT_KEYBOARD_H__ */
/* == End of keyboard.h ==================================================== */
