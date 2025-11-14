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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "action.h"
#include "server.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/**
 * State of the hot-corner handler.
 *
 * The hot corner compoment tracks output layout and pointer position. When the
 * pointer enters any of the 4 corners of the output's bounding rectangle, a
 * timer with a 'cooldown' period is armed. If the pointer is moved before the
 * cooldown expires, the timer is disarmed, and nothing happens.
 *
 * If the pointer stays in the corner until the timer fires, we do consider the
 * corner as 'activated'.
 */
struct _wlmaker_corner_t {
    /** Back-link to server. Required to execute actions. */
    wlmaker_server_t          *server_ptr;

    /** Cursor that is tracked here. */
    wlmaker_cursor_t          *cursor_ptr;

    /** Listener for wlr_output_layout::events::change. */
    struct wl_listener        output_layout_changed_listener;

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
    /**
     * Tracks whether the corner was occoppied and the timer had fired.
     *
     * Required to trigger 'leave' actions when the corner is cleared.
     */
    bool                      corner_triggered;

    /** Configuration: Wait time before triggering 'Enter',. */
    uint64_t                  trigger_delay_msec;
    /** Action when entering the top-left corner. */
    wlmaker_action_t          top_left_enter_action;
    /** Action when leaving the top-left corner. */
    wlmaker_action_t          top_left_leave_action;
    /** Action when entering the top-right corner. */
    wlmaker_action_t          top_right_enter_action;
    /** Action when leaving the top-right corner. */
    wlmaker_action_t          top_right_leave_action;
    /** Action when entering the bottom-left corner. */
    wlmaker_action_t          bottom_left_enter_action;
    /** Action when leaving the bottom-left corner. */
    wlmaker_action_t          bottom_left_leave_action;
    /** Action when entering the bottom-right corner. */
    wlmaker_action_t          bottom_right_enter_action;
    /** Action when leaving the bottom-right corner. */
    wlmaker_action_t          bottom_right_leave_action;
};

static void _wlmaker_corner_clear(wlmaker_corner_t *corner_ptr);
static void _wlmaker_corner_occupy(
    wlmaker_corner_t *corner_ptr,
    unsigned position);
static void _wlmaker_corner_update_layout(
    wlmaker_corner_t *corner_ptr,
    struct wlr_box *extents_ptr);
static void _wlmaker_corner_evaluate(
    wlmaker_corner_t *corner_ptr);

static int _wlmaker_corner_handle_timer(void *data_ptr);

static void _wlmaker_corner_handle_output_layout_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_corner_handle_position_updated(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Descriptor for the 'HotConfig' config dictionary. */
static const bspl_desc_t _wlmaker_corner_config_desc[] = {
    BSPL_DESC_UINT64(
        "TriggerDelay", true, wlmaker_corner_t,
        trigger_delay_msec, trigger_delay_msec, 500),
    BSPL_DESC_ENUM(
        "TopLeftEnter", false, wlmaker_corner_t,
        top_left_enter_action,top_left_enter_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "TopLeftLeave", false, wlmaker_corner_t,
        top_left_leave_action, top_left_leave_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "TopRightEnter", false, wlmaker_corner_t,
        top_right_enter_action, top_right_enter_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "TopRightLeave", false, wlmaker_corner_t,
        top_right_leave_action, top_right_leave_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "BottomLeftEnter", false, wlmaker_corner_t,
        bottom_left_enter_action, bottom_left_enter_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "BottomLeftLeave", false, wlmaker_corner_t,
        bottom_left_leave_action,  bottom_left_leave_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "BottomRightEnter", false, wlmaker_corner_t,
        bottom_right_enter_action, bottom_right_enter_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_ENUM(
        "BottomRightLeave", false, wlmaker_corner_t,
        bottom_right_leave_action, bottom_right_leave_action,
        WLMAKER_ACTION_NONE, wlmaker_action_desc),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_corner_t *wlmaker_corner_create(
    bspl_dict_t *hot_corner_config_dict_ptr,
    struct wl_event_loop *wl_event_loop_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_corner_t *corner_ptr = logged_calloc(1, sizeof(wlmaker_corner_t));
    if (NULL == corner_ptr) return NULL;
    corner_ptr->server_ptr = server_ptr;
    corner_ptr->cursor_ptr = cursor_ptr;

    if (!bspl_decode_dict(
            hot_corner_config_dict_ptr,
            _wlmaker_corner_config_desc,
            corner_ptr)) {
        bs_log(BS_ERROR, "Failed to parse 'HotConfig' dict.");
        wlmaker_corner_destroy(corner_ptr);
        return NULL;
    }

    corner_ptr->timer_event_source_ptr = wl_event_loop_add_timer(
        wl_event_loop_ptr,
        _wlmaker_corner_handle_timer,
        corner_ptr);
    if (NULL == corner_ptr->timer_event_source_ptr) {
        bs_log(BS_ERROR, "Failed wl_event_loop_add_timer(%p, %p, %p)",
               wl_event_loop_ptr,
               _wlmaker_corner_handle_timer,
               corner_ptr);
        wlmaker_corner_destroy(corner_ptr);
        return NULL;
    }

    struct wlr_box extents;
    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &extents);
    corner_ptr->pointer_x = cursor_ptr->wlr_cursor_ptr->x;
    corner_ptr->pointer_y = cursor_ptr->wlr_cursor_ptr->y;
    _wlmaker_corner_update_layout(corner_ptr, &extents);

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &corner_ptr->output_layout_changed_listener,
        _wlmaker_corner_handle_output_layout_changed);
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
        &corner_ptr->output_layout_changed_listener);

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

    // Disarms the timer.
    wl_event_source_timer_update(corner_ptr->timer_event_source_ptr, 0);

    if (corner_ptr->corner_triggered) {
        wlmaker_action_t action = WLMAKER_ACTION_NONE;
        switch (corner_ptr->current_corner) {
        case WLR_EDGE_TOP | WLR_EDGE_LEFT:
            action = corner_ptr->top_left_leave_action;
            break;
        case WLR_EDGE_TOP | WLR_EDGE_RIGHT:
            action = corner_ptr->top_right_leave_action;
            break;
        case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
            action = corner_ptr->bottom_right_leave_action;
            break;
        case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
            action = corner_ptr->bottom_left_leave_action;
            break;
        default: break;
        }
        wlmaker_action_execute(corner_ptr->server_ptr, action, NULL);
        corner_ptr->corner_triggered = false;
    }
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

    // Occupy: Store the active corner and (re-arm) event timer.
    corner_ptr->current_corner = position;
    wl_event_source_timer_update(corner_ptr->timer_event_source_ptr,
                                 corner_ptr->trigger_delay_msec);
}

