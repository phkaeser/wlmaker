/* ========================================================================= */
/**
 * @file cursor.h
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
#ifndef __CURSOR_H__
#define __CURSOR_H__

/** Forward declaration of wlmaker cursor state. */
typedef struct _wlmaker_cursor_t wlmaker_cursor_t;

#include "server.h"
#include "view.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Mode of the cursor. */
typedef enum {
    /** Cursor movements are passed on to the client. */
    WLMAKER_CURSOR_PASSTHROUGH,
    /** View-move mode. Movements kept and used to propel the view. */
    WLMAKER_CURSOR_MOVE,
    /** Resize mode. Movements kept and used to resize the view. */
    WLMAKER_CURSOR_RESIZE,
} wlmaker_cursor_mode_t;

/** State and tools for handling wlmaker cursors. */
struct _wlmaker_cursor_t {
    /** Back-link to wlmaker_server_t. */
    wlmaker_server_t          *server_ptr;

    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;

    /** Listener for the `motion` event of `wlr_cursor`. */
    struct wl_listener        motion_listener;
    /** Listener for the `motion_absolute` event of `wlr_cursor`. */
    struct wl_listener        motion_absolute_listener;
    /** Listener for the `button` event of `wlr_cursor`. */
    struct wl_listener        button_listener;
    /** Listener for the `axis` event of `wlr_cursor`. */
    struct wl_listener        axis_listener;
    /** Listener for the `frame` event of `wlr_cursor`. */
    struct wl_listener        frame_listener;

    /** Listener for the `request_set_cursor` event of `wlr_seat`. */
    struct wl_listener        seat_request_set_cursor_listener;

    /** Mode that the cursor is in, eg. moving or resizing. */
    wlmaker_cursor_mode_t     mode;
    /** The currently grabbed view, when in "move" mode. */
    wlmaker_view_t            *grabbed_view_ptr;
    /** The view that is currently active and under the cursor. */
    wlmaker_view_t            *under_cursor_view_ptr;
    /**
     * Horizontal position of when the move was activated, relative to the
     * grabbed view.
     */
    double                    grab_x;
    /**
     * Vertical position of when the move was activated, relative to the
     * grabbed view.
     */
    double                    grab_y;
    /** Geometry at the time the move was initiated. */
    struct wlr_box            grabbed_geobox;
    /** Edges to resize along. */
    uint32_t                  resize_edges;


    /** wlmaker internal: catch 'release' events of cursors. */
    struct wl_signal          button_release_event;
};

/**
 * Creates the cursor handlers.
 *
 * @param server_ptr
 *
 * @return The cursor handler or NULL on error.
 */
wlmaker_cursor_t *wlmaker_cursor_create(wlmaker_server_t *server_ptr);

/**
 * Destroys the cursor handlers.
 *
 * @param cursor_ptr
 */
void wlmaker_cursor_destroy(wlmaker_cursor_t *cursor_ptr);

/**
 * Attaches the input device. May be a pointer, touch or tablet_tool device.
 *
 * @param cursor_ptr
 * @param wlr_input_device_ptr
 */
void wlmaker_cursor_attach_input_device(
    wlmaker_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr);

/**
 * Begins a "move" interaction for the given view.
 *
 * @param cursor_ptr
 * @param view_ptr
 */
void wlmaker_cursor_begin_move(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Begins a "resize" interaction for the given view.
 *
 * @param cursor_ptr
 * @param view_ptr
 * @param edges
 */
void wlmaker_cursor_begin_resize(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    uint32_t edges);

/**
 * Reports |view_ptr| as unmapped. Removes it from the set of views that can
 * be called back, etc.
 *
 * @param cursor_ptr
 * @param view_ptr
 */
void wlmaker_cursor_unmap_view(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr);

/**
 * Retrieves the current pointer's position into |*x_ptr|, |*y_ptr|.
 *
 * @param cursor_ptr
 * @param x_ptr               May be NULL.
 * @param y_ptr               May be NULL.
 */
void wlmaker_cursor_get_position(
    const wlmaker_cursor_t *cursor_ptr,
    double *x_ptr,
    double *y_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CURSOR_H__ */
/* == End of cursor.h ====================================================== */
