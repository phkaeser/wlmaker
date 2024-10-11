/* ========================================================================= */
/**
 * @file corner.c
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

#include "corner.h"

#include <wlr/util/edges.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the hot-corner handler. */
struct _wlmaker_corner_t {
    /** Back-link to server. Required to execute actions. */
    wlmaker_server_t          *server_ptr;

    /** Cursor that is tracked here. */
    wlmaker_cursor_t          *cursor_ptr;

    /** Listener for `change` signals raised by `wlr_output_layout`. */
    struct wl_listener        output_layout_change_listener;

    /** Listener for when the cursor position was updated. */
    struct wl_listener        cursor_position_updated_listener;

    /** Current extents of the output, cached for convience. */
    struct wlr_box            extents;

    /** Pointer X coordinate, rounded to pixel position. */
    int                       pointer_x;
    /** Pointer Y coordinate, rounded to pixel position. */
    int                       pointer_y;

    /** Timer: Armed when the corner is occupied, triggers action. */
    struct wl_event_source    *timer_event_source_ptr;

    /** The cursor's current corner. 0 if not currently in a corner. */
    unsigned                  current_corner;
};

static void _wlmaker_corner_clear(wlmaker_corner_t *corner_ptr);
static void _wlmaker_corner_occupy(
    wlmaker_corner_t *corner_ptr,
    unsigned position);
static void _wlmaker_corner_update_layout(
    wlmaker_corner_t *corner_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr);
static void _wlmaker_corner_evaluate(
    wlmaker_corner_t *corner_ptr);

static int _wlmaker_corner_handle_timer(void *data_ptr);

