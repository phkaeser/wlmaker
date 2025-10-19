/* ========================================================================= */
/**
 * @file window2.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMTK_WINDOW2_H__
#define __WLMTK_WINDOW2_H__

#include "element.h"
#include "menu.h"
#include "style.h"
#include "workspace.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Window. */
typedef struct _wlmtk_window2_t wlmtk_window2_t;

/** Signals available for the @ref wlmtk_window2_t class. */
typedef struct {
    /**
     * Signals that the window state (maximize, iconify, ...) changed.
     *
     * Window state can be retrieved from:
     * - @ref wlmtk_window_is_maximized
     * - @ref wlmtk_window_is_fullscreen
     * - @ref wlmtk_window_is_shaded
     *
     * The signal is also raised when the window's workspace is changed.
     * Retrieve through @ref wlmtk_window_get_workspace.
     *
     * data_ptr points to the window state (@ref wlmtk_window2_t).
     */
    struct wl_signal          state_changed;
} wlmtk_window2_events_t;

/**
 * Creates a window.
 *
 * @param content_element_ptr
 * @param style_ptr
 * @param menu_style_ptr
 *
 * @return The window handle, or NULL on error.
 */
wlmtk_window2_t *wlmtk_window2_create(
    wlmtk_element_t *content_element_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr);

/**
 * Destroys the window.
 *
 * @param window_ptr
 */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr);

/**
 * Gets the set of events available to a window, for binding listeners.
 *
 * @param window_ptr
 *
 * @return Pointer to this window's @ref wlmtk_window2_t::events.
 */
wlmtk_window2_events_t *wlmtk_window2_events(wlmtk_window2_t *window_ptr);

/**
 * Returns the element for adding the window into a container.
 *
 * @param window_ptr
 *
 * @return Pointer to the super element of @ref wlmtk_window2_t::container.
 */
wlmtk_element_t *wlmtk_window2_element(wlmtk_window2_t *window_ptr);

/**
 * Returns the window, where `element_ptr` is @ref wlmtk_window2_element.
 *
 * @param element_ptr
 *
 * @return the @ref wlmtk_window2_t from the element pointer.
 */
wlmtk_window2_t *wlmtk_window2_from_element(wlmtk_element_t *element_ptr);

/**
 * Sets @ref wlmtk_window2_t::workspace_ptr.
 *
 * Protected method, to be called only from @ref wlmtk_workspace_t.
 *
 * @param window_ptr
 * @param workspace_ptr
 */
void wlmtk_window2_set_workspace(
    wlmtk_window2_t *window_ptr,
    wlmtk_workspace_t *workspace_ptr);

/** @return The value of @ref wlmtk_window_t::workspace_ptr. */
wlmtk_workspace_t *wlmtk_window2_get_workspace(wlmtk_window2_t *window_ptr);

/** Window unit test cases. */
extern const bs_test_case_t wlmtk_window2_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW2_H__ */
/* == End of window2.h ===================================================== */