/* ------------------------------------------------------------------------- */
/** Updates the output extents. Triggers a re-evaluation. */
void _wlmaker_corner_update_layout(
    wlmaker_corner_t *corner_ptr,
    struct wlr_box *extents_ptr)
{
    corner_ptr->extents = *extents_ptr;
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
/** Handles timer callbacks: Sends 'enter' event and registers triggering. */
int _wlmaker_corner_handle_timer(void *data_ptr)
{
    wlmaker_corner_t *corner_ptr = data_ptr;

    corner_ptr->corner_triggered = true;

    wlmaker_action_t action = WLMAKER_ACTION_NONE;
    switch (corner_ptr->current_corner) {
    case WLR_EDGE_TOP | WLR_EDGE_LEFT:
        action = corner_ptr->top_left_enter_action;
        break;
    case WLR_EDGE_TOP | WLR_EDGE_RIGHT:
        action = corner_ptr->top_right_enter_action;
        break;
    case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
        action = corner_ptr->bottom_right_enter_action;
        break;
    case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
        action = corner_ptr->bottom_left_enter_action;
        break;
    default: break;
    }
    wlmaker_action_execute(corner_ptr->server_ptr, action, NULL);
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
 * @param data_ptr            Points to a struct wlr_output_layout.
 */
void _wlmaker_corner_handle_output_layout_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_corner_t *corner_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_corner_t, output_layout_changed_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    struct wlr_box extents;
    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &extents);
    _wlmaker_corner_update_layout(corner_ptr, &extents);
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

/* == Unit tests =========================================================== */

static void _wlmaker_corner_test(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmaker_corner_test_cases[] = {
    { true, "test", _wlmaker_corner_test },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_corner_test_set = BS_TEST_SET(
    true, "corner", wlmaker_corner_test_cases);

/* ------------------------------------------------------------------------- */
/** Exercises the hot corner module. */
void _wlmaker_corner_test(bs_test_t *test_ptr)
{
    bspl_object_t *obj_ptr = bspl_create_object_from_plist_string(
        "{"
        "TriggerDelay = 500;"
        "}");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, obj_ptr);

    struct wl_event_loop *wl_event_loop_ptr = wl_event_loop_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_event_loop_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);

    struct wlr_cursor wlr_cursor = {};
    wlmaker_cursor_t cursor = { .wlr_cursor_ptr = &wlr_cursor };
    wl_signal_init(&cursor.position_updated);
    wlmaker_server_t server = {};
    wlmaker_corner_t *c_ptr = wlmaker_corner_create(
        bspl_dict_from_object(obj_ptr),
        wl_event_loop_ptr,
        wlr_output_layout_ptr,
        &cursor,
        &server);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c_ptr);

    BS_TEST_VERIFY_EQ(test_ptr, 500, c_ptr->trigger_delay_msec);

    BS_TEST_VERIFY_EQ(
        test_ptr, 0, c_ptr->current_corner);

    // Set dimensions. Pointer still at (0, 0), that's top-left.
    struct wlr_output output = { .width = 640, .height = 480, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLR_EDGE_TOP | WLR_EDGE_LEFT, c_ptr->current_corner);
    BS_TEST_VERIFY_FALSE(test_ptr, c_ptr->corner_triggered);

    // Move pointer to
    wlr_cursor.x = 639;
    wlr_cursor.y = 479;
    wl_signal_emit(&cursor.position_updated, &wlr_cursor);
    BS_TEST_VERIFY_EQ(
        test_ptr, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT, c_ptr->current_corner);
    BS_TEST_VERIFY_FALSE(test_ptr, c_ptr->corner_triggered);

    // Pretend the timer expired.
    _wlmaker_corner_handle_timer(c_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, c_ptr->corner_triggered);

    // Move pointer: Clears triggers.
    wlr_cursor.x = 320;
    wlr_cursor.y = 240;
    wl_signal_emit(&cursor.position_updated, &wlr_cursor);
    BS_TEST_VERIFY_EQ(
        test_ptr, 0, c_ptr->current_corner);
    BS_TEST_VERIFY_FALSE(test_ptr, c_ptr->corner_triggered);

    wlmaker_corner_destroy(c_ptr);
    wl_display_destroy(wl_display_ptr);
    wl_event_loop_destroy(wl_event_loop_ptr);
    bspl_object_unref(obj_ptr);
}

/* == End of corner.c ====================================================== */
