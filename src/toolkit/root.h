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

#include "lock.h"

#include "surface.h"

/** Forward declaration: Wlroots scene. */
struct wlr_scene;
struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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
 * Handles a pointer motion event.
 *
 * @param root_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether there was an element under the pointer.
 */
bool wlmtk_root_pointer_motion(
    wlmtk_root_t *root_ptr,
    double x,
    double y,
    uint32_t time_msec);

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
 * Locks the root, using the provided lock.
 *
 * The root must not be locked already. If locked successfully, the root will
 * keep a reference to `lock_ptr`. The lock must call @ref wlmtk_root_unlock
 * to unlock root, and for releasing the reference.
 *
 * @param root_ptr
 * @param lock_ptr
 *
 * @return Whether the lock was established.
 */
bool wlmtk_root_lock(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr);

/**
 * Unlocks the root, and releases the reference from @ref wlmtk_root_lock.
 *
 * Unlocking can only be done with `lock_ptr` matching the `lock_ptr` argument
 * from @ref wlmtk_root_lock.
 *
 * @param root_ptr
 * @param lock_ptr
 *
 * @return Whether the lock was lifted.
 */
bool wlmtk_root_unlock(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr);

/**
 * Releases the lock reference, but keeps the root locked.
 *
 * This is in accordance with the session lock protocol specification [1],
 * stating the session should remain locked if the client dies.
 * This call is a no-op if `lock_ptr` is not currently the lock of `root_ptr`.
 *
 * [1] https://wayland.app/protocols/ext-session-lock-v1
 *
 * @param root_ptr
 * @param lock_ptr
 */
void wlmtk_root_lock_unreference(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr);

/**
 * Temporary: Set the lock surface, so events get passed correctly.
 *
 * TODO(kaeser@gubbe.ch): Remove the method, events should get passed via
 * the container.
 *
 * @param root_ptr
 * @param surface_ptr
 */
void wlmtk_root_set_lock_surface(
    wlmtk_root_t *root_ptr,
    wlmtk_surface_t *surface_ptr);

/** Connects a listener and handler to @ref wlmtk_root_t::unlock_event. */
void wlmtk_root_connect_unlock_signal(
    wlmtk_root_t *root_ptr,
    struct wl_listener *listener_ptr,
    wl_notify_func_t handler);

/** @returns pointer to the root's @ref wlmtk_element_t. (Temporary) */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr);

/** Creates a root with fake setup for tests. */
wlmtk_root_t *wlmtk_fake_root_create(void);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_root_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ROOT_H__ */
/* == End of root.h ======================================================== */
