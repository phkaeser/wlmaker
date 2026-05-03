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

#include <ini.h>
#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State and tools for handling wlmaker cursors. */
struct _wlmim_cursor_t {
    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;

    /** The toolkit wrapper for above. */
    wlmtk_pointer_t           *pointer_ptr;

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

    /** Back-link to input manager. */
    wlmim_t                   *input_manager_ptr;
    /** WLR seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** Root of the compositor. */
    wlmtk_root_t              *root_ptr;
};

static void _wlmim_cursor_handle_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_cursor_handle_motion_absolute(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_cursor_handle_button(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_cursor_handle_axis(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_cursor_handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmim_cursor_handle_seat_request_set_cursor(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmim_cursor_process_motion(
    wlmim_cursor_t *cursor_ptr,
    uint32_t time_msec);

char *_wlmim_cursor_get_theme_name(const struct wlmim_cursor_style *style_ptr);
static int _wlmim_cursor_theme_handler(
    void *ud_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr);

/* == Data ================================================================= */

const bspl_desc_t             wlmim_cursor_style_desc[] = {
    BSPL_DESC_BOOL(
        "OverrideSystemConfiguration", false, struct wlmim_cursor_style,
        override_system_configuration, override_system_configuration, false),
    BSPL_DESC_STRING(
        "Name", true, struct wlmim_cursor_style, name_ptr, name_ptr,
        "default"),
    BSPL_DESC_UINT64(
        "Size", true, struct wlmim_cursor_style, size, size, 24),
    BSPL_DESC_SENTINEL()
};

/** Name of the system-wide configuration file for cursor themes. */
static const char *_wlmim_cursor_system_config_file =
    "/usr/share/icons/default/index.theme";
/** The debian specific versino of the system-wide cursor configuration. */
static const char *_wlmim_cursor_system_config_alternative_file =
    "/etc/alternatives/x-cursor-theme";

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmim_cursor_t *wlmim_cursor_create(
    wlmim_t *input_manager_ptr,
    const struct wlmim_cursor_style *style_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr)
{
    wlmim_cursor_t *cursor_ptr = logged_calloc(1, sizeof(wlmim_cursor_t));
    if (NULL == cursor_ptr) return NULL;
    cursor_ptr->input_manager_ptr = input_manager_ptr;
    cursor_ptr->wlr_seat_ptr = wlr_seat_ptr;
    cursor_ptr->root_ptr = root_ptr;

    // tinywl: wlr_cursor is a utility tracking the cursor image shown on
    // screen.
    cursor_ptr->wlr_cursor_ptr = wlr_cursor_create();
    if (NULL == cursor_ptr->wlr_cursor_ptr) {
        bs_log(BS_ERROR, "Failed wlr_cursor_create()");
        wlmim_cursor_destroy(cursor_ptr);
        return NULL;
    }
    wlr_cursor_attach_output_layout(
        cursor_ptr->wlr_cursor_ptr, wlr_output_layout_ptr);

    wlmim_cursor_set_style(cursor_ptr, style_ptr);
    cursor_ptr->pointer_ptr = wlmtk_pointer_create(
        cursor_ptr->wlr_cursor_ptr,
        cursor_ptr->wlr_xcursor_manager_ptr);
    if (NULL == cursor_ptr->pointer_ptr) {
        wlmim_cursor_destroy(cursor_ptr);
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

    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.motion,
        &cursor_ptr->motion_listener,
        _wlmim_cursor_handle_motion);
    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.motion_absolute,
        &cursor_ptr->motion_absolute_listener,
        _wlmim_cursor_handle_motion_absolute);
    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.button,
        &cursor_ptr->button_listener,
        _wlmim_cursor_handle_button);
    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.axis,
        &cursor_ptr->axis_listener,
        _wlmim_cursor_handle_axis);
    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_cursor_ptr->events.frame,
        &cursor_ptr->frame_listener,
        _wlmim_cursor_handle_frame);

    wlmtk_util_connect_listener_signal(
        &cursor_ptr->wlr_seat_ptr->events.request_set_cursor,
        &cursor_ptr->seat_request_set_cursor_listener,
        _wlmim_cursor_handle_seat_request_set_cursor);

    return cursor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmim_cursor_destroy(wlmim_cursor_t *cursor_ptr)
{
    wlmtk_util_disconnect_listener(
        &cursor_ptr->seat_request_set_cursor_listener);
    if (NULL != cursor_ptr->wlr_cursor_ptr) {
        wlmtk_util_disconnect_listener(&cursor_ptr->frame_listener);
        wlmtk_util_disconnect_listener(&cursor_ptr->axis_listener);
        wlmtk_util_disconnect_listener(&cursor_ptr->button_listener);
        wlmtk_util_disconnect_listener(&cursor_ptr->motion_absolute_listener);
        wlmtk_util_disconnect_listener(&cursor_ptr->motion_listener);
    }

    if (NULL != cursor_ptr->pointer_ptr) {
        wlmtk_pointer_destroy(cursor_ptr->pointer_ptr);
        cursor_ptr->pointer_ptr = NULL;
    }

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
bool wlmim_cursor_set_style(
    wlmim_cursor_t *cursor_ptr,
    const struct wlmim_cursor_style *style_ptr)
{
    char *theme_name_ptr = _wlmim_cursor_get_theme_name(style_ptr);
    if (NULL == theme_name_ptr) return false;

    struct wlr_xcursor_manager *wxm_ptr = wlr_xcursor_manager_create(
        theme_name_ptr, style_ptr->size);
    if (NULL == wxm_ptr) {
        bs_log(BS_ERROR,
               "Failed wlr_xcursor_manager_create(\"%s\", %"PRIu64")",
               theme_name_ptr, style_ptr->size);
        free(theme_name_ptr);
        return false;
    }

    if (!wlr_xcursor_manager_load(wxm_ptr, 1.0)) {
        bs_log(BS_ERROR, "Failed wlr_xcursor_manager_load() for %s, %"PRIu64,
               theme_name_ptr, style_ptr->size);
        wlr_xcursor_manager_destroy(wxm_ptr);
        free(theme_name_ptr);
        return false;
    }
    free(theme_name_ptr);

    if (NULL != cursor_ptr->pointer_ptr) {
        wlmtk_pointer_set_xcursor_manager(cursor_ptr->pointer_ptr, wxm_ptr);
    }
    if (NULL != cursor_ptr->wlr_xcursor_manager_ptr) {
        wlr_xcursor_manager_destroy(cursor_ptr->wlr_xcursor_manager_ptr);
    }
    cursor_ptr->wlr_xcursor_manager_ptr = wxm_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
struct wlr_cursor *wlmim_cursor_wlr_cursor(wlmim_cursor_t *cursor_ptr)
{
    return cursor_ptr->wlr_cursor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmim_cursor_attach_input_device(
    wlmim_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr)
{
     wlr_cursor_attach_input_device(
         cursor_ptr->wlr_cursor_ptr,
         wlr_input_device_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmim_cursor_detach_input_device(
    wlmim_cursor_t *cursor_ptr,
    struct wlr_input_device *wlr_input_device_ptr)
{
     wlr_cursor_detach_input_device(
         cursor_ptr->wlr_cursor_ptr,
         wlr_input_device_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `motion` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_motion_event`.
 */
void _wlmim_cursor_handle_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, motion_listener);
    struct wlr_pointer_motion_event *wlr_pointer_motion_event_ptr = data_ptr;

    wlmim_report_activity(cursor_ptr->input_manager_ptr);

    wlr_cursor_move(
        cursor_ptr->wlr_cursor_ptr,
        &wlr_pointer_motion_event_ptr->pointer->base,
        wlr_pointer_motion_event_ptr->delta_x,
        wlr_pointer_motion_event_ptr->delta_y);

    _wlmim_cursor_process_motion(
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
void _wlmim_cursor_handle_motion_absolute(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, motion_absolute_listener);
    struct wlr_pointer_motion_absolute_event
        *wlr_pointer_motion_absolute_event_ptr = data_ptr;

    wlmim_report_activity(cursor_ptr->input_manager_ptr);

    wlr_cursor_warp_absolute(
        cursor_ptr->wlr_cursor_ptr,
        &wlr_pointer_motion_absolute_event_ptr->pointer->base,
        wlr_pointer_motion_absolute_event_ptr->x,
        wlr_pointer_motion_absolute_event_ptr->y);

    _wlmim_cursor_process_motion(
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
void _wlmim_cursor_handle_button(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, button_listener);
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr = data_ptr;

    wlmim_report_activity(cursor_ptr->input_manager_ptr);

    struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
        cursor_ptr->wlr_seat_ptr);
    uint32_t modifiers = 0;
    if (NULL != wlr_keyboard_ptr) {
        modifiers = wlr_keyboard_get_modifiers(wlr_keyboard_ptr);
    }

    wlmtk_root_pointer_button(
        cursor_ptr->root_ptr,
        wlr_pointer_button_event_ptr,
        modifiers);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `axis` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr Points to a `wlr_pointer_axis_event`.
 */
void _wlmim_cursor_handle_axis(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, axis_listener);
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr = data_ptr;

    wlmim_report_activity(cursor_ptr->input_manager_ptr);

    wlmtk_root_pointer_axis(
        cursor_ptr->root_ptr,
        wlr_pointer_axis_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `frame` event of `wlr_cursor`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmim_cursor_handle_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, frame_listener);

    /* Notify the client with pointer focus of the frame event. */
    wlr_seat_pointer_notify_frame(cursor_ptr->wlr_seat_ptr);
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
void _wlmim_cursor_handle_seat_request_set_cursor(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmim_cursor_t *cursor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_cursor_t, seat_request_set_cursor_listener);
    struct wlr_seat_pointer_request_set_cursor_event
        *wlr_seat_pointer_request_set_cursor_event_ptr = data_ptr;

    struct wlr_seat_client *focused_wlr_seat_client_ptr =
        cursor_ptr->wlr_seat_ptr->pointer_state.focused_client;
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
void _wlmim_cursor_process_motion(
    wlmim_cursor_t *cursor_ptr,
    uint32_t time_msec)
{
    wl_signal_emit_mutable(
        &wlmim_events(cursor_ptr->input_manager_ptr)->cursor_position_updated,
        cursor_ptr->wlr_cursor_ptr);

    // TODO(kaeser@gubbe.ch): also make this an event-based callback.
    wlmtk_root_pointer_motion(
        cursor_ptr->root_ptr,
        cursor_ptr->wlr_cursor_ptr->x,
        cursor_ptr->wlr_cursor_ptr->y,
        time_msec,
        cursor_ptr->pointer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Returns a copy of the cursor theme's name to use, from configured style.
 *
 * This will either be a copy of @ref wlmim_cursor_style::name_ptr, or what is
 * described by the system-wide cursor theme files.
 *
 * @param style_ptr
 *
 * @return Cursor theme name, as string. Must be released via free().
 */
char *_wlmim_cursor_get_theme_name(const struct wlmim_cursor_style *style_ptr)
{
    char *theme_name_ptr;

    if (!style_ptr->override_system_configuration) {
        if (0 == ini_parse(_wlmim_cursor_system_config_file,
                           _wlmim_cursor_theme_handler, &theme_name_ptr) ||
            0 == ini_parse(_wlmim_cursor_system_config_alternative_file,
                           _wlmim_cursor_theme_handler, &theme_name_ptr)) {
            return theme_name_ptr;
        }
        bs_log(BS_WARNING, "System-wide cursor theme configuration not found, "
               "neither in %s nor %s. Falling back...",
               _wlmim_cursor_system_config_file,
               _wlmim_cursor_system_config_alternative_file);
    }
    return logged_strdup(style_ptr->name_ptr);
}

/* ------------------------------------------------------------------------- */
/** inih library parser callback for the system-wide cursor theme files. */
int _wlmim_cursor_theme_handler(
    void *ud_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr)
{
    // Skip anything other than the "Inherits" key in "[Icon Theme]".
    if (0 != strcmp(section_ptr, "Icon Theme") ||
        0 != strcmp(name_ptr, "Inherits")) return 1;

    char **theme_name_ptr_ptr = ud_ptr;
    *theme_name_ptr_ptr = logged_strdup(value_ptr);
    if (NULL == *theme_name_ptr_ptr) return 0;
    return 1;
}

/* == Unit tests =========================================================== */

static void _wlmim_cursor_test_theme_name(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t _wlmim_cursor_test_cases[] = {
    { 1, "theme_name", _wlmim_cursor_test_theme_name },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmim_cursor_test_set = BS_TEST_SET(
    true, "cursor", _wlmim_cursor_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests obtaining the theme name. */
void _wlmim_cursor_test_theme_name(bs_test_t *test_ptr)
{
    char *theme_name_ptr;

    int rv = ini_parse(bs_test_data_path(test_ptr, "input/cursor-index.theme"),
                       _wlmim_cursor_theme_handler, &theme_name_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, rv);
    BS_TEST_VERIFY_STREQ(test_ptr, "ThemeName", theme_name_ptr);
    free(theme_name_ptr);

    struct wlmim_cursor_style style = {
        .override_system_configuration = true,
        .name_ptr = "OverrideName"
    };
    theme_name_ptr = _wlmim_cursor_get_theme_name(&style);
    BS_TEST_VERIFY_STREQ(test_ptr, "OverrideName", theme_name_ptr);
    free(theme_name_ptr);
}

/* == End of cursor.c ====================================================== */
