/* ========================================================================= */
/**
 * @file toplevel.h
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
#ifndef __WLMTK_TOPLEVEL_H__
#define __WLMTK_TOPLEVEL_H__

/** Forward declaration: Toplevel. */
typedef struct _wlmtk_toplevel_t wlmtk_toplevel_t;
/** Forward declaration: Virtual method table. */
typedef struct _wlmtk_toplevel_vmt_t wlmtk_toplevel_vmt_t;

#include "bordered.h"
#include "box.h"
#include "content.h"
#include "element.h"
#include "resizebar.h"
#include "titlebar.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a toplevel for the given content.
 *
 * @param env_ptr
 * @param content_ptr         Will take ownership of content_ptr.
 *
 * @return Pointer to the toplevel state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_toplevel_destroy.
 */
wlmtk_toplevel_t *wlmtk_toplevel_create(
    wlmtk_env_t *env_ptr,
    wlmtk_content_t *content_ptr);

/**
 * Destroys the toplevel.
 *
 * @param toplevel_ptr
 */
void wlmtk_toplevel_destroy(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Returns the super Element of the toplevel.
 *
 * TODO(kaeser@gubbe.ch): Re-evaluate whether to work with accessors, or
 *     whether to keep the ancestry members public.
 *
 * @param toplevel_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     toplevel_ptr.
 */
wlmtk_element_t *wlmtk_toplevel_element(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Returns the toplevel from the super Element.
 *
 * @param element_ptr
 *
 * @return Pointer to the @ref wlmtk_toplevel_t, for which `element_ptr` is
 *     the ancestor.
 */
wlmtk_toplevel_t *wlmtk_toplevel_from_element(wlmtk_element_t *element_ptr);

/**
 * Sets the toplevel as activated, depending on the argument's value.
 *
 * An activated toplevel will have keyboard focus and would have distinct
 * decorations to indicate state.
 *
 * @param toplevel_ptr
 * @param activated
 */
void wlmtk_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated);

/**
 * Sets whether to have server-side decorations for this toplevel.
 *
 * @param toplevel_ptr
 * @param decorated
 */
void wlmtk_toplevel_set_server_side_decorated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool decorated);

/**
 * Sets the title for the toplevel.
 *
 * If `title_ptr` is NULL, a generic name is set.
 *
 * @param toplevel_ptr
 * @param title_ptr           May be NULL.
 */
void wlmtk_toplevel_set_title(
    wlmtk_toplevel_t *toplevel_ptr,
    const char *title_ptr);

/**
 * Returns the title of the toplevel.
 *
 * @param toplevel_ptr
 *
 * @returns Pointer to the toplevel title. Will remain valid until the next call
 *     to @ref wlmtk_toplevel_set_title, or until the toplevel is destroyed. Will
 *     never be NULL.
 */
const char *wlmtk_toplevel_get_title(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Requests to close the toplevel.
 *
 * @param toplevel_ptr
 */
void wlmtk_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Requests to minimize (iconify) the toplevel.
 *
 * @param toplevel_ptr
 */
void wlmtk_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Reuests the toplevel to be maximized.
 *
 * Requires the toplevel to be mapped (to a workspace). Will lookup the maximize
 * extents from the workspace, and request a corresponding updated position and
 * size for the toplevel. @ref wlmtk_toplevel_t::organic_size will not be updated.
 *
 * This may be implemented as an asynchronous operation. Maximization will be
 * applied once the size change has been committed by the content.
 *
 * @param toplevel_ptr
 * @param maximized
 */
void wlmtk_toplevel_request_maximize(
    wlmtk_toplevel_t *toplevel_ptr,
    bool maximized);

/** Returns whether the toplevel is currently (requested to be) maximized. */
bool wlmtk_toplevel_maximized(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Requests a move for the toplevel.
 *
 * Requires the toplevel to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_toplevel_move.
 *
 * @param toplevel_ptr
 */
void wlmtk_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr);

/**
 * Requests the toplevel to be resized.
 *
 * Requires the toplevel to be mapped (to a workspace), and forwards the call to
 * @ref wlmtk_workspace_begin_toplevel_resize.
 *
 * @param toplevel_ptr
 * @param edges
 */
void wlmtk_toplevel_request_resize(wlmtk_toplevel_t *toplevel_ptr,
                                 uint32_t edges);

/**
 * Sets the toplevel's position. This is a synchronous operation.
 *
 * Updates the position in @ref wlmtk_toplevel_t::organic_size.
 * @param toplevel_ptr
 * @param x
 * @param y
 */
void wlmtk_toplevel_set_position(wlmtk_toplevel_t *toplevel_ptr, int x, int y);

/**
 * Obtains the size of the toplevel, including potential decorations.
 *
 * @param toplevel_ptr
 * @param width_ptr           May be NULL.
 * @param height_ptr          May be NULL.
 */
void wlmtk_toplevel_get_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int *width_ptr,
    int *height_ptr);

