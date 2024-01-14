/* ========================================================================= */
/**
 * @file button.c
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

#include "button.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE

/* == Definitions ========================================================== */

/** State of an interactive button. */
typedef struct {
    /** The interactive (parent structure). */
    wlmaker_interactive_t     interactive;

    /** Callback, issued when the button is triggered (released). */
    wlmaker_interactive_callback_t button_callback;
    /** Extra argument to provide to |button_callback|. */
    void                      *button_callback_arg;

    /** WLR buffer, contains texture for the button in released state. */
    struct wlr_buffer         *button_released_buffer_ptr;
    /** WLR buffer, contains texture for the button in "pressed" state. */
    struct wlr_buffer         *button_pressed_buffer_ptr;
    /** WLR buffer, contains texture for the button in "blurred" state. */
    struct wlr_buffer         *button_blurred_buffer_ptr;

    /** Button state "activated": Button was pressed, not yet released. */
    bool                      activated;
    /**
     * Button state "pressed": when "activated" and below cursor.
     *
     * For consistency: Update this value only via the |button_press| method.
     */
    bool                      pressed;
} wlmaker_button_t;

static wlmaker_button_t *button_from_interactive(
    wlmaker_interactive_t *interactive_ptr);
static void button_press(wlmaker_button_t *button_ptr, bool pressed);

static void _button_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _button_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _button_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y);
static void _button_focus(
    wlmaker_interactive_t *interactive_ptr);
static void _button_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _button_destroy(wlmaker_interactive_t *interactive_ptr);

/* == Data ================================================================= */

/** Implementation: callbacks for the interactive. */
static const wlmaker_interactive_impl_t wlmaker_interactive_button_impl = {
    .enter = _button_enter,
    .leave = _button_leave,
    .motion = _button_motion,
    .focus = _button_focus,
    .button = _button_button,
    .destroy = _button_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_button_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t button_callback,
    void *button_callback_arg,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr)
{
    wlmaker_button_t *button_ptr = logged_calloc(1, sizeof(wlmaker_button_t));
    if (NULL == button_ptr) return NULL;

    if (button_pressed_ptr->width != button_released_ptr->width ||
        button_pressed_ptr->height != button_released_ptr->height) {
        bs_log(BS_ERROR, "Button texture sizes do not match. "
               "Pressed %d x %d, Released %d x %d",
               button_pressed_ptr->width, button_pressed_ptr->height,
               button_released_ptr->width, button_released_ptr->height);
        _button_destroy(&button_ptr->interactive);
        return NULL;
    }

    wlmaker_interactive_init(
        &button_ptr->interactive,
        &wlmaker_interactive_button_impl,
        wlr_scene_buffer_ptr,
        cursor_ptr,
        button_released_ptr);

    button_ptr->button_callback = button_callback;
    button_ptr->button_callback_arg = button_callback_arg;
    button_ptr->button_released_buffer_ptr =
        wlr_buffer_lock(button_released_ptr);
    button_ptr->button_pressed_buffer_ptr =
        wlr_buffer_lock(button_pressed_ptr);
    button_ptr->button_blurred_buffer_ptr =
        wlr_buffer_lock(button_blurred_ptr);

    return &button_ptr->interactive;
}

/* ------------------------------------------------------------------------- */
void wlmaker_button_set_textures(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);

    wlr_buffer_unlock(button_ptr->button_released_buffer_ptr);
    button_ptr->button_released_buffer_ptr =
        wlr_buffer_lock(button_released_ptr);

    wlr_buffer_unlock(button_ptr->button_pressed_buffer_ptr);
    button_ptr->button_pressed_buffer_ptr =
        wlr_buffer_lock(button_pressed_ptr);

    wlr_buffer_unlock(button_ptr->button_blurred_buffer_ptr);
    button_ptr->button_blurred_buffer_ptr =
        wlr_buffer_lock(button_blurred_ptr);

    if (button_ptr->interactive.focussed) {
        wlmaker_interactive_set_texture(
            interactive_ptr,
            button_ptr->pressed ?
            button_ptr->button_pressed_buffer_ptr :
            button_ptr->button_released_buffer_ptr);
    } else {
        wlmaker_interactive_set_texture(
            &button_ptr->interactive,
            button_ptr->button_blurred_buffer_ptr);
    }
}

