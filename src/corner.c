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

    /** Listener for when the cursor is moved. */
    struct wl_listener        cursor_motion_listener;

    /** Current extents of the output, cached for convience. */
    struct wlr_box            extents;

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

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_corner_t *wlmaker_corner_create(
    wlmaker_server_t *server_ptr,
    wlmaker_cursor_t *cursor_ptr)
{
    wlmaker_corner_t *corner_ptr = logged_calloc(1, sizeof(wlmaker_corner_t));
    if (NULL == corner_ptr) return NULL;
    corner_ptr->server_ptr = server_ptr;
    server_ptr->cursor_ptr = cursor_ptr;

    // handle layout update: store the layout. Trigger evaluation.
    //
    // re-evaluate position:
    // - get current pointer position
    // - get ouput layout. if dimension is 0: clear all corners, return.
    // - for each corner X:
    //   - if in corner X: clear other timers, setup timer (if not done)
    // - it not in any corner:
    //   - clear all timers

    _wlmaker_corner_update_layout(
        corner_ptr,
        server_ptr->wlr_output_layout_ptr);

    return corner_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_corner_destroy(wlmaker_corner_t *corner_ptr)
{
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
 * Starts occupatoin of a corner.
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
/** (Re)evaluates the hot corner state. */
void _wlmaker_corner_evaluate(
    wlmaker_corner_t *corner_ptr)
{
    if (0 >= corner_ptr->extents.width || 0>= corner_ptr->extents.height) {
        bs_log(BS_INFO, "Zero extents found, clearing corner setup.");
       _wlmaker_corner_clear(corner_ptr);
        return;
    }

    double x, y;
    wlmaker_cursor_get_position(corner_ptr->cursor_ptr, &x, &y);

    struct wlr_box *extents_ptr = &corner_ptr->extents;
    unsigned position = WLR_EDGE_NONE;
    if (floor(x) == extents_ptr->x) {
        position |= WLR_EDGE_LEFT;
    } else if (floor(x) == extents_ptr->x + extents_ptr->width) {
        position |= WLR_EDGE_RIGHT;
    }
    if (floor(y) == extents_ptr->y) {
        position |= WLR_EDGE_TOP;
    } else if (floor(y) == extents_ptr->y + extents_ptr->height) {
        position |= WLR_EDGE_RIGHT;
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

    // OK: We are in a corner. If this is the same we already have: No-op.
    if (position == corner_ptr->current_corner) return;

    // Reset the timer and store positoin.
    corner_ptr->current_corner = position;
}

/* == End of corner.c ====================================================== */
