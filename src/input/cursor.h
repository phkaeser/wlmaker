/* ========================================================================= */
/**
 * @file cursor.h
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
#ifndef __WLMAKER_INPUT_CURSOR_H__
#define __WLMAKER_INPUT_CURSOR_H__

#include <stdbool.h>
#include <stdint.h>

#include "manager.h"
#include "toolkit/toolkit.h"

/** Forward declaration of wlmaker cursor state. */
typedef struct _wlmim_cursor_t wlmim_cursor_t;

struct wlr_input_device;
struct wlr_output_layout;
struct wlr_seat;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Style of the cursor. */
struct wlmim_cursor_style {
    /** Name of the XCursor theme to use. */
    char                      *name_ptr;
    /** Size, when non-scaled. */
    uint64_t                  size;
};

/**
 * Creates the cursor handlers.
 *
 * @param input_manager_ptr
 * @param style_ptr
 * @param wlr_output_layout_ptr
 * @param wlr_seat_ptr
 * @param root_ptr
 *
 * @return The cursor handler or NULL on error.
 */
wlmim_cursor_t *wlmim_cursor_create(
    wlmim_t *input_manager_ptr,
    const struct wlmim_cursor_style *style_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr);

/**
 * Destroys the cursor handlers.
 *
 * @param cursor_ptr
 */
void wlmim_cursor_destroy(wlmim_cursor_t *cursor_ptr);

/**
 * Updates the cursor's style.
 *
 * @param cursor_ptr
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmim_cursor_set_style(
    wlmim_cursor_t *cursor_ptr,
    const struct wlmim_cursor_style *style_ptr);

/** @return @ref wlmim_cursor_t::wlr_cursor_ptr. */
struct wlr_cursor *wlmim_cursor_wlr_cursor(wlmim_cursor_t *cursor_ptr);

/**
 * Attaches the input device. May be a pointer, touch or tablet_tool device.
 *
 * @param cursor_ptr
 * @param wlr_input_device_ptr
 */
void wlmim_cursor_attach_input_device(
    wlmim_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr);

/**
 * Detaches the input device. @see wlmim_cursor_attach_input_device.
 *
 * @param cursor_ptr
 * @param wlr_input_device_ptr
 */
void wlmim_cursor_detach_input_device(
    wlmim_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_INPUT_CURSOR_H__ */
/* == End of cursor.h ====================================================== */
