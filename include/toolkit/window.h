/* ========================================================================= */
/**
 * @file window.h
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
#ifndef __WLMTK_WINDOW_H__
#define __WLMTK_WINDOW_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>

/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t;

#include "content.h"  // IWYU pragma: keep
#include "element.h"
#include "env.h"
#include "menu.h"
#include "style.h"
#include "util.h"
#include "workspace.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Signals available for the @ref wlmtk_window_t class. */
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
     * data_ptr points to the window state (@ref wlmtk_window_t).
     */
    struct wl_signal          state_changed;
} wlmtk_window_events_t;

/** Window properties. */
typedef enum {
    /** Can be resized. Server-side decorations will show resize-bar. */
    WLMTK_WINDOW_PROPERTY_RESIZABLE = UINT32_C(1) << 0,
    /** Can be iconified. Server-side decorations include icnonify button. */
    WLMTK_WINDOW_PROPERTY_ICONIFIABLE = UINT32_C(1) << 1,
    /** Can be closed. Server-side decorations include close button. */
    WLMTK_WINDOW_PROPERTY_CLOSABLE = UINT32_C(1) << 2,

    /**
     * Kludge: a window that closes on right-click-release.
     * The window's element must pointer_grab.
     * TODO(kaeser@gubbe.ch): This should be... better.
     */
    WLMTK_WINDOW_PROPERTY_RIGHTCLICK = UINT32_C(1) << 3
} wlmtk_window_property_t;

/**
 * Creates a window for the given content.
 *
 * @param env_ptr
 * @param style_ptr
 * @param menu_style_ptr
 * @param content_ptr
 *
 * @return Pointer to the window state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_window_destroy.
 */
wlmtk_window_t *wlmtk_window_create(
    wlmtk_content_t *content_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Gets the set of events available to a window, for binding listeners.
 *
 * @param window_ptr
 *
 * @return Pointer to this window's @ref wlmtk_window_t::events.
 */
wlmtk_window_events_t *wlmtk_window_events(
    wlmtk_window_t *window_ptr);

/**
 * Destroys the window.
 *
 * @param window_ptr
 */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr);

/**
 * Returns the super Element of the window.
 *
 * TODO(kaeser@gubbe.ch): Re-evaluate whether to work with accessors, or
 *     whether to keep the ancestry members public.
 *
 * @param window_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     window_ptr.
 */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr);

/**
 * Returns the window from the super Element.
 *
 * @param element_ptr
 *
 * @return Pointer to the @ref wlmtk_window_t, for which `element_ptr` is
 *     the ancestor.
 */
wlmtk_window_t *wlmtk_window_from_element(wlmtk_element_t *element_ptr);

/** Returns @ref wlmtk_window_t for the `dlnode_ptr`. */
wlmtk_window_t *wlmtk_window_from_dlnode(bs_dllist_node_t *dlnode_ptr);
/** Accessor: Returns pointer to @ref wlmtk_window_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_window(wlmtk_window_t *window_ptr);

/**
 * Sets the output for the window. Used for fullscreen requests.
 *
 * @param window_ptr
 * @param wlr_output_ptr      Output to consider when requesting a window as
 *                            fullscreen. Can be NULL to indicate no preference.
 */
void wlmtk_window_set_output(
    wlmtk_window_t *window_ptr,
    struct wlr_output *wlr_output_ptr);

/**
 * Sets the window as activated, depending on the argument's value.
 *
 * An activated window will have keyboard focus and would have distinct
 * decorations to indicate state.
 *
 * @param window_ptr
 * @param activated
 */
void wlmtk_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated);

/**
 * Returns whether the window is activated (has keyboard focus).
 *
 * @param window_ptr
 *
 * @return activation status.
 */
bool wlmtk_window_is_activated(wlmtk_window_t *window_ptr);

/**
 * Sets whether to have server-side decorations for this window.
 *
 * @param window_ptr
 * @param decorated
 */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated);

/**
 * Sets the window's properties.
 *
 * @param window_ptr
 * @param properties          See @ref wlmtk_window_property_t.
 */
void wlmtk_window_set_properties(
    wlmtk_window_t *window_ptr,
    uint32_t properties);

/**
 * Sets the title for the window.
 *
 * If `title_ptr` is NULL, a generic name is set.
 *
 * @param window_ptr
 * @param title_ptr           May be NULL.
 */
void wlmtk_window_set_title(
    wlmtk_window_t *window_ptr,
    const char *title_ptr);

