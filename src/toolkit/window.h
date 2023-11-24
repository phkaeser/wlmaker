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

/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t;

#include "box.h"
#include "content.h"
#include "element.h"
#include "resizebar.h"
#include "titlebar.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration. */
struct wlr_cursor;
/** Forward declaration. */
struct wlr_xcursor_manager;

/** Maximum number of pending state updates. */
#define WLMTK_WINDOW_MAX_PENDING 64

/** Pending positional updates. */
typedef struct {
    /** Node within @ref wlmtk_window_t::pending_updates. */
    bs_dllist_node_t          dlnode;
    /** Serial of the update. */
    uint32_t                  serial;
    /** Pending X position. */
    int                       x;
    /** Pending Y position. */
    int                       y;
    /** Width that is to be committed at serial. */
    unsigned                  width;
    /** Height that is to be committed at serial. */
    unsigned                  height;
} wlmtk_pending_update_t;

/** Virtual method table for @ref wlmtk_window_t. */
typedef struct {
    /** Destructor. */
    void (*destroy)(wlmtk_window_t *window_ptr);
    /** See @ref wlmtk_window_set_activated. */
    void (*set_activated)(wlmtk_window_t *window_ptr,
                          bool activated);
    /** See @ref wlmtk_window_set_server_side_decorated. */
    void (*set_server_side_decorated)(wlmtk_window_t *window_ptr,
                                      bool decorated);
    /** See @ref wlmtk_window_set_title. */
    void (*set_title)(wlmtk_window_t *window_ptr, const char *title_ptr);

    /** See @ref wlmtk_window_request_close. */
    void (*request_close)(wlmtk_window_t *window_ptr);
    /** See @ref wlmtk_window_request_minimize. */
    void (*request_minimize)(wlmtk_window_t *window_ptr);
    /** See @ref wlmtk_window_request_move. */
    void (*request_move)(wlmtk_window_t *window_ptr);
    /** See @ref wlmtk_window_request_resize. */
    void (*request_resize)(wlmtk_window_t *window_ptr,
                           uint32_t edges);

    /** See @ref wlmtk_window_request_size. */
    void (*request_size)(wlmtk_window_t *window_ptr,
                         int x, int y);
    /** See @ref wlmtk_window_request_position_and_size. */
    void (*request_position_and_size)(wlmtk_window_t *window_ptr,
                                      int x, int y, int width, int height);
} wlmtk_window_impl_t;

/** State of the window. */
struct _wlmtk_window_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;
    /** Original virtual method table of the box' container superclass. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Virtual method table. */
    wlmtk_window_impl_t       impl;

    /** Content of this window. */
    wlmtk_content_t           *content_ptr;
    /** Titlebar. */
    wlmtk_titlebar_t          *titlebar_ptr;
    /** Resizebar. */
    wlmtk_resizebar_t         *resizebar_ptr;

    /** Window title. Set through @ref wlmtk_window_set_title. */
    char                      *title_ptr;

    /** Pending updates. */
    bs_dllist_t               pending_updates;
    /** List of udpates currently available. */
    bs_dllist_t               available_updates;
    /** Pre-alloocated updates. */
    wlmtk_pending_update_t     pre_allocated_updates[WLMTK_WINDOW_MAX_PENDING];
};

/**
 * Creates a window for the given content.
 *
 * @param wlr_cursor_ptr
 * @param wlr_xcursor_manager_ptr
 * @param content_ptr         Will take ownership of content_ptr.
 *
 * @return Pointer to the window state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_window_destroy.
 */
wlmtk_window_t *wlmtk_window_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr,
    wlmtk_content_t *content_ptr);

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
 * Sets whether to have server-side decorations for this window.
 *
 * @param window_ptr
 * @param decorated
 */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated);

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
 * Requests a new size for the window, including potential decorations.
 *
 * This may be implemented as an asynchronous operation.
 *
 * @param window_ptr
 * @param width
 * @param height
 */
void wlmtk_window_request_size(
    wlmtk_window_t *window_ptr,
    int width,
    int height);

/**
 * Requests an updated position and size for the window, including potential
 * decorations.
 *
 * This may be implemented as an asynchronous operation. The re-positioning
 * will be applied only once the size change has been committed by the client.
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
 * serial. The content is expected to call @ref wlmtk_window_serial with the
 * returned serial when the size is committed.
 * Only then, the corresponding positional update on the top/left edges are
 * supposed to be applied.
 *
 * @param window_ptr
 * @param serial
 */
void wlmtk_window_serial(wlmtk_window_t *window_ptr, uint32_t serial);

/** State of the fake window, for tests. */
typedef struct {
    /** Window state. */
    wlmtk_window_t            window;
    /** Argument to last @ref wlmtk_window_set_activated call. */
    bool                      activated;
    /** Argument to last @ref wlmtk_window_set_server_side_decorated call. */
    bool                      decorated;
    /** Whether @ref wlmtk_window_request_close was called. */
    bool                      request_close_called;
    /** Whether @ref wlmtk_window_request_minimize was called. */
    bool                      request_minimize_called;
   /** Whether @ref wlmtk_window_request_move was called. */
    bool                      request_move_called;
    /** Whether @ref wlmtk_window_request_resize was called. */
    bool                      request_resize_called;
    /** Argument to last @ref wlmtk_window_request_resize call. */
    uint32_t                  request_resize_edges;
    /** Whether @ref wlmtk_window_request_size was called. */
    bool                      request_size_called;
    /** Whether @ref wlmtk_window_request_position_and_size was called. */
    bool                      request_position_and_size_called;
    /** Argument to last @ref wlmtk_window_request_size call. */
    int                       x;
    /** Argument to last @ref wlmtk_window_request_size call. */
    int                       y;
    /** Argument to last @ref wlmtk_window_request_size call. */
    int                       width;
    /** Argument to last @ref wlmtk_window_request_size call. */
    int                       height;

    /** Fake content, to manipulate the fake window's content. */
    wlmtk_fake_content_t      *fake_content_ptr;
} wlmtk_fake_window_t;

/** Ctor. */
wlmtk_fake_window_t *wlmtk_fake_window_create(void);
/** Dtor. */
void wlmtk_fake_window_destroy(wlmtk_fake_window_t *fake_window_ptr);

/** Unit tests for window. */
extern const bs_test_case_t wlmtk_window_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW_H__ */
/* == End of window.h ====================================================== */
