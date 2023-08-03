/* ========================================================================= */
/**
 * @file cursor.c
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

#include "cursor.h"

#include "config.h"
#include "util.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_xcursor_manager.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void handle_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_motion_absolute(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_button(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_axis(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_seat_request_set_cursor(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void process_motion(wlmaker_cursor_t *cursor_ptr, uint32_t time_msec);
static void update_under_cursor_view(wlmaker_cursor_t *cursor_ptr,
                                     wlmaker_view_t *view_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_cursor_t *wlmaker_cursor_create(wlmaker_server_t *server_ptr)
{
    wlmaker_cursor_t *cursor_ptr = logged_calloc(1, sizeof(wlmaker_cursor_t));
    if (NULL == cursor_ptr) return NULL;
    cursor_ptr->server_ptr = server_ptr;

    // tinywl: wlr_cursor is a utility tracking the cursor image shown on
    // screen.
    cursor_ptr->wlr_cursor_ptr = wlr_cursor_create();
    if (NULL == cursor_ptr->wlr_cursor_ptr) {
        bs_log(BS_ERROR, "Failed wlr_cursor_create()");
        wlmaker_cursor_destroy(cursor_ptr);
        return NULL;
    }
    // Must be initialized after wlr_output_layout_ptr.
    BS_ASSERT(NULL != server_ptr->wlr_output_layout_ptr);
    wlr_cursor_attach_output_layout(cursor_ptr->wlr_cursor_ptr,
                                    server_ptr->wlr_output_layout_ptr);

    cursor_ptr->wlr_xcursor_manager_ptr = wlr_xcursor_manager_create(
        config_xcursor_theme_name,
        config_xcursor_theme_size);
    if (NULL == cursor_ptr->wlr_xcursor_manager_ptr) {
        bs_log(BS_ERROR, "Failed wlr_xcursor_manager_create(%s, %"PRIu32")",
               config_xcursor_theme_name, config_xcursor_theme_size);
        wlmaker_cursor_destroy(cursor_ptr);
        return NULL;
    }
    if (!wlr_xcursor_manager_load(cursor_ptr->wlr_xcursor_manager_ptr,
                                  config_output_scale)) {

        bs_log(BS_ERROR, "Failed wlr_xcursor_manager_load() for %s, %"PRIu32,
               config_xcursor_theme_name, config_xcursor_theme_size);
        wlmaker_cursor_destroy(cursor_ptr);
        return NULL;
    }

    // tinywl: wlr_cursor *only* displays an image on screen. It does not move
    // around when the pointer moves. However, we can attach input devices to
    // it, and it will generate aggregate events for all of them. In these
    // events, we can choose how we want to process them, forwarding them to
    // clients and moving the cursor around. More detail on this process is
    // described in the input handling blog post:
    //
    // https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html

    // TODO: Need a mode for 'normal', 'move', 'resize'.
    wlm_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.motion,
        &cursor_ptr->motion_listener,
        handle_motion);
    wlm_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.motion_absolute,
        &cursor_ptr->motion_absolute_listener,
        handle_motion_absolute);
    wlm_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.button,
        &cursor_ptr->button_listener,
        handle_button);
    wlm_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.axis,
        &cursor_ptr->axis_listener,
        handle_axis);
    wlm_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.frame,
        &cursor_ptr->frame_listener,
        handle_frame);

    wlm_util_connect_listener_signal(
        &cursor_ptr->server_ptr->wlr_seat_ptr->events.request_set_cursor,
        &cursor_ptr->seat_request_set_cursor_listener,
        handle_seat_request_set_cursor);

    wl_signal_init(&cursor_ptr->button_release_event);
    return cursor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_destroy(wlmaker_cursor_t *cursor_ptr)
{
    if (NULL != cursor_ptr->wlr_xcursor_manager_ptr) {
        wlr_xcursor_manager_destroy(cursor_ptr->wlr_xcursor_manager_ptr);
        cursor_ptr->wlr_xcursor_manager_ptr = NULL;
    }

    if (NULL != cursor_ptr->wlr_cursor_ptr) {
        wlr_cursor_destroy(cursor_ptr->wlr_cursor_ptr);
        cursor_ptr->wlr_cursor_ptr = NULL;
    }

    free(cursor_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_attach_input_device(
    wlmaker_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr)
{
     wlr_cursor_attach_input_device(
         cursor_ptr->wlr_cursor_ptr,
         wlr_input_device_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_begin_move(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr)
{
    if (view_ptr != wlmaker_workspace_get_activated_view(
            wlmaker_server_get_current_workspace(cursor_ptr->server_ptr))) {
        bs_log(BS_WARNING, "Denying move request from non-activated view.");
        return;
    }

    cursor_ptr->grabbed_view_ptr = view_ptr;
    int x, y;
    wlmaker_view_get_position(cursor_ptr->grabbed_view_ptr, &x, &y);
    // TODO(kaeser@gubbe.ch): Inconsistent to have separate meaning of grab_x
    // and grab_y for MOVE vs RESIZE. Should be cleaned up.
    cursor_ptr->grab_x = cursor_ptr->wlr_cursor_ptr->x - x;
    cursor_ptr->grab_y = cursor_ptr->wlr_cursor_ptr->y - y;
    cursor_ptr->mode = WLMAKER_CURSOR_MOVE;
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_begin_resize(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    uint32_t edges)
{
    if (view_ptr != wlmaker_workspace_get_activated_view(
            wlmaker_server_get_current_workspace(cursor_ptr->server_ptr))) {
        bs_log(BS_WARNING, "Denying resize request from non-activated view.");
        return;
    }

    cursor_ptr->grabbed_view_ptr = view_ptr;
    cursor_ptr->grab_x = cursor_ptr->wlr_cursor_ptr->x;
    cursor_ptr->grab_y = cursor_ptr->wlr_cursor_ptr->y;
    cursor_ptr->mode = WLMAKER_CURSOR_RESIZE;

    uint32_t width, height;
    wlmaker_view_get_size(view_ptr, &width, &height);
    cursor_ptr->grabbed_geobox.width = width;
    cursor_ptr->grabbed_geobox.height = height;
    wlmaker_view_get_position(view_ptr,
                              &cursor_ptr->grabbed_geobox.x,
                              &cursor_ptr->grabbed_geobox.y);
    cursor_ptr->resize_edges = edges;
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_unmap_view(
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr)
{
    if (cursor_ptr->grabbed_view_ptr == view_ptr) {
        cursor_ptr->grabbed_view_ptr = NULL;
        cursor_ptr->mode = WLMAKER_CURSOR_PASSTHROUGH;
    }

    if (cursor_ptr->under_cursor_view_ptr == view_ptr) {
        // TODO(kaeser@gubbe.ch): Should eavluate which of the view is now
        // below the cursor and update "pointer focus" accordingly.
        update_under_cursor_view(cursor_ptr, NULL);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_cursor_get_position(
    const wlmaker_cursor_t *cursor_ptr,
    double *x_ptr,
    double *y_ptr)
{
    if (NULL != x_ptr) *x_ptr = cursor_ptr->wlr_cursor_ptr->x;
    if (NULL != y_ptr) *y_ptr = cursor_ptr->wlr_cursor_ptr->y;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `motion` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_motion_event`.
 */
