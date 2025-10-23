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

/** Forward declaration: Window. */
typedef struct _wlmtk_window2_t wlmtk_window2_t;

#include "element.h"
#include "menu.h"
#include "style.h"
#include "workspace.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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

    /** Signals that @ref wlmtk_window2_t::activated changed. */
    struct wl_signal          set_activated;

    /**
     * Signals that the window was requested to be closed.
     *
     * Applies only to windows with @ref WLMTK_WINDOW_PROPERTY_CLOSABLE.
     */
    struct wl_signal          request_close;

    /**
     * Signals that the window's size is requested to change.
     *
     * Takes a `struct wlr_box*` as argument.
     */
    struct wl_signal          request_size;

    /**
     * Signals that the window is desiring to switch to fullscreen.
     *
     * Takes a `bool` as argument, specifying whether to enable fullscreen.
     */
    struct wl_signal          request_fullscreen;
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
 * @return Pointer to the super element of @ref wlmtk_window2_t::bordered.
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
 * Returns the bounding box for the window.
 *
 * @param window_ptr
 *
 * @return the box.
 */
struct wlr_box wlmtk_window2_get_bounding_box(wlmtk_window2_t *window_ptr);

/** Helper: Indicates that the position has changed. */
void wlmtk_window2_position_changed(wlmtk_window2_t *window_ptr);

/**
 * Sets properties for the window.
 *
 * @param window_ptr
 * @param properties          @see wlmtk_window_property_t.
 */
void wlmtk_window2_set_properties(
    wlmtk_window2_t *window_ptr,
    uint32_t properties);

/** @return Pointer to @ref wlmtk_content_t::client for the window's element. */
const wlmtk_util_client_t *wlmtk_window2_get_client_ptr(
    wlmtk_window2_t *window_ptr);

/**
 * Sets the WLR output for the window. Used for fullscreen requests.
 *
 * @param window_ptr
 * @param wlr_output_ptr      Output to consider when requesting a window as
 *                            fullscreen. Can be NULL to indicate no preference.
 */
void wlmtk_window2_set_wlr_output(
    wlmtk_window2_t *window_ptr,
    struct wlr_output *wlr_output_ptr);

/**
 * Gets the struct wlr_output that the window prefers, or is on.
 *
 * @param window_ptr
 *
 * @return Pointer to the struct wlr_output the center of the window is placed
 *     on, or NULL if none is available or the window is not mapped.
 */
struct wlr_output *wlmtk_window2_get_wlr_output(
    wlmtk_window2_t *window_ptr);

/**
 * Sets the title for the window.
 *
 * If `title_ptr` is NULL, a generic name is set.
 *
 * @param window_ptr
 * @param title_ptr           May be NULL.
 *
 * @return true on success.
 */
bool wlmtk_window2_set_title(
    wlmtk_window2_t *window_ptr,
    const char *title_ptr);

/**
 * Returns the title of the window.
 *
 * @param window_ptr
 *
 * @returns Pointer to the window title. Will remain valid until the next call
 *     to @ref wlmtk_window2_set_title, or until the window is destroyed. Will
 *     never be NULL.
 */
const char *wlmtk_window2_get_title(wlmtk_window2_t *window_ptr);

/**
 * Sets the window as activated, depending on the argument's value.
 *
 * An activated window will have keyboard focus and would have distinct
 * decorations to indicate state.
 *
 * @param window_ptr
 * @param activated
 */
void wlmtk_window2_set_activated(
    wlmtk_window2_t *window_ptr,
    bool activated);

/** @return whether the window is currently activated. */
bool wlmtk_window2_is_activated(wlmtk_window2_t *window_ptr);

/**
 * Requests the window's size to be updated.
 *
 * @param window_ptr
 * @param box_ptr             Only the `width` and `height` are considered.
 */
void wlmtk_window2_request_size(
    wlmtk_window2_t *window_ptr,
    const struct wlr_box *box_ptr);

/**
 * Sets the resizing edges. Keeps the opposite edges firm when resizing.
 *
 * @param window_ptr
 * @param edges
 */
void wlmtk_window2_resize_edges(
    wlmtk_window2_t *window_ptr,
    uint32_t edges);

/**
 * Requests to close the window.
 *
 * @param window_ptr
 */
void wlmtk_window2_request_close(wlmtk_window2_t *window_ptr);

/**
 * Requests to minimize (iconify) the window.
 *
 * @param window_ptr
 */
void wlmtk_window2_request_minimize(wlmtk_window2_t *window_ptr);

/**
 * Requests the window to be fullscreen (or end fullscreen).
 *
 * @param window_ptr
 * @param fullscreen
 */
void wlmtk_window2_request_fullscreen(wlmtk_window2_t *window_ptr, bool fullscreen);

/** @return whether the window currently is in fullscreen mode. */
bool wlmtk_window2_is_fullscreen(wlmtk_window2_t *window_ptr);

/**
 * Commits the window as fullscreen: Client has comitted the surface in
 * fullscreen state.
 *
 * @param window_ptr
 * @param fullscreen
 */
void wlmtk_window2_commit_fullscreen(
    wlmtk_window2_t *window_ptr,
    bool fullscreen);

/**
 * Requests the window to be maximized (or end maximized).
 *
 * @param window_ptr
 * @param maximized
 */
void wlmtk_window2_request_maximized(wlmtk_window2_t *window_ptr, bool maximized);

/** @return whether the window currently is in maximized mode. */
bool wlmtk_window2_is_maximized(wlmtk_window2_t *window_ptr);

/**
 * Requests the window to be shaded (rolled up) or not.
 *
 * @param window_ptr
 * @param shaded
 */
void wlmtk_window2_request_shaded(wlmtk_window2_t *window_ptr, bool shaded);

/** @return whether the window currently is shaded. */
bool wlmtk_window2_is_shaded(wlmtk_window2_t *window_ptr);

/**
 * En-/Disables the window menu.
 *
 * @param window_ptr
 * @param enabled
 */
void wlmtk_window2_menu_set_enabled(wlmtk_window2_t *window_ptr, bool enabled);

/**
 * Sets whether to have server-side decorations for this window.
 *
 * @param window_ptr
 * @param decorated
 */
void wlmtk_window2_set_server_side_decorated(
    wlmtk_window2_t *window_ptr,
    bool decorated);

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

/** @return pointer to @ref wlmtk_window_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_window2(wlmtk_window2_t *window_ptr);
/** @return the @ref wlmtk_window_t for @ref wlmtk_window_t::dlnode. */
wlmtk_window2_t *wlmtk_window2_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/** Window unit test cases. */
extern const bs_test_case_t wlmtk_window2_test_cases[];

/**
 * Creates a window, with default styles, for testing.
 *
 * @param content_element_ptr
 *
 * @return See @ref wlmtk_window2_create.
 */
wlmtk_window2_t *wlmtk_test_window2_create(
    wlmtk_element_t *content_element_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW2_H__ */
/* == End of window2.h ===================================================== */
