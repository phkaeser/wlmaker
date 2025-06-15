/* ========================================================================= */
/**
 * @file element.h
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
#ifndef __WLMTK_ELEMENT_H__
#define __WLMTK_ELEMENT_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>

#define WLR_USE_UNSTABLE
#include <wlr/util/box.h>
#undef WLR_USE_UNSTABLE

struct _wlmtk_element_t;
struct _wlmtk_element_vmt_t;
struct wlr_scene_tree;

/** Forward declaration: Element. */
typedef struct _wlmtk_element_t wlmtk_element_t;
/** Forward declaration: Element virtual method table. */
typedef struct _wlmtk_element_vmt_t wlmtk_element_vmt_t;

/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
/** Forward declaration: Wlroots keyboard event. */
struct wlr_keyboard_key_event;

#include "input.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_pointer.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Events of the element. */
typedef struct {
    /** The pointer just entered this element. Pointer focus gained. */
    struct wl_signal          pointer_enter;
    /** Pointer exited this element (or is obstructed). Pointer focus lost. */
    struct wl_signal          pointer_leave;
} wlmtk_element_events_t;

/** Virtual method table for the element. */
struct _wlmtk_element_vmt_t {
    /** Abstract: Destroys the implementation of the element. */
    void (*destroy)(wlmtk_element_t *element_ptr);

    /** Abstract: Creates element's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_element_t *element_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);

    /** Abstract: Gets dimensions of the element, relative to the element's position. */
    void (*get_dimensions)(
        wlmtk_element_t *element_ptr,
        int *x1_ptr,
        int *y1_ptr,
        int *x2_ptr,
        int *y2_ptr);

    /** Gets element area to accept pointer activity, relative to position. */
    void (*get_pointer_area)(
        wlmtk_element_t *element_ptr,
        int *left_ptr,
        int *top_ptr,
        int *right_ptr,
        int *bottom_ptr);

    /**
     * Indicates pointer motion into or within the element area to (x,y).
     *
     * The default implementation at @ref _wlmtk_element_pointer_motion updates
     * @ref wlmtk_element_t::last_pointer_motion_event.
     *
     * Derived classes that overwrite this method are advised to call the
     * initial implementation for keeping these internals updated.
     *
     * @param element_ptr
     * @param motion_event_ptr Details on the pointer motion. The coordinates
     *                        are relative to the element's coordinates.
     *
     * @return Whether the motion is considered within the element's pointer
     *     area. If it returns true, the caller should consider this element
     *     as having pointer focus.
     */
    bool (*pointer_motion)(wlmtk_element_t *element_ptr,
                           wlmtk_pointer_motion_event_t *motion_event_ptr);

    /**
     * Indicates pointer button event.
     *
     * @param element_ptr
     * @param button_event_ptr
     *
     * @return true If the button event was consumed.
     */
    bool (*pointer_button)(wlmtk_element_t *element_ptr,
                           const wlmtk_button_event_t *button_event_ptr);

    /**
     * Indicates a pointer axis event.
     *
     * @param element_ptr
     * @param wlr_pointer_axis_event_ptr
     *
     * @return true If the axis event was consumed.
     */
    bool (*pointer_axis)(
        wlmtk_element_t *element_ptr,
        struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);

    /**
     * Cancels a held pointer grab.
     *
     * Required to have an implementation by any element that requests a
     * pointer grab through @ref wlmtk_container_pointer_grab.
     *
     * Private: Must only to be called by the parent container.
     *
     * @param element_ptr
     */
    void (*pointer_grab_cancel)(wlmtk_element_t *element_ptr);

    /**
     * Blurs (de-activates) keyboard focus for the element. Propagates to child
     * elements, where available.
     *
     * @param element_ptr
     */
    void (*keyboard_blur)(wlmtk_element_t *element_ptr);

    /**
     * Handler for keyboard events.
     *
     * This handler is suitable for passing keyboard events on to Wayland
     * clients, which may have their own keymap and state tracking.
     *
     * @param element_ptr
     * @param wlr_keyboard_key_event_ptr
     *
     * @return true if the key was handled.
     */
    bool (*keyboard_event)(
        wlmtk_element_t *element_ptr,
        struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr);

    /**
     * Handler for already-translated keys.
     *
     * This handler is intended for toolkit elements reacting on key strokes,
     * and expects the parent to have translated the key event into (a series
     * of) keysym events.
     *
     * @param element_ptr
     * @param keysym
     * @param direction
     * @param modifiers
     *
     * @return true if the key was processed.
     */
    bool (*keyboard_sym)(
        wlmtk_element_t *element_ptr,
        xkb_keysym_t keysym,
        enum xkb_key_direction direction,
        uint32_t modifiers);
};

/** State of an element. */
struct _wlmtk_element_t {
    /**
     * X position of the element in pixels, relative to parent container.
     *
     * This value may be stale, if @ref wlmtk_element_t::wlr_scene_node_ptr is
     * set and had been updated directly. Therefore, consider the value as
     * "private", and access only through @ref wlmtk_element_get_position.
     */
    int x;
    /**
     * Y position of the element, relative to the container.
     *
     * Same observations apply as for @ref wlmtk_element_t::x.
     */
    int y;

