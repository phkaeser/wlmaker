/* ========================================================================= */
/**
 * @file resizebar.c
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

#include "resizebar.h"

#include <linux/input-event-codes.h>
#include <wlr/util/edges.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of an interactive resizebar element. */
typedef struct {
    /** The interactive (parent structure). */
    wlmaker_interactive_t     interactive;

    /** Back-link to the view. */
    wlmaker_view_t            *view_ptr;
    /** Texture of the resize bar, not pressed. */
    struct wlr_buffer         *resizebar_buffer_ptr;
    /** Texture of the resize bar, pressed. */
    struct wlr_buffer         *resizebar_pressed_buffer_ptr;
    /** Which edges will be controlled by this element. */
    uint32_t                  edges;

    /** Status of the element. */
    bool                      pressed;
} wlmaker_resizebar_t;

static wlmaker_resizebar_t *resizebar_from_interactive(
    wlmaker_interactive_t *interactive_ptr);

static void _resizebar_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _resizebar_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _resizebar_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x,
    double y);
static void _resizebar_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _resizebar_destroy(wlmaker_interactive_t *interactive_ptr);

/* == Data ================================================================= */

/** Implementation: callbacks for the interactive. */
static const wlmaker_interactive_impl_t wlmaker_interactive_resizebar_impl = {
    .enter = _resizebar_enter,
    .leave = _resizebar_leave,
    .motion = _resizebar_motion,
    .button = _resizebar_button,
    .destroy = _resizebar_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_resizebar_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *resizebar_buffer_ptr,
    struct wlr_buffer *resizebar_pressed_buffer_ptr,
    uint32_t edges)
{
    wlmaker_resizebar_t *resizebar_ptr = logged_calloc(
        1, sizeof(wlmaker_resizebar_t));
    if (NULL == resizebar_ptr) return NULL;
    resizebar_ptr->view_ptr = view_ptr;
    resizebar_ptr->resizebar_buffer_ptr =
        wlr_buffer_lock(resizebar_buffer_ptr);
    resizebar_ptr->resizebar_pressed_buffer_ptr =
        wlr_buffer_lock(resizebar_pressed_buffer_ptr);
    resizebar_ptr->edges = edges;

    wlmaker_interactive_init(
        &resizebar_ptr->interactive,
        &wlmaker_interactive_resizebar_impl,
        wlr_scene_buffer_ptr,
        cursor_ptr,
        resizebar_buffer_ptr);

    return &resizebar_ptr->interactive;
}

/* ------------------------------------------------------------------------- */
void wlmaker_resizebar_set_textures(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *resizebar_buffer_ptr,
    struct wlr_buffer *resizebar_pressed_buffer_ptr)
{
    wlmaker_resizebar_t *resizebar_ptr = resizebar_from_interactive(
        interactive_ptr);

    // This only updates the internal references...
    wlr_buffer_unlock(resizebar_ptr->resizebar_buffer_ptr);
    resizebar_ptr->resizebar_buffer_ptr =
        wlr_buffer_lock(resizebar_buffer_ptr);
    wlr_buffer_unlock(resizebar_ptr->resizebar_pressed_buffer_ptr);
    resizebar_ptr->resizebar_pressed_buffer_ptr =
        wlr_buffer_lock(resizebar_pressed_buffer_ptr);

    // ... and we also need to set the current texture in appropriate state.
    wlmaker_interactive_set_texture(
        interactive_ptr,
        resizebar_ptr->pressed ?
        resizebar_ptr->resizebar_pressed_buffer_ptr :
        resizebar_ptr->resizebar_buffer_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast (with assertion) |interactive_ptr| to the |wlmaker_resizebar_t|.
 *
 * @param interactive_ptr
 */
wlmaker_resizebar_t *resizebar_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    if (NULL != interactive_ptr &&
        interactive_ptr->impl != &wlmaker_interactive_resizebar_impl) {
        bs_log(BS_FATAL, "Not a resizebar: %p", interactive_ptr);
        abort();
    }
    return (wlmaker_resizebar_t*)interactive_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor enters the resizebar area.
 *
 * @param interactive_ptr
 */
void _resizebar_enter(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_resizebar_t *resizebar_ptr = resizebar_from_interactive(
        interactive_ptr);

    const char *xcursor_name_ptr = "left_ptr";  // Default.
    if (resizebar_ptr->edges == WLR_EDGE_BOTTOM) {
        xcursor_name_ptr = "s-resize";
    } else if (resizebar_ptr->edges == (WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT)) {
        xcursor_name_ptr = "se-resize";
    } else if (resizebar_ptr->edges == (WLR_EDGE_BOTTOM | WLR_EDGE_LEFT)) {
        xcursor_name_ptr = "sw-resize";
    }

    wlr_cursor_set_xcursor(
        interactive_ptr->cursor_ptr->wlr_cursor_ptr,
        interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
        xcursor_name_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor leaves the resizebar area.
 *
 * @param interactive_ptr
 */
void _resizebar_leave(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor motion in the resizebar area.
 *
 *
 * @param interactive_ptr
 * @param x
 * @param y
 */
void _resizebar_motion(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr,
    __UNUSED__ double x,
    __UNUSED__ double y)
{
    // Nothing to do.
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
void _resizebar_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_resizebar_t *resizebar_ptr = resizebar_from_interactive(
        interactive_ptr);

    if (wlr_pointer_button_event_ptr->button != BTN_LEFT) return;
    switch (wlr_pointer_button_event_ptr->state) {
    case WLR_BUTTON_PRESSED:
        if (wlmaker_interactive_contains(&resizebar_ptr->interactive, x, y)) {
            resizebar_ptr->pressed = true;
        }
        break;

    case WLR_BUTTON_RELEASED:
        resizebar_ptr->pressed = false;
        break;

    default:
        /* huh, that's unexpected... */
        break;
    }

    wlmaker_interactive_set_texture(
        interactive_ptr,
        resizebar_ptr->pressed ?
        resizebar_ptr->resizebar_pressed_buffer_ptr :
        resizebar_ptr->resizebar_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys resizebar interactive.
 *
 * @param interactive_ptr
 */
void _resizebar_destroy(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_resizebar_t *resizebar_ptr = resizebar_from_interactive(
        interactive_ptr);

    if (NULL != resizebar_ptr->resizebar_buffer_ptr) {
        wlr_buffer_unlock(resizebar_ptr->resizebar_buffer_ptr);
        resizebar_ptr->resizebar_buffer_ptr = NULL;
    }
    if (NULL != resizebar_ptr->resizebar_pressed_buffer_ptr) {
        wlr_buffer_unlock(resizebar_ptr->resizebar_pressed_buffer_ptr);
        resizebar_ptr->resizebar_pressed_buffer_ptr = NULL;
    }

    free(resizebar_ptr);
}

/* == End of resizebar.c =================================================== */
