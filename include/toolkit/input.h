/* ========================================================================= */
/**
 * @file input.h
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
#ifndef __WLMTK_INPUT_H__
#define __WLMTK_INPUT_H__

#include <stdint.h>

struct _wlmtk_button_event_t;
struct wlr_cursor;
struct wlr_xcursor_manager;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Button event. */
typedef struct _wlmtk_button_event_t wlmtk_button_event_t;

/** Forward declaration: Pointer. */
typedef struct _wlmtk_pointer_t wlmtk_pointer_t;

/** Button state. */
typedef enum {
    WLMTK_BUTTON_DOWN,
    WLMTK_BUTTON_UP,
    WLMTK_BUTTON_CLICK,
    WLMTK_BUTTON_DOUBLE_CLICK,
} wlmtk_button_event_type_t;

/** Cursor types. */
typedef enum {
    /** Default. */
    WLMTK_POINTER_CURSOR_DEFAULT = 0,
    /** Resizing, southern border. */
    WLMTK_POINTER_CURSOR_RESIZE_S,
    /** Resizing, south-eastern corner. */
    WLMTK_POINTER_CURSOR_RESIZE_SE,
    /** Resizing, south-western corner. */
    WLMTK_POINTER_CURSOR_RESIZE_SW,

    /** Sentinel: Maximum value. */
    WLMTK_POINTER_CURSOR_MAX,
} wlmtk_pointer_cursor_t;

/** Button events. */
struct _wlmtk_button_event_t {
    /** Button for which the event applies: linux/input-event-codes.h */
    uint32_t                  button;
    /** Type of the event: DOWN, UP, ... */
    wlmtk_button_event_type_t type;
    /** Time of the button event, in milliseconds. */
    uint32_t                  time_msec;
};

/** Motion event. */
typedef struct {
    /** X position. */
    double                    x;
    /** Y position. */
    double                    y;
    /** Time of the motion event, in milliseconds. */
    uint32_t                  time_msec;
    /** Pointer that's associated with this motion event. */
    wlmtk_pointer_t           *pointer_ptr;
} wlmtk_pointer_motion_event_t;

/**
 * Creates the pointer handler.
 *
 * @param wlr_cursor_ptr
 * @param wlr_xcursor_manager_ptr
 *
 * @return A @ref wlmtk_pointer_t or NULL on error.
 */
wlmtk_pointer_t *wlmtk_pointer_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr);

/**
 * Destroys the pointer handler.
 *
 * @param pointer_ptr
 */
void wlmtk_pointer_destroy(wlmtk_pointer_t *pointer_ptr);

/**
 * Sets the cursor for the pointer.
 */
void wlmtk_pointer_set_cursor(
    wlmtk_pointer_t *pointer_ptr,
    wlmtk_pointer_cursor_t cursor);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_INPUT_H__ */
/* == End of input.h ======================================================= */