/**
 * Requests a new size for the toplevel, including potential decorations.
 *
 * This may be implemented as an asynchronous operation.
 *
 * @param toplevel_ptr
 * @param width
 * @param height
 */
void wlmtk_toplevel_request_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int width,
    int height);

/**
 * Returns the current position and size of the toplevel.
 *
 * @param toplevel_ptr
 *
 * @return The position of the toplevel (the toplevel's element), and the currently
 *     committed width and height of the toplevel.
 */
struct wlr_box wlmtk_toplevel_get_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr);

/**
 * Requests an updated position and size for the toplevel, including potential
 * decorations.
 *
 * This may be implemented as an asynchronous operation. The re-positioning
 * will be applied only once the size change has been committed by the client.
 *
 * The position and size will be stored in @ref wlmtk_toplevel_t::organic_size.
 *
 * @param toplevel_ptr
 * @param x
 * @param y
 * @param width
 * @param height
 */
void wlmtk_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height);

/**
 * Updates the toplevel state to what was requested at the `serial`.
 *
 * Used for example when resizing a toplevel from the top or left edges. In that
 * case, @ref wlmtk_content_request_size may be asynchronous and returns a
 * serial. The content is expected to call @ref wlmtk_toplevel_serial with the
 * returned serial when the size is committed.
 * Only then, the corresponding positional update on the top/left edges are
 * supposed to be applied.
 *
 * @ref wlmtk_toplevel_t::organic_size will be updated, if there was no pending
 * update: Meaning that the commit originated not from an earlier
 * @ref wlmtk_toplevel_request_position_and_size or @ref
 * wlmtk_toplevel_request_maximize call.
 *
 * @param toplevel_ptr
 * @param serial
 */
void wlmtk_toplevel_serial(wlmtk_toplevel_t *toplevel_ptr, uint32_t serial);

/* ------------------------------------------------------------------------- */

/** State of the fake toplevel, for tests. */
typedef struct {
    /** Toplevel state. */
    wlmtk_toplevel_t            *toplevel_ptr;
    /** Fake content, to manipulate the fake toplevel's content. */
    wlmtk_fake_content_t      *fake_content_ptr;

    /** Argument to last @ref wlmtk_toplevel_set_activated call. */
    bool                      activated;
    /** Whether @ref wlmtk_toplevel_request_close was called. */
    bool                      request_close_called;
    /** Whether @ref wlmtk_toplevel_request_minimize was called. */
    bool                      request_minimize_called;
   /** Whether @ref wlmtk_toplevel_request_move was called. */
    bool                      request_move_called;
    /** Whether @ref wlmtk_toplevel_request_resize was called. */
    bool                      request_resize_called;
    /** Argument to last @ref wlmtk_toplevel_request_resize call. */
    uint32_t                  request_resize_edges;
    /** Whether @ref wlmtk_toplevel_request_position_and_size was called. */
    bool                      request_position_and_size_called;
    /** Argument to last @ref wlmtk_toplevel_request_size call. */
    int                       x;
    /** Argument to last @ref wlmtk_toplevel_request_size call. */
    int                       y;
    /** Argument to last @ref wlmtk_toplevel_request_size call. */
    int                       width;
    /** Argument to last @ref wlmtk_toplevel_request_size call. */
    int                       height;
} wlmtk_fake_toplevel_t;

/** Ctor. */
wlmtk_fake_toplevel_t *wlmtk_fake_toplevel_create(void);
/** Dtor. */
void wlmtk_fake_toplevel_destroy(wlmtk_fake_toplevel_t *fake_toplevel_ptr);

/** Unit tests for toplevel. */
extern const bs_test_case_t wlmtk_toplevel_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TOPLEVEL_H__ */
/* == End of toplevel.h ==================================================== */