    /** The container this element belongs to, if any. */
    wlmtk_container_t         *parent_container_ptr;
    /** The node of elements. */
    bs_dllist_node_t          dlnode;

    /** Virtual method table for the element. */
    wlmtk_element_vmt_t       vmt;
    /** Events available from the element. */
    wlmtk_element_events_t    events;

    /** Points to the wlroots scene graph API node, if attached. */
    struct wlr_scene_node     *wlr_scene_node_ptr;

    /** Whether the element is visible (drawn, when part of a scene graph). */
    bool                      visible;

    /** Listener for the `destroy` signal of `wlr_scene_node_ptr`. */
    struct wl_listener        wlr_scene_node_destroy_listener;

    /** Details of last @ref wlmtk_element_pointer_motion call. */
    wlmtk_pointer_motion_event_t last_pointer_motion_event;

    /** Whether the pointer is currently within the element's bounds. */
    bool                      pointer_inside;
};

/**
 * Initializes the element.
 *
 * @param element_ptr
 *
 * @return true on success.
 */
bool wlmtk_element_init(wlmtk_element_t *element_ptr);

/**
 * Extends the element's virtual methods.
 *
 * @param element_ptr
 * @param element_vmt_ptr
 *
 * @return The previous virtual method table.
 */
wlmtk_element_vmt_t wlmtk_element_extend(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_vmt_t *element_vmt_ptr);

/**
 * Cleans up the element.
 *
 * @param element_ptr
 */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr);

/** Gets the dlnode from the element. */
bs_dllist_node_t *wlmtk_dlnode_from_element(
    wlmtk_element_t *element_ptr);
/** Gets the element from the dlnode. */
wlmtk_element_t *wlmtk_element_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Sets the parent container for the element.
 *
 * Will call @ref wlmtk_element_attach_to_scene_graph to align the scene graph
 * with the new (or deleted) parent.
 *
 * Private: Should only be called by wlmtk_container_add_element, respectively
 * wlmtk_container_remove_element ("friends").
 *
 * @param element_ptr
 * @param parent_container_ptr Pointer to the parent container, or NULL if
 *     the parent should be cleared.
 */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr);

/**
 * Attaches or detaches the element to the parent's wlroots scene tree.
 *
 * If the element has a parent, and that parent is itself attached to the
 * wlroots scene tree, this will either re-parent an already existing node,
 * or invoke @ref wlmtk_element_vmt_t::create_scene_node to create and attach a
 * new node to the paren'ts tree.
 * Otherwise, it will clear any existing node.
 *
 * The function is idempotent.
 *
 * Private: Should only called by wlmtk_container_t methods, when there are
 * changes to wlmtk_container_t::wlr_scene_tree.
 *
 * @param element_ptr
 */
void wlmtk_element_attach_to_scene_graph(
    wlmtk_element_t *element_ptr);

/**
 * Sets the element's visibility.
 *
 * @param element_ptr
 * @param visible
 */
void wlmtk_element_set_visible(wlmtk_element_t *element_ptr, bool visible);

/**
 * Returns the position of the element.
 *
 * @param element_ptr
 * @param x_ptr               Optional, may be NULL.
 * @param y_ptr               Optional, may be NULL.
 */
void wlmtk_element_get_position(
    wlmtk_element_t *element_ptr,
    int *x_ptr,
    int *y_ptr);

/**
 * Sets the position of the element.
 *
 * @param element_ptr
 * @param x
 * @param y
 */
void wlmtk_element_set_position(
    wlmtk_element_t *element_ptr,
    int x,
    int y);

/**
 * Gets the area that the element on which the element accepts pointer events.
 *
 * The area extents are relative to the element's position. By default, this
 * overlaps with the element dimensions. Some elements (eg. a surface with
 * further-extending sub-surfaces) may differ.
 *
 * @param element_ptr
 * @param x1_ptr              Leftmost position of pointer area. May be NULL.
 * @param y1_ptr              Topmost position of pointer area. May be NULL.
 * @param x2_ptr              Rightmost position of pointer area. May be NULL.
 * @param y2_ptr              Bottommost position of pointer area. May be NULL.
 */
static inline void wlmtk_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr)
{
    element_ptr->vmt.get_pointer_area(
        element_ptr, x1_ptr, y1_ptr, x2_ptr, y2_ptr);
}

/**
 * Gets the dimensions of the element in pixels, relative to the position.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
static inline void wlmtk_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    element_ptr->vmt.get_dimensions(
        element_ptr, left_ptr, top_ptr, right_ptr, bottom_ptr);
}

/**
 * Gets the element's dimensions in pixel as wlr_box, relative to the position.
 *
 * @param element_ptr
 *
 * @return A struct wlr_box that specifies the top-left corner of the element
 *     relative to it's position, and the element's total width and height.
 */
