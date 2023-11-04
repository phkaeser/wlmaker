/* ========================================================================= */
/**
 * @file titlebar.c
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

#include "config.h"
#include "titlebar.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Titlebar state, with respect to moves. */
typedef enum {
    /** Idle */
    TITLEBAR_IDLE,
    /** Clicked, waiting to initiate move. */
    TITLEBAR_CLICKED,
    /** Actively moving. */
    TITLEBAR_MOVING
} titlebar_state_t;

/** State of the interactive titlebar. */
typedef struct {
    /** The interactive (parent structure). */
    wlmaker_interactive_t     interactive;

    /** Back-link to the view owning this titlebar. */
    wlmaker_view_t            *view_ptr;

    /** WLR buffer, contains texture for the title bar when focussed. */
    struct wlr_buffer         *titlebar_buffer_ptr;
    /** WLR buffer, contains texture for the title bar when blurred. */
    struct wlr_buffer         *titlebar_blurred_buffer_ptr;

    /** Titlebar state. */
    titlebar_state_t          state;
    /** X-Position of where the click happened. */
    double                    clicked_x;
    /** Y-Position of where the click happened. */
    double                    clicked_y;

    /** Nanosecond of last mouse-click, to catch double-clicks. */
    uint64_t                  last_click_nsec;
} wlmaker_titlebar_t;

static wlmaker_titlebar_t *titlebar_from_interactive(
    wlmaker_interactive_t *interactive_ptr);

static void _titlebar_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _titlebar_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _titlebar_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y);
static void _titlebar_focus(
    wlmaker_interactive_t *interactive_ptr);
static void _titlebar_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _titlebar_destroy(
    wlmaker_interactive_t *interactive_ptr);

/* == Data ================================================================= */

/** Implementation: callbacks for the interactive. */
static const wlmaker_interactive_impl_t wlmaker_interactive_titlebar_impl = {
    .enter = _titlebar_enter,
    .leave = _titlebar_leave,
    .motion = _titlebar_motion,
    .focus = _titlebar_focus,
    .button = _titlebar_button,
    .destroy = _titlebar_destroy
};

/** Default xcursor to use. */
static const char             *xcursor_name_default = "left_ptr";
/** Xcursor to show when in MOVING state. */
static const char             *xcursor_name_move = "move";
/** Minimum cursor move to enable MOVING afer CLICKED. */
static const double           minimal_move = 2;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_titlebar_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *titlebar_buffer_ptr,
    struct wlr_buffer *titlebar_blurred_buffer_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmaker_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;
    titlebar_ptr->view_ptr = view_ptr;
    titlebar_ptr->titlebar_buffer_ptr = wlr_buffer_lock(titlebar_buffer_ptr);
    titlebar_ptr->titlebar_blurred_buffer_ptr =
        wlr_buffer_lock(titlebar_blurred_buffer_ptr);
    titlebar_ptr->state = TITLEBAR_IDLE;

    wlmaker_interactive_init(
        &titlebar_ptr->interactive,
        &wlmaker_interactive_titlebar_impl,
        wlr_scene_buffer_ptr,
        cursor_ptr,
        titlebar_buffer_ptr);

    return &titlebar_ptr->interactive;
}