void handle_motion(struct wl_listener *listener_ptr,
                   void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, motion_listener);
    struct wlr_pointer_motion_event *wlr_pointer_motion_event_ptr = data_ptr;

    wlr_cursor_move(
        cursor_ptr->wlr_cursor_ptr,
        &wlr_pointer_motion_event_ptr->pointer->base,
        wlr_pointer_motion_event_ptr->delta_x,
        wlr_pointer_motion_event_ptr->delta_y);

    process_motion(
        cursor_ptr,
        wlr_pointer_motion_event_ptr->time_msec);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `motion_absolute` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_motion_absolute_event`.
 */
void handle_motion_absolute(struct wl_listener *listener_ptr,
                            void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, motion_absolute_listener);
    struct wlr_pointer_motion_absolute_event
        *wlr_pointer_motion_absolute_event_ptr = data_ptr;

    wlr_cursor_warp_absolute(
        cursor_ptr->wlr_cursor_ptr,
        &wlr_pointer_motion_absolute_event_ptr->pointer->base,
        wlr_pointer_motion_absolute_event_ptr->x,
        wlr_pointer_motion_absolute_event_ptr->y);

    process_motion(
        cursor_ptr,
        wlr_pointer_motion_absolute_event_ptr->time_msec);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `button` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_button_event`.
 */