static void _wlmaker_corner_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_corner_handle_position_updated(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_corner_t *wlmaker_corner_create(
    wlmaker_server_t *server_ptr,
    struct wl_display *wl_display_ptr,
    wlmaker_cursor_t *cursor_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    wlmaker_corner_t *corner_ptr = logged_calloc(1, sizeof(wlmaker_corner_t));
    if (NULL == corner_ptr) return NULL;
    corner_ptr->server_ptr = server_ptr;
    corner_ptr->cursor_ptr = cursor_ptr;

    // handle layout update: store the layout. Trigger evaluation.
    //
    // re-evaluate position:
    // - get current pointer position
    // - get ouput layout. if dimension is 0: clear all corners, return.
    // - for each corner X:
    //   - if in corner X: clear other timers, setup timer (if not done)
    // - it not in any corner:
    //   - clear all timers

    corner_ptr->timer_event_source_ptr = wl_event_loop_add_timer(
        wl_display_get_event_loop(wl_display_ptr),
        _wlmaker_corner_handle_timer,
        corner_ptr);
    if (NULL == corner_ptr->timer_event_source_ptr) {
        bs_log(BS_ERROR, "Failed wl_event_loop_add_timer(%p, %p, %p)",
               wl_display_get_event_loop(wl_display_ptr),
               _wlmaker_corner_handle_timer,
               corner_ptr);
        wlmaker_corner_destroy(corner_ptr);
        return NULL;
    }

    corner_ptr->pointer_x = cursor_ptr->wlr_cursor_ptr->x;
    corner_ptr->pointer_y = cursor_ptr->wlr_cursor_ptr->y;
    _wlmaker_corner_update_layout(
        corner_ptr,
        wlr_output_layout_ptr);

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &corner_ptr->output_layout_change_listener,
        _wlmaker_corner_handle_output_layout_change);
    wlmtk_util_connect_listener_signal(
        &cursor_ptr->position_updated,
        &corner_ptr->cursor_position_updated_listener,
        _wlmaker_corner_handle_position_updated);

    return corner_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_corner_destroy(wlmaker_corner_t *corner_ptr)
{
    wlmtk_util_disconnect_listener(
        &corner_ptr->cursor_position_updated_listener);
    wlmtk_util_disconnect_listener(
        &corner_ptr->output_layout_change_listener);

    if (NULL != corner_ptr->timer_event_source_ptr) {
        wl_event_source_remove(corner_ptr->timer_event_source_ptr);
        corner_ptr->timer_event_source_ptr = NULL;
    }

    free(corner_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Clears the hot-corner tracking and activation. */
void _wlmaker_corner_clear(wlmaker_corner_t *corner_ptr)
{
    if (0 == corner_ptr->current_corner) return;

    bs_log(BS_WARNING, "FIXME: Clearing corner %d", corner_ptr->current_corner);
    corner_ptr->current_corner = 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Starts occupation of a corner.
 *
 * @param corner_ptr
 * @param position
 *
 */
void _wlmaker_corner_occupy(
    wlmaker_corner_t *corner_ptr,
    unsigned position)
{
    // guard clauses: Ignore non-positions and if re-occupying same corner.
    if (0 == position) return;
    if (position == corner_ptr->current_corner) return;

    // A different corner? First clear an existing corner.
    if (position != corner_ptr->current_corner &&
        0 != corner_ptr->current_corner) {
        _wlmaker_corner_clear(corner_ptr);
    }

    // Occupy: Register event timer.
    corner_ptr->current_corner = position;
    bs_log(BS_WARNING, "FIXME: Occupy corner %d", corner_ptr->current_corner);

    wl_event_source_timer_update(corner_ptr->timer_event_source_ptr, 100);
}

/* ------------------------------------------------------------------------- */
/** Updates the output extents. Triggers a re-evaluation. */
void _wlmaker_corner_update_layout(
    wlmaker_corner_t *corner_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    if (NULL == wlr_output_layout_ptr) {
        _wlmaker_corner_clear(corner_ptr);
        return;
    }

    wlr_output_layout_get_box(
        wlr_output_layout_ptr, NULL, &corner_ptr->extents);

    _wlmaker_corner_evaluate(corner_ptr);
}

/* ------------------------------------------------------------------------- */
/** (Re)evaluates hot corner state from layout extents and pointer position. */
void _wlmaker_corner_evaluate(
    wlmaker_corner_t *corner_ptr)
{
    if (0 >= corner_ptr->extents.width || 0>= corner_ptr->extents.height) {
        bs_log(BS_INFO, "Zero extents found, clearing corner setup.");
       _wlmaker_corner_clear(corner_ptr);
        return;
    }

    struct wlr_box *e_ptr = &corner_ptr->extents;
    unsigned position = WLR_EDGE_NONE;
    if (corner_ptr->pointer_x == e_ptr->x) {
        position |= WLR_EDGE_LEFT;
    } else if (corner_ptr->pointer_x >= e_ptr->x + e_ptr->width - 1) {
        position |= WLR_EDGE_RIGHT;
    }
    if (corner_ptr->pointer_y == e_ptr->y) {
        position |= WLR_EDGE_TOP;
    } else if (corner_ptr->pointer_y >= e_ptr->y + e_ptr->height - 1) {
        position |= WLR_EDGE_BOTTOM;
    }

    switch (position) {
    case WLR_EDGE_TOP | WLR_EDGE_LEFT:
    case WLR_EDGE_TOP | WLR_EDGE_RIGHT:
    case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
    case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
        _wlmaker_corner_occupy(corner_ptr, position);
        break;
    default:
        _wlmaker_corner_clear(corner_ptr);
    }
}

/* ------------------------------------------------------------------------- */
int _wlmaker_corner_handle_timer(void *data_ptr)
{
    bs_log(BS_ERROR, "FIXME: Timer %p", data_ptr);
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles `change` events of `struct wlr_output_layout`.
 *
 * Will recompute the output's layout settings and re-evaluate the current
 * cursor position.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_corner_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_corner_t *corner_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_corner_t, output_layout_change_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    _wlmaker_corner_update_layout(
        corner_ptr,
        wlr_output_layout_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles @ref wlmaker_cursor_t::position_updated signal callbacks.
 *
 * Stores the pointer's position in @ref wlmaker_corner_t. If the position is
 * different than before, triggers a re-evaluation of whether a corner is
 * occupied.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a `struct wlr_cursor`.
 */
void _wlmaker_corner_handle_position_updated(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_corner_t *corner_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_corner_t, cursor_position_updated_listener);
    struct wlr_cursor *wlr_cursor_ptr = data_ptr;

    // Optimization: Ignore updates that are moves within the same pixel.
    if (corner_ptr->pointer_x == wlr_cursor_ptr->x &&
        corner_ptr->pointer_y == wlr_cursor_ptr->y) return;
    corner_ptr->pointer_x = wlr_cursor_ptr->x;
    corner_ptr->pointer_y = wlr_cursor_ptr->y;

    _wlmaker_corner_evaluate(corner_ptr);
}

/* == End of corner.c ====================================================== */