/* ------------------------------------------------------------------------- */
void wlmaker_title_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *titlebar_buffer_ptr,
    struct wlr_buffer *titlebar_blurred_buffer_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);
    wlr_buffer_unlock(titlebar_ptr->titlebar_buffer_ptr);
    wlr_buffer_unlock(titlebar_ptr->titlebar_blurred_buffer_ptr);
    titlebar_ptr->titlebar_buffer_ptr = wlr_buffer_lock(titlebar_buffer_ptr);
    titlebar_ptr->titlebar_blurred_buffer_ptr =
        wlr_buffer_lock(titlebar_blurred_buffer_ptr);
    wlmaker_interactive_set_texture(
        interactive_ptr,
        interactive_ptr->focussed ?
        titlebar_ptr->titlebar_buffer_ptr :
        titlebar_ptr->titlebar_blurred_buffer_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast (with assertion) |interactive_ptr| to the |wlmaker_titlebar_t|.
 *
 * @param interactive_ptr
 */
wlmaker_titlebar_t *titlebar_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    if (NULL != interactive_ptr &&
        interactive_ptr->impl != &wlmaker_interactive_titlebar_impl) {
        bs_log(BS_FATAL, "Not a titlebar: %p", interactive_ptr);
        abort();
    }
    return (wlmaker_titlebar_t*)interactive_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor enters the button area.
 *
 * @param interactive_ptr
 */
void _titlebar_enter(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);

    const char *cursor_name_ptr = xcursor_name_default;
    if (titlebar_ptr->state == TITLEBAR_MOVING) {
        cursor_name_ptr = xcursor_name_move;
    }

    wlr_cursor_set_xcursor(
        interactive_ptr->cursor_ptr->wlr_cursor_ptr,
        interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
        cursor_name_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor leaves the button area.
 *
 * @param interactive_ptr
 */
void _titlebar_leave(__UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Handle cursor motion.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 */
void _titlebar_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);

    if (titlebar_ptr->state == TITLEBAR_CLICKED &&
        (fabs(titlebar_ptr->clicked_x - x) > minimal_move ||
         fabs(titlebar_ptr->clicked_y - y) > minimal_move)) {
        titlebar_ptr->state = TITLEBAR_MOVING;
        wlmaker_cursor_begin_move(
            titlebar_ptr->interactive.cursor_ptr,
            titlebar_ptr->view_ptr);

        wlr_cursor_set_xcursor(
            interactive_ptr->cursor_ptr->wlr_cursor_ptr,
            interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
            xcursor_name_move);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Focus state changes.
 *
 * @param interactive_ptr
 */
static void _titlebar_focus(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);

    wlmaker_interactive_set_texture(
        interactive_ptr,
        interactive_ptr->focussed ?
        titlebar_ptr->titlebar_buffer_ptr :
        titlebar_ptr->titlebar_blurred_buffer_ptr);
    if (!interactive_ptr->focussed) {
        titlebar_ptr->state = TITLEBAR_IDLE;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Handle cursor button, ie. button press or release.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 * @param wlr_pointer_button_event_ptr
 */
void _titlebar_button(
    wlmaker_interactive_t *interactive_ptr,
    double x,
    double y,
    __UNUSED__ struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);
    uint64_t now_nsec;

    if (wlr_pointer_button_event_ptr->button == BTN_RIGHT &&
        wlr_pointer_button_event_ptr->state == WLR_BUTTON_PRESSED) {
        wlmaker_view_window_menu_show(titlebar_ptr->view_ptr);
    }

    if (wlr_pointer_button_event_ptr->button != BTN_LEFT) return;

    switch (wlr_pointer_button_event_ptr->state) {
    case WLR_BUTTON_PRESSED:
        now_nsec = bs_mono_nsec();
        if (now_nsec - titlebar_ptr->last_click_nsec <
            wlmaker_config_double_click_wait_msec * 1000000ull) {
            // two clicks! will take it!
            titlebar_ptr->state = TITLEBAR_IDLE;
            wlmaker_view_shade(titlebar_ptr->view_ptr);
            break;
        }

        if (titlebar_ptr->state == TITLEBAR_IDLE) {
            titlebar_ptr->state = TITLEBAR_CLICKED;
            titlebar_ptr->clicked_x = x;
            titlebar_ptr->clicked_y = y;
        }
        titlebar_ptr->last_click_nsec = now_nsec;
        break;

    case WLR_BUTTON_RELEASED:
        titlebar_ptr->state = TITLEBAR_IDLE;
        // Reset cursor to default, if it is within our bounds.
        if (wlmaker_interactive_contains(&titlebar_ptr->interactive, x, y)) {
            wlr_cursor_set_xcursor(
                interactive_ptr->cursor_ptr->wlr_cursor_ptr,
                interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
                xcursor_name_default);
        }
        break;

    default:
        /* huh, that's unexpected... */
        break;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the titlebar interactive.
 *
 * @param interactive_ptr
 */
void _titlebar_destroy(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_titlebar_t *titlebar_ptr = titlebar_from_interactive(
        interactive_ptr);

    if (NULL != titlebar_ptr->titlebar_buffer_ptr) {
        wlr_buffer_unlock(titlebar_ptr->titlebar_buffer_ptr);
        titlebar_ptr->titlebar_buffer_ptr = NULL;
    }
    if (NULL != titlebar_ptr->titlebar_blurred_buffer_ptr) {
        wlr_buffer_unlock(titlebar_ptr->titlebar_blurred_buffer_ptr);
        titlebar_ptr->titlebar_blurred_buffer_ptr = NULL;
    }
    free(titlebar_ptr);
}

/* == End of titlebar.c ==================================================== */