void handle_button(struct wl_listener *listener_ptr,
                   void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, button_listener);
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr = data_ptr;

    struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
        cursor_ptr->server_ptr->wlr_seat_ptr);
    if (NULL != wlr_keyboard_ptr) {
        uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_keyboard_ptr);
        if (wlmaker_config_window_drag_modifiers != 0 &&
            wlmaker_config_window_drag_modifiers == modifiers &&
            wlr_pointer_button_event_ptr->state == WLR_BUTTON_PRESSED) {
            struct wlr_surface *wlr_surface_ptr;
            double rel_x, rel_y;
            wlmaker_view_t *view_ptr = wlmaker_view_at(
                cursor_ptr->server_ptr,
                cursor_ptr->wlr_cursor_ptr->x,
                cursor_ptr->wlr_cursor_ptr->y,
                &wlr_surface_ptr,
                &rel_x,
                &rel_y);
            if (NULL != view_ptr) {
                wlmaker_workspace_raise_view(
                    wlmaker_server_get_current_workspace(
                        cursor_ptr->server_ptr),
                    view_ptr);
                wlmaker_workspace_activate_view(
                    wlmaker_server_get_current_workspace(
                        cursor_ptr->server_ptr),
                    view_ptr);
                update_under_cursor_view(cursor_ptr, view_ptr);
                wlmaker_cursor_begin_move(cursor_ptr, view_ptr);
                return;
            }
        }
    }

    // Notify the client with pointer focus that a button press has occurred.
    wlr_seat_pointer_notify_button(
        cursor_ptr->server_ptr->wlr_seat_ptr,
        wlr_pointer_button_event_ptr->time_msec,
        wlr_pointer_button_event_ptr->button,
        wlr_pointer_button_event_ptr->state);

    // Let the view take action on the button press.
    struct wlr_surface *wlr_surface_ptr;
    double rel_x, rel_y;
    wlmaker_view_t *view_ptr = wlmaker_view_at(
        cursor_ptr->server_ptr,
        cursor_ptr->wlr_cursor_ptr->x,
        cursor_ptr->wlr_cursor_ptr->y,
        &wlr_surface_ptr,
        &rel_x,
        &rel_y);
    if (NULL != view_ptr) {
        wlmaker_view_handle_button(
            view_ptr,
            cursor_ptr->wlr_cursor_ptr->x,
            cursor_ptr->wlr_cursor_ptr->y,
            wlr_pointer_button_event_ptr);
    }
    update_under_cursor_view(cursor_ptr, view_ptr);

    if (wlr_pointer_button_event_ptr->state == WLR_BUTTON_RELEASED) {
        wl_signal_emit(&cursor_ptr->button_release_event, data_ptr);
        cursor_ptr->mode = WLMAKER_CURSOR_PASSTHROUGH;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `axis` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_axis_event`.
 */
void handle_axis(struct wl_listener *listener_ptr,
                 void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, axis_listener);
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr = data_ptr;

    /* Notify the client with pointer focus of the axis event. */
    wlr_seat_pointer_notify_axis(
        cursor_ptr->server_ptr->wlr_seat_ptr,
        wlr_pointer_axis_event_ptr->time_msec,
        wlr_pointer_axis_event_ptr->orientation,
        wlr_pointer_axis_event_ptr->delta,
        wlr_pointer_axis_event_ptr->delta_discrete,
        wlr_pointer_axis_event_ptr->source);

    // Let the view take action on the button press.
    struct wlr_surface *wlr_surface_ptr;
    double rel_x, rel_y;
    wlmaker_view_t *view_ptr = wlmaker_view_at(
        cursor_ptr->server_ptr,
        cursor_ptr->wlr_cursor_ptr->x,
        cursor_ptr->wlr_cursor_ptr->y,
        &wlr_surface_ptr,
        &rel_x,
        &rel_y);
    if (NULL != view_ptr) {
        wlmaker_view_handle_axis(
            view_ptr,
            cursor_ptr->wlr_cursor_ptr->x,
            cursor_ptr->wlr_cursor_ptr->y,
            wlr_pointer_axis_event_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `frame` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_frame(struct wl_listener *listener_ptr,
                  __UNUSED__ void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, frame_listener);

    /* Notify the client with pointer focus of the frame event. */
    wlr_seat_pointer_notify_frame(cursor_ptr->server_ptr->wlr_seat_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_set_cursor` event of `wlr_seat`.
 *
 * This event is raised when a client provides a cursor image. It is accepted
 * only if the client also has the pointer focus.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_seat_pointer_request_set_cursor_event`.
 */
void handle_seat_request_set_cursor(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_cursor_t, seat_request_set_cursor_listener);
    struct wlr_seat_pointer_request_set_cursor_event
        *wlr_seat_pointer_request_set_cursor_event_ptr = data_ptr;

    struct wlr_seat_client *focused_wlr_seat_client_ptr =
        cursor_ptr->server_ptr->wlr_seat_ptr->pointer_state.focused_client;
    if (focused_wlr_seat_client_ptr ==
        wlr_seat_pointer_request_set_cursor_event_ptr->seat_client) {
        wlr_cursor_set_surface(
            cursor_ptr->wlr_cursor_ptr,
            wlr_seat_pointer_request_set_cursor_event_ptr->surface,
            wlr_seat_pointer_request_set_cursor_event_ptr->hotspot_x,
            wlr_seat_pointer_request_set_cursor_event_ptr->hotspot_y);

    } else {
        bs_log(BS_WARNING, "request_set_cursor called without pointer focus.");
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Processes the cursor motion: Lookups up the view & surface under the
 * pointer and sets (respectively: clears) the pointer focus. Also sets the
 * default cursor image, if no view is given (== no client-side cursor).
 *
 * @param cursor_ptr
 * @param time_msec
 */
void process_motion(wlmaker_cursor_t *cursor_ptr, uint32_t time_msec)
{
    if (cursor_ptr->mode == WLMAKER_CURSOR_MOVE) {
        wlmaker_view_set_position(
            cursor_ptr->grabbed_view_ptr,
            cursor_ptr->wlr_cursor_ptr->x - cursor_ptr->grab_x,
            cursor_ptr->wlr_cursor_ptr->y - cursor_ptr->grab_y);
        return;
    } else if (cursor_ptr->mode == WLMAKER_CURSOR_RESIZE) {

        // The geometry describes the overall shell geometry *relative* to the
        // node position. This may eg. include client-side decoration, that
        // may be placed in an extra surface above the nominal window (and
        // node).
        //
        // Thus the position and dimensions of the visible area is given by
        // the geobox position (relative to the node position) and with x height.

        double x = cursor_ptr->wlr_cursor_ptr->x - cursor_ptr->grab_x;
        double y = cursor_ptr->wlr_cursor_ptr->y - cursor_ptr->grab_y;

        // Update new boundaries by the relative movement.
        int top = cursor_ptr->grabbed_geobox.y;
        int bottom = cursor_ptr->grabbed_geobox.y + cursor_ptr->grabbed_geobox.height;
        if (cursor_ptr->resize_edges & WLR_EDGE_TOP) {
            top += y;
            if (top >= bottom) top = bottom - 1;
        } else if (cursor_ptr->resize_edges & WLR_EDGE_BOTTOM) {
            bottom += y;
            if (bottom <= top) bottom = top + 1;
        }

        int left = cursor_ptr->grabbed_geobox.x;
        int right = cursor_ptr->grabbed_geobox.x + cursor_ptr->grabbed_geobox.width;
        if (cursor_ptr->resize_edges & WLR_EDGE_LEFT) {
            left += x;
            if (left >= right) left = right - 1 ;
        } else if (cursor_ptr->resize_edges & WLR_EDGE_RIGHT) {
            right += x;
            if (right <= left) right = left + 1;
        }

        wlmaker_view_set_position(cursor_ptr->grabbed_view_ptr, left, top);
        wlmaker_view_set_size(cursor_ptr->grabbed_view_ptr,
                              right - left, bottom - top);
        return;
    }

    double rel_x, rel_y;
    struct wlr_surface *wlr_surface_ptr = NULL;
    wlmaker_view_t *view_ptr = wlmaker_view_at(
        cursor_ptr->server_ptr,
        cursor_ptr->wlr_cursor_ptr->x,
        cursor_ptr->wlr_cursor_ptr->y,
        &wlr_surface_ptr,
        &rel_x,
        &rel_y);
    update_under_cursor_view(cursor_ptr, view_ptr);
    if (NULL == view_ptr) {
        wlr_xcursor_manager_set_cursor_image(
            cursor_ptr->wlr_xcursor_manager_ptr,
            "left_ptr",
            cursor_ptr->wlr_cursor_ptr);
    } else {
        wlmaker_view_handle_motion(
            view_ptr,
            cursor_ptr->wlr_cursor_ptr->x,
            cursor_ptr->wlr_cursor_ptr->y);
    }

    if (NULL == wlr_surface_ptr) {
        // Clear pointer focus, so that future button events are no longer sent
        // to the surface that had the focus.
        wlr_seat_pointer_clear_focus(cursor_ptr->server_ptr->wlr_seat_ptr);

    } else {
        // The notify_enter() function gives the pointer focus to the specified
        // surface. The seat will send future button events there.
        wlr_seat_pointer_notify_enter(
            cursor_ptr->server_ptr->wlr_seat_ptr,
            wlr_surface_ptr,
            rel_x,
            rel_y);
        wlr_seat_pointer_notify_motion(
            cursor_ptr->server_ptr->wlr_seat_ptr,
            time_msec,
            rel_x,
            rel_y);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Updates which view currently has "cursor focus". This is used to notify the
 * view when the cursor exits it's region.
 *
 * @param cursor_ptr
 * @param view_ptr
 */
void update_under_cursor_view(wlmaker_cursor_t *cursor_ptr,
                              wlmaker_view_t *view_ptr)
{
    // Nothing to do if ther was no change.
    if (cursor_ptr->under_cursor_view_ptr == view_ptr) return;

    // Otherwise: Send a 'LEAVE' notification to the former view
    if (NULL != cursor_ptr->under_cursor_view_ptr) {
        wlmaker_view_cursor_leave(cursor_ptr->under_cursor_view_ptr);
    }

    cursor_ptr->under_cursor_view_ptr = view_ptr;
}

/* == End of cursor.c ====================================================== */
