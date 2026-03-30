/* ========================================================================= */
/**
 * @file pointer.h
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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
#ifndef __WLMAKER_POINTER_H__
#define __WLMAKER_POINTER_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <libinput.h>
#include <stdbool.h>

struct wlr_input_device;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Parameters to a pointer. */
struct wlmim_pointer_param {
    /** Configuration for pointers directed by touchpads. */
    struct {
        /** Whether to enable touchpads. True by default. */
        bool                  enabled;
        /** Disable the touchpad while typing. */
        bool                  disable_while_typing;
        /** Click method: None, ButtonAreas or ClickFinger. */
        enum libinput_config_click_method click_method;
        /** Whether tap-to-click is enabled. */
        bool                  tap_to_click;
        /** Scroll method: NoScroll, TwoFingers, Edge, OnButtonDown. */
        enum libinput_config_scroll_method scroll_method;
        /** Natural scrolling, where the content is "moved" by touchpad. */
        bool                  natural_scrolling;
    } touchpad;
};

/** Forward declaration: Pointer handle. */
typedef struct _wlmim_pointer_t wlmim_pointer_t;

/**
 * Creates a pointer, and configures it as specified.
 *
 * @param wlr_input_device_ptr
 * @param param_ptr
 *
 * @return A pointer handle or NULL on error.
 */
wlmim_pointer_t *wlmim_pointer_create(
    struct wlr_input_device *wlr_input_device_ptr,
    const struct wlmim_pointer_param *param_ptr);

/**
 * Destroys the pointer.
 *
 * @param pointer_ptr
 */
void wlmim_pointer_destroy(wlmim_pointer_t *pointer_ptr);

/** @return @ref wlmim_pointer_t::enabled. */
bool wlmim_pointer_enabled(wlmim_pointer_t *pointer_ptr);

/**
 * Parses the pointer-specific parts of the config dict.
 *
 * @param config_dict_ptr
 * @param param_ptr
 *
 * @return true on success
 */
bool wlmim_pointer_parse_config(
    bspl_dict_t *config_dict_ptr,
    struct wlmim_pointer_param *param_ptr);

/** Unit test set for pointers. */
extern const bs_test_set_t wlmim_pointer_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_POINTER_H__
/* == End of pointer.h ================= */
