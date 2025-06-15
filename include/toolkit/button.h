/* ========================================================================= */
/**
 * @file button.h
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
#ifndef __WLMTK_BUTTON_H__
#define __WLMTK_BUTTON_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <wayland-server-core.h>

#include "buffer.h"
#include "element.h"

struct _wlmtk_button_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of a button. */
typedef struct _wlmtk_button_t wlmtk_button_t;

/** Virtual method table of the button. */
typedef struct {
    /** Optional: Called when the button has been clicked. */
    void (*clicked)(wlmtk_button_t *button_ptr);
} wlmtk_button_vmt_t;

/** State of a button. */
struct _wlmtk_button_t {
    /** Super class of the button: A buffer. */
    wlmtk_buffer_t            super_buffer;
    /** Original virtual method table of the superclass element. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** The virtual method table. */
    wlmtk_button_vmt_t        vmt;

    /** WLR buffer holding the button in released state. */
    struct wlr_buffer         *released_wlr_buffer_ptr;
    /** WLR buffer holding the button in pressed state. */
    struct wlr_buffer         *pressed_wlr_buffer_ptr;

    /** Listens to when we obtain pointer focus. */
    struct wl_listener        pointer_enter_listener;
    /** Listens to when we lose pointer focus. */
    struct wl_listener        pointer_leave_listener;

    /** Whether the button is currently pressed. */
    bool                      pressed;
};

/**
 * Initializes the button.
 *
 * @param button_ptr
 *
 * @return true on success.
 */
bool wlmtk_button_init(wlmtk_button_t *button_ptr);

/**
 * Extends the button's virtual methods.
 *
 * @param button_ptr
 * @param button_vmt_ptr
 *
 * @return The original virtual method table.
 */
wlmtk_button_vmt_t wlmtk_button_extend(
    wlmtk_button_t *button_ptr,
    const wlmtk_button_vmt_t *button_vmt_ptr);

/**
 * Cleans up the button.
 *
 * @param button_ptr
 */
void wlmtk_button_fini(wlmtk_button_t *button_ptr);

/**
 * Sets (or updates) the button textures.
 *
 * @param button_ptr
 * @param released_wlr_buffer_ptr
 * @param pressed_wlr_buffer_ptr
 */
void wlmtk_button_set(
    wlmtk_button_t *button_ptr,
    struct wlr_buffer *released_wlr_buffer_ptr,
    struct wlr_buffer *pressed_wlr_buffer_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_button_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BUTTON_H__ */
/* == End of button.h ====================================================== */
