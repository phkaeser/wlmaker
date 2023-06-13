/* ========================================================================= */
/**
 * @file interactive.h
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
 *
 * Abstract interface for interactive elements used in wlmaker: Buttons, title
 * bar and resize bar elements. It is used as a common interface to pass along
 * cursor motion and button events.
 */
#ifndef __INTERACTIVE_H__
#define __INTERACTIVE_H__

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/** Handle for the interactive. */
typedef struct _wlmaker_interactive_t wlmaker_interactive_t;

#include "cursor.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Implementation methods for the interactive. */
typedef struct {
    /** Called when the cursor enters the interactive area. */
    void (*enter)(wlmaker_interactive_t *interactive_ptr);
    /** Called when the cursor leaves the interactive area. */
    void (*leave)(wlmaker_interactive_t *interactive_ptr);
    /** Called when there is a cursor motion in the area. */
    void (*motion)(
        wlmaker_interactive_t *interactive_ptr,
        double x, double y);
    /** Called when the focus status changes. */
    void (*focus)(wlmaker_interactive_t *interactive_ptr);
    /**
     * Called on button press in the area, or any button release event.
     */
    void (*button)(
        wlmaker_interactive_t *interactive_ptr,
        double x, double y,
        struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
    /** Destructor. */
    void (*destroy)(wlmaker_interactive_t *interactive_ptr);
} wlmaker_interactive_impl_t;

/** Callback for when an interactive element needs to trigger an action. */
typedef void (*wlmaker_interactive_callback_t)(
    wlmaker_interactive_t *interactive_ptr,
    void *data_ptr);

/** Handle for the interactive. */
struct _wlmaker_interactive_t {
    /** Implementation grid. */
    const wlmaker_interactive_impl_t *impl;

    /** Node of the AVL tree. */
    bs_avltree_node_t         avlnode;

    /** Whether the interactive is focussed (may receive actions) or not. */
    bool                      focussed;

    /** Buffer scene node. Holds the interactive. */
    struct wlr_scene_buffer   *wlr_scene_buffer_ptr;
    /** For convenience: Width of the interactive, in pixels. */
    int                       width;
    /** For convenience: Height of the interactive, in pixels. */
    int                       height;

    /** Back-link to cursor. */
    wlmaker_cursor_t          *cursor_ptr;
};

/**
 * Initializes the interactive.
 *
 * @param interactive_ptr
 * @param impl_ptr
 * @param wlr_scene_buffer_ptr Buffer scene node to contain the button. Must
 *                            outlive the interactive, not taking ownership.
 * @param cursor_ptr
 * @param initial_wlr_buffer_ptr Texture WLR buffer to initialize
 *                            |wlr_scene_buffer_ptr| from.
 */
void wlmaker_interactive_init(
    wlmaker_interactive_t *interactive_ptr,
    const wlmaker_interactive_impl_t *impl_ptr,
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    struct wlr_buffer *initial_wlr_buffer_ptr);

/**
 * Sets this interactive's texture. Also updates dimensions accordingly.
 *
 * @param interactive_ptr
 * @param wlr_buffer_ptr
 */
void wlmaker_interactive_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *wlr_buffer_ptr);

/**
 * Returns whether the interactive contains |x|, |y| in relative coordinates.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 *
 * @return True if |x|, |y| is within [0, |width|), [0, |height|).
 */
static inline bool wlmaker_interactive_contains(
    const wlmaker_interactive_t *interactive_ptr,
    double x, double y) {
    return (0 <= x && x < interactive_ptr->width &&
            0 <= y && y < interactive_ptr->height);
}

/**
 * Call when the cursor enters the interactive area.
 *
 * @param interactive_ptr
 */
static inline void wlmaker_interactive_enter(
    wlmaker_interactive_t *interactive_ptr) {
    if (!interactive_ptr->focussed) return;
    interactive_ptr->impl->enter(interactive_ptr);
}

/**
 * Call to specify whether the view containing the interactive is focussed.
 *
 * This is used to adjust eg. the decoration style to focussed or blurred
 * windows.
 *
 * @param interactive_ptr
 * @param focussed
 */
static inline void wlmaker_interactive_focus(
    wlmaker_interactive_t *interactive_ptr,
    bool focussed) {
    interactive_ptr->focussed = focussed;
    if (interactive_ptr->impl->focus) {
        interactive_ptr->impl->focus(interactive_ptr);
    }
}

/**
 * Call when the cursor leaves the interactive area.
 *
 * @param interactive_ptr
 */
static inline void wlmaker_interactive_leave(
    wlmaker_interactive_t *interactive_ptr) {
    interactive_ptr->impl->leave(interactive_ptr);
}

/**
 * Call when the cursor moves in the interactive area.
 *
 * @param interactive_ptr
 * @param x                   New cursor x pos, relative to the interactive.
 * @param y                   New cursor y pos, relative to the interactive.
 */
static inline void wlmaker_interactive_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y) {
    if (!interactive_ptr->focussed) return;
    interactive_ptr->impl->motion(interactive_ptr, x, y);
}

/**
 * Call when there is a button event for the interactive.
 *
 * Called when a button is pressed while over the interactive. But also for any
 * button release event (of the entire server), in order to wrap up state of
 * clickable actions.
 *
 * @param interactive_ptr
 * @param x                   New cursor x pos, relative to the interactive.
 * @param y                   New cursor y pos, relative to the interactive.
 * @param wlr_pointer_button_event_ptr
 */
static inline void wlmaker_interactive_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr) {
    interactive_ptr->impl->button(
        interactive_ptr, x, y, wlr_pointer_button_event_ptr);
}

/**
 * AVL tree comparator: Compares the `wlr_scene_buffer.node` pointers.
 *
 * @param node_ptr
 * @param key_ptr
 *
 * @return -1 if less, 0 if equal, 1 if larger.
 */
int wlmaker_interactive_node_cmp(const bs_avltree_node_t *node_ptr,
                                 const void *key_ptr);

/**
 * Destroy the avl tree node, ie. the interactive at this node.
 *
 * @param node_ptr
 */
void wlmaker_interactive_node_destroy(bs_avltree_node_t *node_ptr);

/**
 * Cast the AVL tree node to the `wlmaker_interactive_t`.
 *
 * @param node_ptr
 *
 * @return The interactive of this node.
 */
wlmaker_interactive_t *wlmaker_interactive_from_avlnode(
    bs_avltree_node_t *node_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __INTERACTIVE_H__ */
/* == End of interactive.h ================================================= */