/* == Local methods ======================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast (with assertion) |interactive_ptr| to the |wlmaker_button_t|.
 *
 * @param interactive_ptr
 */
wlmaker_button_t *button_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    if (NULL != interactive_ptr &&
        interactive_ptr->impl != &wlmaker_interactive_button_impl) {
        bs_log(BS_FATAL, "Not a button: %p", interactive_ptr);
        abort();
    }
    return (wlmaker_button_t*)interactive_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Changes the "pressed" state of this button, and updates the buffer texture.
 *
 * @param button_ptr
 * @param pressed
 */
void button_press(wlmaker_button_t *button_ptr, bool pressed)
{
    if (button_ptr->pressed == pressed) return;

    button_ptr->pressed = pressed;
    wlmaker_interactive_set_texture(
        &button_ptr->interactive,
        button_ptr->pressed ?
        button_ptr->button_pressed_buffer_ptr :
        button_ptr->button_released_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor enters the button area.
 *
 * @param interactive_ptr
 */
void _button_enter(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);
    if (button_ptr->activated) button_press(button_ptr, true);

    wlr_cursor_set_xcursor(
        interactive_ptr->cursor_ptr->wlr_cursor_ptr,
        interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
        "left_ptr");
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor leaves the button area.
 *
 * @param interactive_ptr
 */
void _button_leave(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);
    if (button_ptr->activated) button_press(button_ptr, false);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Handle cursor motion.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 */
void _button_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);
    if (button_ptr->activated) {
        button_press(
            button_ptr,
            wlmaker_interactive_contains(&button_ptr->interactive, x, y));
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Focus state changes.
 *
 * @param interactive_ptr
 */
static void _button_focus(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);

    if (!interactive_ptr->focussed) {
        button_ptr->activated = false;
        button_press(button_ptr, false);
        wlmaker_interactive_set_texture(
            &button_ptr->interactive,
            button_ptr->button_blurred_buffer_ptr);
    } else {
        wlmaker_interactive_set_texture(
            &button_ptr->interactive,
            button_ptr->pressed ?
            button_ptr->button_pressed_buffer_ptr :
            button_ptr->button_released_buffer_ptr);
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
void _button_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);
    bool triggered;

    if (wlr_pointer_button_event_ptr->button != BTN_LEFT) return;

    switch (wlr_pointer_button_event_ptr->state) {
    case WLR_BUTTON_PRESSED:
        if (wlmaker_interactive_contains(&button_ptr->interactive, x, y)) {
            button_ptr->activated = true;
            button_press(button_ptr, true);
        }
        break;

    case WLR_BUTTON_RELEASED:
        triggered = (
            button_ptr->activated &&
            wlmaker_interactive_contains(&button_ptr->interactive, x, y));
        button_ptr->activated = false;
        button_press(button_ptr, false);
        if (triggered) {
            button_ptr->button_callback(
                interactive_ptr, button_ptr->button_callback_arg);
        }
        break;

    default:
        /* huh, that's unexpected... */
        break;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the button interactive.
 *
 * @param interactive_ptr
 */
void _button_destroy(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_button_t *button_ptr = button_from_interactive(interactive_ptr);

    if (NULL != button_ptr->button_released_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->button_released_buffer_ptr);
        button_ptr->button_released_buffer_ptr = NULL;
    }

    if (NULL != button_ptr->button_pressed_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->button_pressed_buffer_ptr);
        button_ptr->button_pressed_buffer_ptr = NULL;
    }

    if (NULL != button_ptr->button_blurred_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->button_blurred_buffer_ptr);
        button_ptr->button_pressed_buffer_ptr = NULL;
    }

    free(button_ptr);
}

/* == End of button.c ====================================================== */
