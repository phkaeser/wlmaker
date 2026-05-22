/* ========================================================================= */
/**
 * @file root.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright 2024 Google LLC
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
#ifndef __WLMTK_ROOT_H__
#define __WLMTK_ROOT_H__

/** Forward declaration: Root element wrapper. */
typedef struct _wlmtk_root_t wlmtk_root_t;

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_pointer.h>
#undef WLR_USE_UNSTABLE

#include "element.h"
#include "input.h"

struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Signals available for the @ref wlmtk_root_t class. */
typedef struct {
    /** An unclaimed button event. Arg: @ref wlmtk_button_event_t. */
    struct wl_signal          unclaimed_button_event;
} wlmtk_root_events_t;

/**
 * Creates the root element wrapper.
 *
 * @param element_ptr         The wrapped element.
 * @param wlr_output_layout_ptr wlroots output layout to track output frames on.
 *
 * @return Handle of the root wrapper or NULL on error.
 */
wlmtk_root_t *wlmtk_root_create(
    wlmtk_element_t *element_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr);

/**
 * Destroys the root wrapper.
 *
 * @param root_ptr            Root wrapper handle.
 */
void wlmtk_root_destroy(wlmtk_root_t *root_ptr);

/**
 * Gets the set of events available in root.
 *
 * @param root_ptr            Root wrapper handle.
 *
 * @return Pointer to @ref wlmtk_root_events_t.
 */
wlmtk_root_events_t *wlmtk_root_events(wlmtk_root_t *root_ptr);

/**
 * Gets the wrapped element.
 *
 * @param root_ptr            Root wrapper handle.
 *
 * @return Pointer to the wrapped @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr);

/**
 * Handles a pointer motion event.
 *
 * @param root_ptr            Root wrapper handle.
 * @param x                   X coordinate of the motion event.
 * @param y                   Y coordinate of the motion event.
 * @param time_msec           Timestamp of the motion event.
 * @param pointer_ptr         Pointer state reference.
 *
 * @return Whether there was an element under the pointer.
 */
bool wlmtk_root_pointer_motion(
    wlmtk_root_t *root_ptr,
    double x,
    double y,
    uint32_t time_msec,
    wlmtk_pointer_t *pointer_ptr);

/**
 * Handles a button event: Translates to button down/up/click/dblclick events.
 *
 * Each button activity (button pressed or released) will directly trigger a
 * corresponding BUTTON_DOWN or BUTTON_UP event. Depending on timing and
 * motion, a "released" event may also triccer a CLICK, DOUBLE_CLICK or
 * DRAG event.
 * These events will be forwarded to the event currently having pointer focus.
 *
 * TODO(kaeser@gubbe.ch): Implement DOUBLE_CLICK and DRAG events, and make it
 * well tested.
 *
 * @param root_ptr            Root wrapper handle.
 * @param event_ptr           Wayland button event pointer.
 * @param modifiers           Keyboard modifiers state.
 *
 * @return Whether the button event was consumed.
 */
bool wlmtk_root_pointer_button(
    wlmtk_root_t *root_ptr,
    const struct wlr_pointer_button_event *event_ptr,
    uint32_t modifiers);

/**
 * Handles a pointer axis event.
 *
 * @param root_ptr            Root wrapper handle.
 * @param wlr_pointer_axis_event_ptr wlroots axis event pointer.
 *
 * @return Whether the axis event was consumed.
 */
bool wlmtk_root_pointer_axis(
    wlmtk_root_t *root_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);

/** Unit test cases. */
extern const bs_test_set_t wlmtk_root_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ROOT_H__ */
/* == End of root.h ======================================================== */
