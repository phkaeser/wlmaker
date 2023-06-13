/* ========================================================================= */
/**
 * @file tile.c
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

#include "tile.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>

/* == Declarations ========================================================= */

/** State of an interactive tile. */
typedef struct {
    /** The interactive (parent structure). */
    wlmaker_interactive_t     interactive;

    /** Callback, issued when the tile is triggered (clicked). */
    wlmaker_interactive_callback_t tile_callback;
    /** Extra argument to provide to |tile_callback|. */
    void                      *tile_callback_arg;

    /** WLR buffer, contains texture for the tile in released state. */
    struct wlr_buffer         *tile_released_buffer_ptr;
} wlmaker_tile_t;

static wlmaker_tile_t *tile_from_interactive(
    wlmaker_interactive_t *interactive_ptr);

static void _tile_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _tile_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _tile_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y);
static void _tile_focus(
    wlmaker_interactive_t *interactive_ptr);
static void _tile_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _tile_destroy(wlmaker_interactive_t *interactive_ptr);

/* == Data ================================================================= */

/** Implementation: callbacks for the interactive. */
static const wlmaker_interactive_impl_t wlmaker_interactive_tile_impl = {
    .enter = _tile_enter,
    .leave = _tile_leave,
    .motion = _tile_motion,
    .focus = _tile_focus,
    .button = _tile_button,
    .destroy = _tile_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_tile_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t tile_callback,
    void *tile_callback_arg,
    struct wlr_buffer *tile_released_ptr)
{
    wlmaker_tile_t *tile_ptr = logged_calloc(1, sizeof(wlmaker_tile_t));
    if (NULL == tile_ptr) return NULL;
    tile_ptr->tile_callback = tile_callback;
    tile_ptr->tile_callback_arg = tile_callback_arg;

    wlmaker_interactive_init(
        &tile_ptr->interactive,
        &wlmaker_interactive_tile_impl,
        wlr_scene_buffer_ptr,
        cursor_ptr,
        tile_released_ptr);

    return &tile_ptr->interactive;
}

/* ------------------------------------------------------------------------- */
void wlmaker_tile_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *tile_buffer_ptr)
{
    wlmaker_tile_t *tile_ptr = tile_from_interactive(interactive_ptr);
    if (NULL == tile_ptr) {
        return;
    }

    wlmaker_interactive_set_texture(interactive_ptr, tile_buffer_ptr);

    if (NULL != tile_ptr->tile_released_buffer_ptr) {
        wlr_buffer_unlock(tile_ptr->tile_released_buffer_ptr);
        tile_ptr->tile_released_buffer_ptr = NULL;
    }
    tile_ptr->tile_released_buffer_ptr = wlr_buffer_lock(tile_buffer_ptr);

}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast (with assertion) |interactive_ptr| to the |wlmaker_tile_t|.
 *
 * @param interactive_ptr
 */
wlmaker_tile_t *tile_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    if (NULL != interactive_ptr &&
        interactive_ptr->impl != &wlmaker_interactive_tile_impl) {
        bs_log(BS_FATAL, "Not a tile: %p", interactive_ptr);
        abort();
    }
    return (wlmaker_tile_t*)interactive_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor enters the tile area.
 *
 * @param interactive_ptr
 */
void _tile_enter(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor leaves the tile area.
 *
 * @param interactive_ptr
 */
void _tile_leave(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor motion.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 */
void _tile_motion(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr,
    __UNUSED__ double x, __UNUSED__ double y)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Focus state change.
 *
 * @param interactive_ptr
 */
void _tile_focus(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // Nothing to do.
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Button press.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 * @param wlr_pointer_button_event_ptr
 */
void _tile_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_tile_t *tile_ptr = tile_from_interactive(interactive_ptr);

    if (wlr_pointer_button_event_ptr->button != BTN_LEFT ||
        wlr_pointer_button_event_ptr->state != WLR_BUTTON_PRESSED ||
        !wlmaker_interactive_contains(&tile_ptr->interactive, x, y)) {
        // Not a button press, or outside our area. Nothing to do here.
        return;
    }

    tile_ptr->tile_callback(interactive_ptr, tile_ptr->tile_callback_arg);
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the tile interactive.
 *
 * @param interactive_ptr
 */
void _tile_destroy(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_tile_t *tile_ptr = tile_from_interactive(interactive_ptr);

    if (NULL != tile_ptr->tile_released_buffer_ptr) {
        wlr_buffer_unlock(tile_ptr->tile_released_buffer_ptr);
        tile_ptr->tile_released_buffer_ptr = NULL;
    }

    free(tile_ptr);
}

/* == End of tile.c ======================================================== */