/**
 * Returns the title of the window.
 *
 * @param window_ptr
 *
 * @returns Pointer to the window title. Will remain valid until the next call
 *     to @ref wlmtk_window_set_title, or until the window is destroyed. Will
 *     never be NULL.
 */
const char *wlmtk_window_get_title(wlmtk_window_t *window_ptr);

/**
 * Requests to close the window.
 *
 * @param window_ptr
 */
void wlmtk_window_request_close(wlmtk_window_t *window_ptr);

/**
 * Requests to minimize (iconify) the window.
 *
 * @param window_ptr
 */
void wlmtk_window_request_minimize(wlmtk_window_t *window_ptr);

/**
 * Requests a move for the window.
 *
 * Requires the window to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_window_move.
 *
 * @param window_ptr
 */
void wlmtk_window_request_move(wlmtk_window_t *window_ptr);

/**
 * Requests the window to be resized.
 *
 * Requires the window to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_window_resize.
 *
 * @param window_ptr
 * @param edges
 */
void wlmtk_window_request_resize(wlmtk_window_t *window_ptr,
                                 uint32_t edges);

/**
 * Sets the window's position. This is a synchronous operation.
 *
 * Updates the position in @ref wlmtk_window_t::organic_size.
 * @param window_ptr
 * @param x
 * @param y
 */
void wlmtk_window_set_position(wlmtk_window_t *window_ptr, int x, int y);

/**
 * Obtains the size of the window, including potential decorations.
 *
 * @param window_ptr
 * @param width_ptr           May be NULL.
 * @param height_ptr          May be NULL.
 */
void wlmtk_window_get_size(
    wlmtk_window_t *window_ptr,
    int *width_ptr,
    int *height_ptr);

/**
 * Reuests the window to be maximized.
 *
 * Requires the window to be mapped (to a workspace). Will lookup the maximize
 * extents from the workspace, and request a corresponding updated position and
 * size for the window. @ref wlmtk_window_t::organic_size will not be updated.
 *
 * This may be implemented as an asynchronous operation. Maximization will be
 * applied once the size change has been committed by the surface.
 *
 * @param window_ptr
 * @param maximized
 */
void wlmtk_window_request_maximized(
    wlmtk_window_t *window_ptr,
    bool maximized);

/**
 * Commits the `maximized` mode for the window.
 *
 * This is the "commit" part of the potentially asynchronous operation. To be
 * called by @ref wlmtk_content_t, after @ref wlmtk_content_request_maximized
 * has completed by the client.
 *
 * The call is idempotent: Once the window is committed, further calls with
 * the same `maximized` value will return straight away.
 *
 * @param window_ptr
 * @param maximized
 */
void wlmtk_window_commit_maximized(
    wlmtk_window_t *window_ptr,
    bool maximized);

/** Returns whether the window is currently (requested to be) maximized. */
bool wlmtk_window_is_maximized(wlmtk_window_t *window_ptr);

/**
 * Requests the window to be made fullscreen (or stops so).
 *
 * Requires the window to be mapped (to a workspace). This may be implemented
 * as an asynchronous operation. Once the window content is ready, it should
 * call @ref wlmtk_window_commit_fullscreen to complete the operation.
 *
 * @param window_ptr
 * @param fullscreen          Whether to enable fullscreen mode.
 */
void wlmtk_window_request_fullscreen(
    wlmtk_window_t *window_ptr,
    bool fullscreen);

/**
 * Commits the fullscreen mode for the window.
 *
 * This is the "commit" part of the potentially asynchronous operation. To be
 * called by @ref wlmtk_content_t, after @ref wlmtk_content_request_fullscreen
 * has completed by the client.
 *
 * The call is idempotent: Once the window is committed, further calls with
 * the same `fullscreen` value will return straight away.
 *
 * @param window_ptr
 * @param fullscreen
 */
void wlmtk_window_commit_fullscreen(
    wlmtk_window_t *window_ptr,
    bool fullscreen);

/**
 * Returns whether the window is currently in fullscreen mode.
 *
 * Will return the state after @ref wlmtk_window_commit_fullscreen has
 * completed.
 *
 * @param window_ptr
 *
 * @return Whether it's in fullscreen mode or not.
 */
bool wlmtk_window_is_fullscreen(wlmtk_window_t *window_ptr);

/**
 * Requests the window to be "shaded", ie. rolled-up to just the title bar.
 *
 * This is supported only for server-side decorated windows.
 *
 * @param window_ptr
 * @param shaded
 */
