/* ========================================================================= */
/**
 * @file desktop.h
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
#ifndef __WLMTK_DESKTOP_H__
#define __WLMTK_DESKTOP_H__

/** Forward declaration: Desktop element (technically: container). */
typedef struct _wlmtk_desktop_t wlmtk_desktop_t;
/** Forward declaration: wlr output layout. */

#include <libbase/libbase.h>
#include <stdbool.h>
#include <wayland-server-core.h>

#include "element.h"
#include "menu.h"
#include "surface.h"  // IWYU pragma: keep
#include "window.h"  // IWYU pragma: keep
#include "workspace.h"  // IWYU pragma: keep

struct wlr_output_layout;
/** Forward declaration: Wlroots scene. */
struct wlr_scene;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Signals available for the @ref wlmtk_desktop_t class. */
typedef struct {
    /**
     * Signal: Raised when the current workspace is changed.
     * Data: Pointer to the new `wlmaker_workspace_t`, or NULL.
     */
    struct wl_signal          workspace_changed;

    /** Triggers whenever @ref wlmtk_desktop_unlock succeeds. */
    struct wl_signal          unlock_event;
    /** Triggers when a window is mapped to a workspace. */
    struct wl_signal          window_mapped;
    /** Triggers when a window is unmapped from a workspace. */
    struct wl_signal          window_unmapped;
} wlmtk_desktop_events_t;

/**
 * Creates the desktop element.
 *
 * @param wlr_scene_ptr
 * @param wlr_output_layout_ptr
 *
 * @return Handle of the desktop element or NULL on error.
 */
wlmtk_desktop_t *wlmtk_desktop_create(
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr);

/**
 * Destroys the desktop element.
 *
 * @param desktop_ptr
 */
void wlmtk_desktop_destroy(wlmtk_desktop_t *desktop_ptr);

/**
 * Gets the set of events available in desktop. To bind listeners to.
 *
 * @param desktop_ptr
 *
 * @return Pointer to @ref wlmtk_desktop_t::events.
 */
wlmtk_desktop_events_t *wlmtk_desktop_events(wlmtk_desktop_t *desktop_ptr);

/**
 * Adds a workspace.
 *
 * @param desktop_ptr
 * @param workspace_ptr
 */
void wlmtk_desktop_add_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr);

/**
 * Removes the workspace.
 *
 * @param desktop_ptr
 * @param workspace_ptr
 */
void wlmtk_desktop_remove_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr);

/**
 * Returns a pointer to the currently-active workspace.
 *
 * @param desktop_ptr
 */
wlmtk_workspace_t *wlmtk_desktop_get_current_workspace(wlmtk_desktop_t *desktop_ptr);

/**
 * Destroys the last workspace -- if it's not the only remaining workspace, not
 * the current workspace, and if there's no windows contained in it.
 *
 * @param desktop_ptr
 */
void wlmtk_desktop_destroy_last_workspace(wlmtk_desktop_t *desktop_ptr);

/**
 * Switches to the next workspace.
 *
 * @param desktop_ptr
 */
void wlmtk_desktop_switch_to_next_workspace(wlmtk_desktop_t *desktop_ptr);

/**
 * Switches to the previous workspace.
 *
 * @param desktop_ptr
 */
void wlmtk_desktop_switch_to_previous_workspace(wlmtk_desktop_t *desktop_ptr);

/**
 * Runs |func()| for each workspace.
 *
 * @param desktop_ptr
 * @param func
 * @param ud_ptr
 */
void wlmtk_desktop_for_each_workspace(
    wlmtk_desktop_t *desktop_ptr,
    void (*func)(bs_dllist_node_t *dlnode_ptr, void *ud_ptr),
    void *ud_ptr);

/**
 * Locks the desktop, using the provided element.
 *
 * The desktop must not be locked already. If locked successfully, the desktop will
 * keep a reference to `element_ptr`. The lock must call @ref wlmtk_desktop_unlock
 * to unlock desktop, and for releasing the reference.
 *
 * @param desktop_ptr
 * @param element_ptr
 *
 * @return Whether the lock was established.
 */
bool wlmtk_desktop_lock(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Unlocks the desktop, and releases the reference from @ref wlmtk_desktop_lock.
 *
 * Unlocking can only be done with `element_ptr` matching the `element_ptr`
 * argument from @ref wlmtk_desktop_lock.
 *
 * @param desktop_ptr
 * @param element_ptr
 *
 * @return Whether the lock was lifted.
 */
bool wlmtk_desktop_unlock(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr);

/** @return Whether desktop is locked. */
bool wlmtk_desktop_locked(wlmtk_desktop_t *desktop_ptr);

/**
 * Releases the lock reference, but keeps the desktop locked.
 *
 * This is in accordance with the session lock protocol specification [1],
 * stating the session should remain locked if the client dies.
 * This call is a no-op if `element_ptr` is not currently the lock of
 * `desktop_ptr`.
 *
 * [1] https://wayland.app/protocols/ext-session-lock-v1
 *
 * @param desktop_ptr
 * @param element_ptr
 */
void wlmtk_desktop_lock_unreference(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr);

/** @returns pointer to the desktop's @ref wlmtk_element_t. (Temporary) */
wlmtk_element_t *wlmtk_desktop_element(wlmtk_desktop_t *desktop_ptr);

/** Updates the style for all windows contained in desktop. */
bool wlmtk_desktop_set_style(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_window_style_ref_t *window_style_ref_ptr,
    wlmtk_menu_style_ref_t *menu_style_ref_ptr);

/** Unit test cases. */
extern const bs_test_set_t wlmtk_desktop_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_DESKTOP_H__ */
/* == End of desktop.h ===================================================== */
