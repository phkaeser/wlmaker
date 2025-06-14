/* ========================================================================= */
/**
 * @file root.h
 *
 * @copyright
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

/** Forward declaration: Root element (technically: container). */
typedef struct _wlmtk_root_t wlmtk_root_t;
/** Forward declaration: wlr output layout. */

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>

#include "element.h"
#include "env.h"
#include "input.h"
#include "surface.h"  // IWYU pragma: keep
#include "workspace.h"  // IWYU pragma: keep

struct wlr_output_layout;
/** Forward declaration: Wlroots scene. */
struct wlr_scene;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Signals available for the @ref wlmtk_root_t class. */
typedef struct {
    /**
     * Signal: Raised when the current workspace is changed.
     * Data: Pointer to the new `wlmaker_workspace_t`.
     */
    struct wl_signal          workspace_changed;

    /** Triggers whenever @ref wlmtk_root_unlock succeeds. */
    struct wl_signal          unlock_event;
    /** Triggers when a window is mapped to a workspace. */
    struct wl_signal          window_mapped;
    /** Triggers when a window is unmapped from a workspace. */
    struct wl_signal          window_unmapped;

    /** An unclaimed pointer event. Arg: @ref wlmtk_button_event_t. */
    struct wl_signal          unclaimed_button_event;
} wlmtk_root_events_t;

/**
 * Creates the root element.
 *
 * @param wlr_scene_ptr
 * @param wlr_output_layout_ptr
 * @param env_ptr
 *
 * @return Handle of the root element or NULL on error.
 */
wlmtk_root_t *wlmtk_root_create(
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the root element.
 *
 * @param root_ptr
 */
void wlmtk_root_destroy(wlmtk_root_t *root_ptr);

/**
 * Gets the set of events available in root. To bind listeners to.
 *
 * @param root_ptr
 *
 * @return Pointer to @ref wlmtk_root_t::events.
 */
wlmtk_root_events_t *wlmtk_root_events(wlmtk_root_t *root_ptr);

/**
 * Handles a pointer motion event.
 *
 * @param root_ptr
 * @param x
 * @param y
 * @param time_msec
 * @param pointer_ptr
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
 * @param root_ptr
 * @param event_ptr
 *
 * @return Whether the button was consumed.
 */
bool wlmtk_root_pointer_button(
    wlmtk_root_t *root_ptr,
    const struct wlr_pointer_button_event *event_ptr);

/**
 * Handles a pointer axis event.
 *
 * @param root_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return Whether the axis event was consumed.
 */
bool wlmtk_root_pointer_axis(
    wlmtk_root_t *root_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);

/**
 * Adds a workspace.
 *
 * @param root_ptr
 * @param workspace_ptr
 */
void wlmtk_root_add_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr);

/**
 * Removes the workspace.
 *
 * @param root_ptr
 * @param workspace_ptr
 */
void wlmtk_root_remove_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr);

/**
 * Returns a pointer to the currently-active workspace.
 *
 * @param root_ptr
 */
wlmtk_workspace_t *wlmtk_root_get_current_workspace(wlmtk_root_t *root_ptr);

/**
 * Switches to the next workspace.
 *
 * @param root_ptr
 */
void wlmtk_root_switch_to_next_workspace(wlmtk_root_t *root_ptr);

/**
 * Switches to the previous workspace.
 *
 * @param root_ptr
 */
void wlmtk_root_switch_to_previous_workspace(wlmtk_root_t *root_ptr);

/**
 * Runs |func()| for each workspace.
 *
 * @param root_ptr
 * @param func
 * @param ud_ptr
 */
void wlmtk_root_for_each_workspace(
    wlmtk_root_t *root_ptr,
    void (*func)(bs_dllist_node_t *dlnode_ptr, void *ud_ptr),
    void *ud_ptr);

/**
 * Locks the root, using the provided element.
 *
 * The root must not be locked already. If locked successfully, the root will
 * keep a reference to `element_ptr`. The lock must call @ref wlmtk_root_unlock
 * to unlock root, and for releasing the reference.
 *
 * @param root_ptr
 * @param element_ptr
 *
 * @return Whether the lock was established.
 */
bool wlmtk_root_lock(
    wlmtk_root_t *root_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Unlocks the root, and releases the reference from @ref wlmtk_root_lock.
 *
 * Unlocking can only be done with `element_ptr` matching the `element_ptr`
 * argument from @ref wlmtk_root_lock.
 *
 * @param root_ptr
 * @param element_ptr
 *
 * @return Whether the lock was lifted.
 */
bool wlmtk_root_unlock(
    wlmtk_root_t *root_ptr,
    wlmtk_element_t *element_ptr);

/** @return Whether root is locked. */
bool wlmtk_root_locked(wlmtk_root_t *root_ptr);

/**
 * Releases the lock reference, but keeps the root locked.
 *
 * This is in accordance with the session lock protocol specification [1],
 * stating the session should remain locked if the client dies.
 * This call is a no-op if `element_ptr` is not currently the lock of
 * `root_ptr`.
 *
 * [1] https://wayland.app/protocols/ext-session-lock-v1
 *
 * @param root_ptr
 * @param element_ptr
 */
void wlmtk_root_lock_unreference(
    wlmtk_root_t *root_ptr,
    wlmtk_element_t *element_ptr);

/** @returns pointer to the root's @ref wlmtk_element_t. (Temporary) */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_root_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ROOT_H__ */
/* == End of root.h ======================================================== */