static inline struct wlr_box wlmtk_element_get_dimensions_box(
    wlmtk_element_t *element_ptr)
{
    struct wlr_box box;
    element_ptr->vmt.get_dimensions(
        element_ptr, &box.x, &box.y, &box.width, &box.height);
    box.width += box.x;
    box.height += box.y;
    return box;
}

/**
 * Passes a pointer motion event on to the element.
 *
 * Will forward to @ref wlmtk_element_vmt_t::pointer_motion, and (depending on
 * that return value) trigger @ref wlmtk_element_events_t::pointer_enter or
 * @ref wlmtk_element_events_t::pointer_leave.
 *
 * @param element_ptr
 * @param pointer_motion_ptr
 */
bool wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *pointer_motion_ptr);

/** Calls @ref wlmtk_element_vmt_t::pointer_button. */
static inline bool wlmtk_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    return element_ptr->vmt.pointer_button(element_ptr, button_event_ptr);
}

/** Calls @ref wlmtk_element_vmt_t::pointer_axis. */
static inline bool wlmtk_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    return element_ptr->vmt.pointer_axis(
        element_ptr, wlr_pointer_axis_event_ptr);
}

/** Calls optional @ref wlmtk_element_vmt_t::pointer_grab_cancel. */
static inline void wlmtk_element_pointer_grab_cancel(
    wlmtk_element_t *element_ptr)
{
    if (NULL != element_ptr->vmt.pointer_grab_cancel) {
        element_ptr->vmt.pointer_grab_cancel(element_ptr);
    }
}

/** Calls @ref wlmtk_element_vmt_t::keyboard_event. */
static inline bool wlmtk_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr)
{
    return element_ptr->vmt.keyboard_event(
        element_ptr, wlr_keyboard_key_event_ptr);
}

/** Calls @ref wlmtk_element_vmt_t::keyboard_sym. */
static inline bool wlmtk_element_keyboard_sym(
    wlmtk_element_t *element_ptr,
    xkb_keysym_t keysym,
    enum xkb_key_direction direction,
    uint32_t modifiers)
{
    return element_ptr->vmt.keyboard_sym(
        element_ptr, keysym, direction, modifiers);
}

/** Calls @ref wlmtk_element_vmt_t::keyboard_blur. */
static inline void wlmtk_element_keyboard_blur(
    wlmtk_element_t *element_ptr)
{
    if (NULL != element_ptr->vmt.keyboard_blur) {
        element_ptr->vmt.keyboard_blur(element_ptr);
    }
}

/**
 * Virtual method: Calls the dtor of the element's implementation.
 *
 * The implementation is required to call @ref wlmtk_element_fini().
 *
 * @param element_ptr
 */
static inline void wlmtk_element_destroy(
    wlmtk_element_t *element_ptr) {
    element_ptr->vmt.destroy(element_ptr);
}

/** Unit tests for the element. */
extern const bs_test_case_t wlmtk_element_test_cases[];

/** Fake element, useful for unit test. */
typedef struct {
    /** State of the element. */
    wlmtk_element_t           element;
    /** Original VMT. */
    wlmtk_element_vmt_t       orig_vmt;
    /** Dimensions of the fake element, in pixels. */
    struct wlr_box            dimensions;

    /** Indicates @ref wlmtk_element_vmt_t::pointer_motion() was called. */
    bool                      pointer_motion_called;

    /** Indicates @ref wlmtk_element_vmt_t::pointer_button() was called. */
    bool                      pointer_button_called;
    /** Last button event received. */
    wlmtk_button_event_t      pointer_button_event;
    /** Indicates @ref wlmtk_element_vmt_t::pointer_axis() was called. */
    bool                      pointer_axis_called;
    /** Indicates @ref wlmtk_element_vmt_t::pointer_grab_cancel() was called. */
    bool                      pointer_grab_cancel_called;
    /** Whether the fake element has keyboare focus. */
    bool                      has_keyboard_focus;
    /** Indicates that @ref wlmtk_element_vmt_t::keyboard_event() was called. */
    bool                      keyboard_event_called;
    /** Indicates that @ref wlmtk_element_vmt_t::keyboard_sym() was called. */
    bool                      keyboard_sym_called;

    /** Last axis event received. */
    struct wlr_pointer_axis_event wlr_pointer_axis_event;
} wlmtk_fake_element_t;

/**
 * Ctor for the fake element, useful for tests.
 *
 * @return A pointer to @ref wlmtk_fake_element_t. Should be destroyed via
 *     @ref wlmtk_element_destroy, by passing the pointer to
 *     @ref wlmtk_fake_element_t::element as argument.
 */
wlmtk_fake_element_t *wlmtk_fake_element_create(void);

/**
 * Sets @ref wlmtk_fake_element_t::has_keyboard_focus and calls @ref
 * wlmtk_container_set_keyboard_focus_element for the parent (if set).
 *
 * @param fake_element_ptr
 */
void wlmtk_fake_element_grab_keyboard(wlmtk_fake_element_t *fake_element_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ELEMENT_H__ */
/* == End of element.h ===================================================== */