void wlmtk_window_request_shaded(wlmtk_window_t *window_ptr, bool shaded);

/**
 * Returns whether the window is currently "shaded".
 *
 * @param window_ptr
 *
 * @return true if shaded.
 */
bool wlmtk_window_is_shaded(wlmtk_window_t *window_ptr);

/**
 * Enables (shows) or disabled (hides) the window's menu.
 *
 * @param window_ptr
 * @param enabled
 */
void wlmtk_window_menu_set_enabled(
    wlmtk_window_t *window_ptr,
    bool enabled);

/**
 * Returns a pointer to the window menu's state.
 *
 * @param window_ptr
 *
 * @return A pointer to the @ref wlmtk_menu_t of the window menu.
 */
wlmtk_menu_t *wlmtk_window_menu(wlmtk_window_t *window_ptr);

/**
 * Returns the current position and size of the window.
 *
 * @param window_ptr
 *
 * @return The position of the window (the window's element), and the currently
 *     committed width and height of the window.
 */
struct wlr_box wlmtk_window_get_position_and_size(
    wlmtk_window_t *window_ptr);

/**
 * Requests an updated position and size for the window, including potential
 * decorations.
 *
 * This may be implemented as an asynchronous operation. The re-positioning
 * will be applied only once the size change has been committed by the client.
 *
 * The position and size will be stored in @ref wlmtk_window_t::organic_size.
 *
 * @param window_ptr
 * @param x
 * @param y
 * @param width
 * @param height
 */
void wlmtk_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height);

/**
 * Updates the window state to what was requested at the `serial`.
 *
 * Used for example when resizing a window from the top or left edges. In that
 * case, @ref wlmtk_content_request_size may be asynchronous and returns a
 * serial. The surface is expected to call @ref wlmtk_window_serial with the
 * returned serial when the size is committed.
 * Only then, the corresponding positional update on the top/left edges are
 * supposed to be applied.
 *
 * @ref wlmtk_window_t::organic_size will be updated, if there was no pending
 * update: Meaning that the commit originated not from an earlier
 * @ref wlmtk_window_request_position_and_size or @ref
 * wlmtk_window_request_maximized call.
 *
 * @param window_ptr
 * @param serial
 */
void wlmtk_window_serial(wlmtk_window_t *window_ptr, uint32_t serial);

/**
 * Sets @ref wlmtk_window_t::workspace_ptr.
 *
 * Protected method, to be called only from @ref wlmtk_workspace_t.
 *
 * @param window_ptr
 * @param workspace_ptr
 */
void wlmtk_window_set_workspace(
    wlmtk_window_t *window_ptr,
    wlmtk_workspace_t *workspace_ptr);

/** @return The value of @ref wlmtk_window_t::workspace_ptr. */
wlmtk_workspace_t *wlmtk_window_get_workspace(wlmtk_window_t *window_ptr);

/** @return Pointer to @ref wlmtk_content_t::client for `content_ptr`. */
const wlmtk_util_client_t *wlmtk_window_get_client_ptr(
    wlmtk_window_t *window_ptr);

/* ------------------------------------------------------------------------- */

/** State of the fake window, for tests. */
typedef struct {
    /** Window state. */
    wlmtk_window_t            *window_ptr;
    /** Fake surface, to manipulate the fake window's surface. */
    wlmtk_fake_surface_t      *fake_surface_ptr;
    /** Fake content, wraps the fake surface. */
    wlmtk_fake_content_t      *fake_content_ptr;
    /** Hack: Direct link to window popup menu. */
    wlmtk_menu_t              *window_menu_ptr;

    /** Whether @ref wlmtk_window_request_minimize was called. */
    bool                      request_minimize_called;
   /** Whether @ref wlmtk_window_request_move was called. */
    bool                      request_move_called;
    /** Whether @ref wlmtk_window_request_resize was called. */
    bool                      request_resize_called;
    /** Argument to last @ref wlmtk_window_request_resize call. */
    uint32_t                  request_resize_edges;
} wlmtk_fake_window_t;

/** Ctor. */
wlmtk_fake_window_t *wlmtk_fake_window_create(void);
/** Dtor. */
void wlmtk_fake_window_destroy(wlmtk_fake_window_t *fake_window_ptr);
/** Calls commit_size with the fake surface's serial and dimensions. */
void wlmtk_fake_window_commit_size(wlmtk_fake_window_t *fake_window_ptr);

/** Unit tests for window. */
extern const bs_test_case_t wlmtk_window_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW_H__ */
/* == End of window.h ====================================================== */
