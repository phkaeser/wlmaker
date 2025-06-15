/* ========================================================================= */
/**
 * @file container.c
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

#include "container.h"

#include <linux/input-event-codes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <toolkit/util.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE
#include <xkbcommon/xkbcommon.h>

#include "input.h"

/* == Declarations ========================================================= */

static void _wlmtk_container_element_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr, void *ud_ptr);
static struct wlr_scene_node *_wlmtk_container_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void _wlmtk_container_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static bool _wlmtk_container_element_pointer_accepts_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr);
static void _wlmtk_container_element_pointer_blur(
    wlmtk_element_t *element_ptr);
static bool _wlmtk_container_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_container_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static void _wlmtk_container_element_pointer_grab_cancel(
    wlmtk_element_t *element_ptr);
static void _wlmtk_container_element_keyboard_blur(
    wlmtk_element_t *element_ptr);
static bool _wlmtk_container_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr);
static bool _wlmtk_container_element_keyboard_sym(
    wlmtk_element_t *element_ptr,
    xkb_keysym_t keysym,
    enum xkb_key_direction direction,
    uint32_t modifiers);

static void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);
static bool _wlmtk_container_update_layout(wlmtk_container_t *container_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_vmt_t container_element_vmt = {
    .create_scene_node = _wlmtk_container_element_create_scene_node,
    .get_dimensions = _wlmtk_container_element_get_dimensions,
    .pointer_accepts_motion = _wlmtk_container_element_pointer_accepts_motion,
    .pointer_blur = _wlmtk_container_element_pointer_blur,
    .pointer_button = _wlmtk_container_element_pointer_button,
    .pointer_axis = _wlmtk_container_element_pointer_axis,
    .pointer_grab_cancel = _wlmtk_container_element_pointer_grab_cancel,
    .keyboard_blur = _wlmtk_container_element_keyboard_blur,
    .keyboard_event = _wlmtk_container_element_keyboard_event,
    .keyboard_sym = _wlmtk_container_element_keyboard_sym,
};

/** Default virtual method table. Initializes non-abstract methods. */
static const wlmtk_container_vmt_t container_vmt = {
    .update_layout = _wlmtk_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init(wlmtk_container_t *container_ptr)
{
    BS_ASSERT(NULL != container_ptr);
    *container_ptr = (wlmtk_container_t){ .vmt = container_vmt };

    if (!wlmtk_element_init(&container_ptr->super_element)) {
        return false;
    }
    container_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &container_ptr->super_element, &container_element_vmt);

    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init_attached(
    wlmtk_container_t *container_ptr,
    struct wlr_scene_tree *root_wlr_scene_tree_ptr)
{
    if (!wlmtk_container_init(container_ptr)) return false;

    container_ptr->super_element.wlr_scene_node_ptr =
        _wlmtk_container_element_create_scene_node(
            &container_ptr->super_element, root_wlr_scene_tree_ptr);
    if (NULL == container_ptr->super_element.wlr_scene_node_ptr) {
        wlmtk_container_fini(container_ptr);
        return false;
    }

    BS_ASSERT(NULL != container_ptr->super_element.wlr_scene_node_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_container_vmt_t wlmtk_container_extend(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_vmt_t *container_vmt_ptr)
{
    wlmtk_container_vmt_t orig_vmt = container_ptr->vmt;

    if (NULL != container_vmt_ptr->update_layout) {
        container_ptr->vmt.update_layout =
            container_vmt_ptr->update_layout;
    }
    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_fini(wlmtk_container_t *container_ptr)
{
    bs_dllist_for_each(
        &container_ptr->elements,
        _wlmtk_container_element_dlnode_destroy,
        container_ptr);

    // For containers created with wlmtk_container_init_attached(): We also
    // need to remove references to the WLR scene tree.
    if (NULL != container_ptr->wlr_scene_tree_ptr) {
        BS_ASSERT(NULL == container_ptr->super_element.parent_container_ptr);
        wlr_scene_node_destroy(&container_ptr->wlr_scene_tree_ptr->node);
        container_ptr->wlr_scene_tree_ptr = NULL;
        container_ptr->super_element.wlr_scene_node_ptr = NULL;
    }

    wlmtk_element_fini(&container_ptr->super_element);
    *container_ptr = (wlmtk_container_t){};
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);

    // Before adding the element: Clear potentially set grabs in the child.
    wlmtk_element_pointer_grab_cancel(element_ptr);

    bs_dllist_push_front(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    wlmtk_element_set_parent_container(element_ptr, container_ptr);

    wlmtk_container_update_layout_and_pointer_focus(container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_add_element_atop(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *reference_element_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
    BS_ASSERT(
        NULL == reference_element_ptr ||
        container_ptr == reference_element_ptr->parent_container_ptr);

    if (NULL == reference_element_ptr) {
        bs_dllist_push_back(
            &container_ptr->elements,
            wlmtk_dlnode_from_element(element_ptr));
    } else {
        bs_dllist_insert_node_before(
            &container_ptr->elements,
            wlmtk_dlnode_from_element(reference_element_ptr),
            wlmtk_dlnode_from_element(element_ptr));
    }

    wlmtk_element_set_parent_container(element_ptr, container_ptr);
    if (NULL != element_ptr->wlr_scene_node_ptr) {

        if (NULL == reference_element_ptr) {
            wlr_scene_node_lower_to_bottom(element_ptr->wlr_scene_node_ptr);
        } else {
            BS_ASSERT(NULL != reference_element_ptr->wlr_scene_node_ptr);
            wlr_scene_node_place_above(
                element_ptr->wlr_scene_node_ptr,
                reference_element_ptr->wlr_scene_node_ptr);
        }
    }
    wlmtk_container_update_layout_and_pointer_focus(container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_remove_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(element_ptr->parent_container_ptr == container_ptr);

    wlmtk_element_set_parent_container(element_ptr, NULL);
    bs_dllist_remove(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    wlmtk_element_pointer_blur(element_ptr);

    if (container_ptr->pointer_grab_element_ptr == element_ptr) {
        _wlmtk_container_element_pointer_grab_cancel(
            &container_ptr->super_element);
        if (NULL != container_ptr->super_element.parent_container_ptr) {
            wlmtk_container_pointer_grab_release(
                container_ptr->super_element.parent_container_ptr,
                &container_ptr->super_element);
        }
    }
    if (container_ptr->left_button_element_ptr == element_ptr) {
        container_ptr->left_button_element_ptr = NULL;
    }
    if (container_ptr->keyboard_focus_element_ptr == element_ptr) {
        wlmtk_container_set_keyboard_focus_element(
            container_ptr, element_ptr, false);
    }


    wlmtk_container_update_layout_and_pointer_focus(container_ptr);

    BS_ASSERT(element_ptr != container_ptr->pointer_focus_element_ptr);
    BS_ASSERT(element_ptr != container_ptr->keyboard_focus_element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_raise_element_to_top(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(element_ptr->parent_container_ptr == container_ptr);

    // Already at the top? Nothing to do.
    if (wlmtk_dlnode_from_element(element_ptr) ==
        container_ptr->elements.head_ptr) return;

    bs_dllist_remove(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    bs_dllist_push_front(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));

    if (NULL != element_ptr->wlr_scene_node_ptr) {
        wlr_scene_node_raise_to_top(element_ptr->wlr_scene_node_ptr);
    }

    wlmtk_container_update_layout_and_pointer_focus(container_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_container_request_pointer_focus(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    BS_ASSERT(element_ptr->parent_container_ptr == container_ptr);

    // Element already has focus. Nothing to do.
    if (container_ptr->pointer_focus_element_ptr == element_ptr) {
        BS_ASSERT(container_ptr->super_element.pointer_inside);
        return true;
    }

    // A different element had focus. Blur it first. But prevent our element
    // from accepting that blur -- we retain pointer focus for element_ptr.
    if (container_ptr->pointer_focus_element_ptr != element_ptr &&
        NULL != container_ptr->pointer_focus_element_ptr) {
        container_ptr->super_element.inhibit_pointer_blur = true;
        wlmtk_element_pointer_blur(
            container_ptr->pointer_focus_element_ptr);
        container_ptr->super_element.inhibit_pointer_blur = false;

    }
    container_ptr->pointer_focus_element_ptr = element_ptr;

    bool rv = wlmtk_element_pointer_focus(
        &container_ptr->super_element,
        motion_event_ptr);
    if (!rv) container_ptr->pointer_focus_element_ptr = NULL;
    return rv;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_pointer_grab(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    BS_ASSERT(container_ptr == element_ptr->parent_container_ptr);
    // We only accept elements that have a grab_cancel method.
    BS_ASSERT(NULL != element_ptr->vmt.pointer_grab_cancel);

    if (container_ptr->pointer_grab_element_ptr == element_ptr) return;

    // Cancel a currently-held grab.
    _wlmtk_container_element_pointer_grab_cancel(
        &container_ptr->super_element);

    // Then, setup the grab.
    container_ptr->pointer_grab_element_ptr = element_ptr;
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_pointer_grab(
            container_ptr->super_element.parent_container_ptr,
            &container_ptr->super_element);
    }

    if (NULL != container_ptr->pointer_focus_element_ptr &&
        container_ptr->pointer_focus_element_ptr != element_ptr) {
        wlmtk_element_pointer_blur(container_ptr->pointer_focus_element_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_pointer_grab_release(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    BS_ASSERT(container_ptr == element_ptr->parent_container_ptr);

    if (container_ptr->pointer_grab_element_ptr != element_ptr) return;

    container_ptr->pointer_grab_element_ptr = NULL;
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_pointer_grab_release(
            container_ptr->super_element.parent_container_ptr,
            &container_ptr->super_element);
    } else {
        // Re-trigger focus computation, from top-level.
        wlmtk_container_update_layout_and_pointer_focus(container_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_set_keyboard_focus_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr,
    bool enabled)
{
    BS_ASSERT(NULL != element_ptr);
    if (enabled) {
        // Guard clause: Nothing to do, if that element already has focus.
        if (container_ptr->keyboard_focus_element_ptr == element_ptr) return;

        if (NULL != container_ptr->keyboard_focus_element_ptr) {
            wlmtk_element_keyboard_blur
                (container_ptr->keyboard_focus_element_ptr);
        }
        container_ptr->keyboard_focus_element_ptr = element_ptr;

    } else {
        // Guard clause: Nothing to do if that element does NOT have focus.
        if (container_ptr->keyboard_focus_element_ptr != element_ptr) return;

        container_ptr->keyboard_focus_element_ptr = NULL;
    }

    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_set_keyboard_focus_element(
            container_ptr->super_element.parent_container_ptr,
            &container_ptr->super_element,
            enabled);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_update_layout_and_pointer_focus(
    wlmtk_container_t *container_ptr)
{
    if (container_ptr->inhibit_layout_update) return;

    container_ptr->inhibit_layout_update = true;
    container_ptr->vmt.update_layout(container_ptr);
    container_ptr->inhibit_layout_update = false;

    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout_and_pointer_focus(
            container_ptr->super_element.parent_container_ptr);
    } else {
        if (container_ptr->super_element.pointer_inside) {
            _wlmtk_container_element_pointer_accepts_motion(
                &container_ptr->super_element,
                &container_ptr->super_element.last_pointer_motion_event);
        } else {
            BS_ASSERT(NULL == container_ptr->pointer_focus_element_ptr);
        }
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_tree *wlmtk_container_wlr_scene_tree(
    wlmtk_container_t *container_ptr)
{
    return container_ptr->wlr_scene_tree_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Calls dtor for @ref wlmtk_element_t at `dlnode_ptr` in `ud_ptr`. */
void _wlmtk_container_element_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
    wlmtk_container_t *container_ptr = ud_ptr;

    wlmtk_container_remove_element(container_ptr, element_ptr);
    wlmtk_element_destroy(element_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * Creates the wlroots scene graph tree for the container, and will attach all
 * already-contained elements to the scene graph, as well.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the scene graph API node.
 */
struct wlr_scene_node *_wlmtk_container_element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    BS_ASSERT(NULL == container_ptr->wlr_scene_tree_ptr);
    container_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr);
    BS_ASSERT(NULL != container_ptr->wlr_scene_tree_ptr);

    // Build the nodes from tail to head: Adding an element to the scene graph
    // will always put it on top, so this adds the elements in desired order.
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.tail_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->prev_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    wlmtk_util_connect_listener_signal(
        &container_ptr->wlr_scene_tree_ptr->node.events.destroy,
        &container_ptr->wlr_scene_tree_node_destroy_listener,
        handle_wlr_scene_tree_node_destroy);
    return &container_ptr->wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void _wlmtk_container_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    int left = INT32_MAX, top = INT32_MAX;
    int right = INT32_MIN, bottom = INT32_MIN;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        if (!element_ptr->visible) continue;

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_dimensions(element_ptr, &x1, &y1, &x2, &y2);
        left = BS_MIN(left, x_pos + x1);
        top = BS_MIN(top, y_pos + y1);
        right = BS_MAX(right, x_pos + x2);
        bottom = BS_MAX(bottom, y_pos + y2);
    }

    if (left >= right) { left = 0; right = 0; }
    if (top >= bottom) { top = 0; bottom = 0; }

    if (NULL != left_ptr) *left_ptr = left;
    if (NULL != top_ptr) *top_ptr = top;
    if (NULL != right_ptr) *right_ptr = right;
    if (NULL != bottom_ptr) *bottom_ptr = bottom;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_accepts_motion.
 *
 * @param element_ptr
 * @param motion_event_ptr
 *
 * @return Whether this container has an element that accepts the emotion.
 */
bool _wlmtk_container_element_pointer_accepts_motion(
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    wlmtk_pointer_motion_event_t child_motion = *motion_event_ptr;
    int x_pos, y_pos;

    if (NULL != container_ptr->pointer_grab_element_ptr) {
        child_motion = *motion_event_ptr;
        wlmtk_element_get_position(
            container_ptr->pointer_grab_element_ptr, &x_pos, &y_pos);
        child_motion.x = motion_event_ptr->x - x_pos;
        child_motion.y = motion_event_ptr->y - y_pos;

        return wlmtk_element_pointer_motion(
            container_ptr->pointer_grab_element_ptr,
            &child_motion);
    }

    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *child_element_ptr =
            wlmtk_element_from_dlnode(dlnode_ptr);

        wlmtk_element_get_position(child_element_ptr, &x_pos, &y_pos);
        child_motion.x = motion_event_ptr->x - x_pos;
        child_motion.y = motion_event_ptr->y - y_pos;

        container_ptr->super_element.inhibit_pointer_blur = true;
        bool rv = wlmtk_element_pointer_motion(child_element_ptr, &child_motion);
        container_ptr->super_element.inhibit_pointer_blur = false;
        if (rv) {
            BS_ASSERT(NULL != container_ptr->pointer_focus_element_ptr);
            BS_ASSERT(container_ptr->super_element.pointer_inside);
            return true;
        }
    }

    container_ptr->pointer_focus_element_ptr = NULL;
    wlmtk_element_pointer_blur(element_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_blur. Blurs the element
 * currently having focus, then invokes the superclass' original method.
 *
 * @param element_ptr
 */
void _wlmtk_container_element_pointer_blur(wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    if (NULL != container_ptr->pointer_focus_element_ptr) {
        wlmtk_element_pointer_blur(
            container_ptr->pointer_focus_element_ptr);
        container_ptr->pointer_focus_element_ptr = NULL;
    }
    container_ptr->orig_super_element_vmt.pointer_blur(element_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's pointer_button() method. Forwards it to the
 * element currently having pointer focus.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true if the button was handled.
 */
bool _wlmtk_container_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    bool accepted = false;

    if (NULL != container_ptr->pointer_grab_element_ptr) {
        return wlmtk_element_pointer_button(
            container_ptr->pointer_grab_element_ptr,
            button_event_ptr);
    }

    // TODO: Generalize this for non-LEFT buttons.
    if (BTN_LEFT == button_event_ptr->button) {

        switch (button_event_ptr->type) {
        case WLMTK_BUTTON_DOWN:
            // Forward to the pointer focus element, if any. If it
            // was accepted: remember the element.
            if (NULL != container_ptr->pointer_focus_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->pointer_focus_element_ptr,
                    button_event_ptr);
                if (accepted) {
                    container_ptr->left_button_element_ptr =
                        container_ptr->pointer_focus_element_ptr;
                } else {
                    container_ptr->left_button_element_ptr = NULL;
                }
            }
            break;

        case WLMTK_BUTTON_UP:
            // Forward to the element that received the DOWN, if any.
            if (NULL != container_ptr->left_button_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->left_button_element_ptr,
                    button_event_ptr);
            }
            break;

        case WLMTK_BUTTON_CLICK:
        case WLMTK_BUTTON_DOUBLE_CLICK:
            // Will only be forwarded, if the element still (or again)
            // has pointer focus.
            if (NULL != container_ptr->left_button_element_ptr &&
                container_ptr->left_button_element_ptr ==
                container_ptr->pointer_focus_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->left_button_element_ptr,
                    button_event_ptr);
            }
            break;


        default:  // Uh, don't know about this...
            bs_log(BS_FATAL, "Unhandled button type %d",
                   button_event_ptr->type);
        }

        return accepted;
    }

    if (NULL == container_ptr->pointer_focus_element_ptr) return false;

    return wlmtk_element_pointer_button(
        container_ptr->pointer_focus_element_ptr,
        button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's axis method: Handles axis events, by
 * forwarding it to the element having pointer focus.
 *
 * @param element_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return true if the axis event was handled.
 */
bool _wlmtk_container_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    if (NULL != container_ptr->pointer_grab_element_ptr) {
        return wlmtk_element_pointer_axis(
            container_ptr->pointer_grab_element_ptr,
            wlr_pointer_axis_event_ptr);
    }

    if (NULL != container_ptr->pointer_focus_element_ptr) {
        return wlmtk_element_pointer_axis(
            container_ptr->pointer_focus_element_ptr,
            wlr_pointer_axis_event_ptr);
    } else {
        return false;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_grab_cancel.
 *
 * Cancels an existing pointer grab.
 *
 * @param element_ptr
 */
void _wlmtk_container_element_pointer_grab_cancel(
    wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    if (NULL == container_ptr->pointer_grab_element_ptr) return;

    wlmtk_element_pointer_grab_cancel(
        container_ptr->pointer_grab_element_ptr);
    container_ptr->pointer_grab_element_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::keyboard_blur. Blurs all children. */
void _wlmtk_container_element_keyboard_blur(wlmtk_element_t *element_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    // Guard clause: No elements having keyboard focus, return right away.
    if (NULL == container_ptr->keyboard_focus_element_ptr) return;

    wlmtk_element_keyboard_blur(container_ptr->keyboard_focus_element_ptr);
    container_ptr->keyboard_focus_element_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Handler for keyboard events: Pass to keyboard-focussed element, if any. */
bool _wlmtk_container_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    // Guard clause: No focus here, return right away.
    if (NULL == container_ptr->keyboard_focus_element_ptr) return false;

    return wlmtk_element_keyboard_event(
        container_ptr->keyboard_focus_element_ptr,
        wlr_keyboard_key_event_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for translated keyboard events: To keyboard-focussed, if any. */
bool _wlmtk_container_element_keyboard_sym(
    wlmtk_element_t *element_ptr,
    xkb_keysym_t keysym,
    enum xkb_key_direction direction,
    uint32_t modifiers)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    // Guard clause: No focus here, return right away.
    if (NULL == container_ptr->keyboard_focus_element_ptr) return false;

    return wlmtk_element_keyboard_sym(
        container_ptr->keyboard_focus_element_ptr,
        keysym, direction, modifiers);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'destroy' callback of wlr_scene_tree_ptr->node.
 *
 * Will also detach (but not destroy) each of the still-contained elements.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_container_t, wlr_scene_tree_node_destroy_listener);

    container_ptr->wlr_scene_tree_ptr = NULL;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        // Will read the parent container's wlr_scene_tree_ptr == NULL.
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    // Since this is a callback from the tree node dtor, the tree is going to
    // be destroyed. We are using this to reset the container's reference.
    wl_list_remove(&container_ptr->wlr_scene_tree_node_destroy_listener.link);
}

/* ------------------------------------------------------------------------- */
/**
 * Base implementation of wlmtk_container_vmt_t::update_layout.
 *
 * @param container_ptr
 *
 * @return false, since a nondescript container doesn't rearrange elements.
 */
bool _wlmtk_container_update_layout (
    __UNUSED__ wlmtk_container_t *container_ptr)
{
    return false;
}

/* == Helper for unit tests: A fake container with a tree, as parent ======= */

/** State of the "fake" parent container. Refers to a scene graph. */
typedef struct {
    /** The actual container */
    wlmtk_container_t         container;
    /** A scene graph. Not attached to any output, */
    struct wlr_scene          *wlr_scene_ptr;
} fake_parent_container_t;

/* ------------------------------------------------------------------------- */
wlmtk_container_t *wlmtk_container_create_fake_parent(void)
{
    fake_parent_container_t *fake_parent_container_ptr = logged_calloc(
        1, sizeof(fake_parent_container_t));
    if (NULL == fake_parent_container_ptr) return NULL;

    fake_parent_container_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == fake_parent_container_ptr->wlr_scene_ptr) {
        wlmtk_container_destroy_fake_parent(
            &fake_parent_container_ptr->container);
        return NULL;
    }

    if (!wlmtk_container_init_attached(
            &fake_parent_container_ptr->container,
            &fake_parent_container_ptr->wlr_scene_ptr->tree)) {
        wlmtk_container_destroy_fake_parent(
            &fake_parent_container_ptr->container);
        return NULL;
    }

    return &fake_parent_container_ptr->container;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_destroy_fake_parent(wlmtk_container_t *container_ptr)
{
    fake_parent_container_t *fake_parent_container_ptr = BS_CONTAINER_OF(
        container_ptr, fake_parent_container_t, container);

    wlmtk_container_fini(&fake_parent_container_ptr->container);

    if (NULL != fake_parent_container_ptr->wlr_scene_ptr) {
       wlr_scene_node_destroy(
            &fake_parent_container_ptr->wlr_scene_ptr->tree.node);
        fake_parent_container_ptr->wlr_scene_ptr = NULL;
    }

    free(fake_parent_container_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_add_remove(bs_test_t *test_ptr);
static void test_add_remove_with_scene_graph(bs_test_t *test_ptr);
static void test_add_with_raise(bs_test_t *test_ptr);
static void test_pointer_button(bs_test_t *test_ptr);
static void test_pointer_grab(bs_test_t *test_ptr);
static void test_pointer_grab_events(bs_test_t *test_ptr);
static void test_keyboard_event(bs_test_t *test_ptr);
static void test_keyboard_focus(bs_test_t *test_ptr);

static void test_pointer_axis(bs_test_t *test_ptr);
static void test_pointer_focus_with_parent(bs_test_t *test_ptr);
static void test_pointer_focus_children(bs_test_t *test_ptr);
static void test_pointer_focus_order(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_container_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "add_remove", test_add_remove },
    { 1, "add_remove_with_scene_graph", test_add_remove_with_scene_graph },
    { 1, "add_with_raise", test_add_with_raise },
    { 1, "pointer_button", test_pointer_button },
    { 1, "pointer_grab", test_pointer_grab },
    { 1, "pointer_grab_events", test_pointer_grab_events },
    { 1, "keyboard_event", test_keyboard_event },
    { 1, "keyboard_focus", test_keyboard_focus },
    { 1, "pointer_axis", test_pointer_axis },
    { 1, "pointer_focus_with_parent", test_pointer_focus_with_parent },
    { 1, "pointer_focus_children", test_pointer_focus_children },
    { 1, "test_pointer_focus_order", test_pointer_focus_order },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container));
    // Also expect the super element to be initialized.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, container.super_element.vmt.pointer_accepts_motion);

    wlmtk_container_fini(&container);
    // Also expect the super element to be un-initialized.
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, container.super_element.vmt.pointer_accepts_motion);
}

/* ------------------------------------------------------------------------- */
/** Exercises adding and removing elements, verifies destruction on fini. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container));

    wlmtk_fake_element_t *elem1_ptr, *elem2_ptr, *elem3_ptr;
    elem1_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem1_ptr);
    elem2_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem2_ptr);
    elem3_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem3_ptr);

    // Build sequence: 3 -> 2 -> 1.
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem1_ptr->element.parent_container_ptr);
    wlmtk_container_add_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem2_ptr->element.parent_container_ptr);
    wlmtk_container_add_element(&container, &elem3_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem3_ptr->element.parent_container_ptr);

    // Remove 2, then add at the bottom: 3 -> 1 -> 2.
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, elem2_ptr->element.parent_container_ptr);
    wlmtk_container_add_element_atop(&container, NULL, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, &container, elem2_ptr->element.parent_container_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_element(&elem1_ptr->element)->next_ptr,
        wlmtk_dlnode_from_element(&elem2_ptr->element));

    // Remove elem3 and add atop elem2: 1 -> 3 -> 2.
    wlmtk_container_remove_element(&container, &elem3_ptr->element);
    wlmtk_container_add_element_atop(
        &container, &elem2_ptr->element, &elem3_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_element(&elem3_ptr->element)->next_ptr,
        wlmtk_dlnode_from_element(&elem2_ptr->element));

    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);

    // Will destroy contained elements.
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that elements are attached, resp. detached from scene graph. */
void test_add_remove_with_scene_graph(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_parent_ptr);
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container));

    wlmtk_fake_element_t *fe3_ptr = wlmtk_fake_element_create();
    wlmtk_container_add_element(&container, &fe3_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe3_ptr->element.wlr_scene_node_ptr);
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    wlmtk_container_add_element(&container, &fe2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe2_ptr->element.wlr_scene_node_ptr);

    wlmtk_element_set_parent_container(
        &container.super_element, fake_parent_ptr);

    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fe3_ptr->element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fe2_ptr->element.wlr_scene_node_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.elements.head_ptr, &fe2_ptr->element.dlnode);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.elements.tail_ptr, &fe3_ptr->element.dlnode);

    // The top is at parent->children.prev (see wlr_scene_node_raise_to_top).
    // Seems counter-intuitive, since wayhland-util.h denotes `prev` to refer
    // to the last element in the list.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev,
        &fe3_ptr->element.wlr_scene_node_ptr->link);

    // Want to have the node.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, container.super_element.wlr_scene_node_ptr);

    // Fresh element: No scene graph node yet.
    wlmtk_fake_element_t *fe0_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe0_ptr->element.wlr_scene_node_ptr);

    // Add to container with attached graph: Element now has a graph node.
    wlmtk_container_add_element(&container, &fe0_ptr->element);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fe0_ptr->element.wlr_scene_node_ptr);

    // Now fe0 has to be on top, followed by fe2 and fe3.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev,
        &fe0_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev->prev,
        &fe3_ptr->element.wlr_scene_node_ptr->link);

    // One more element, but we add this atop of fe2.
    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe1_ptr->element.wlr_scene_node_ptr);
    wlmtk_container_add_element_atop(
        &container, &fe2_ptr->element, &fe1_ptr->element);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev,
        &fe0_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev,
        &fe1_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev->prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        container.wlr_scene_tree_ptr->children.prev->prev->prev->prev,
        &fe3_ptr->element.wlr_scene_node_ptr->link);

    // Remove: The element's graph node must be destroyed & cleared..
    wlmtk_container_remove_element(&container, &fe0_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe0_ptr->element.wlr_scene_node_ptr);
    wlmtk_element_destroy(&fe0_ptr->element);

    wlmtk_element_set_parent_container(&container.super_element, NULL);

    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe3_ptr->element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe2_ptr->element.wlr_scene_node_ptr);

    wlmtk_container_remove_element(&container, &fe3_ptr->element);
    wlmtk_element_destroy(&fe3_ptr->element);
    wlmtk_container_remove_element(&container, &fe2_ptr->element);
    wlmtk_element_destroy(&fe2_ptr->element);

    wlmtk_container_fini(&container);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that elements inserted at position are also placed in scene graph. */
void test_add_with_raise(bs_test_t *test_ptr)
{
    wlmtk_container_t *c_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c_ptr);
    wlmtk_element_set_visible(&c_ptr->super_element, true);

    // fe1 added. Sole element, is the top.
    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&fe1_ptr->element, true);
    wlmtk_container_add_element(c_ptr, &fe1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev,
        &fe1_ptr->element.wlr_scene_node_ptr->link);

    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0, .time_msec = 7 };
    wlmtk_element_pointer_motion(&c_ptr->super_element, &e);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    fe1_ptr->pointer_accepts_motion_called = false;
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe1_ptr->element, c_ptr->pointer_focus_element_ptr);

    // fe2 placed atop 'NULL', goes to back.
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&fe2_ptr->element, true);
    wlmtk_container_add_element_atop(c_ptr, NULL, &fe2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev->prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);

    // Raise fe2.
    wlmtk_container_raise_element_to_top(c_ptr, &fe2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev->prev,
        &fe1_ptr->element.wlr_scene_node_ptr->link);

    // Must also update pointer focus.
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe2_ptr->element, c_ptr->pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_accepts_motion_called);
    fe2_ptr->pointer_accepts_motion_called = false;

    // Now remove fe1 and add on top of fe2. Ensure scene graph has fe1 on top
    // and pointer focus is on it, too.
    wlmtk_container_remove_element(c_ptr, &fe1_ptr->element);
    wlmtk_container_add_element_atop(c_ptr, &fe2_ptr->element, &fe1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev,
        &fe1_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        c_ptr->wlr_scene_tree_ptr->children.prev->prev,
        &fe2_ptr->element.wlr_scene_node_ptr->link);
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe1_ptr->element, c_ptr->pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_accepts_motion_called);

    wlmtk_container_remove_element(c_ptr, &fe2_ptr->element);
    wlmtk_element_destroy(&fe2_ptr->element);
    wlmtk_container_remove_element(c_ptr, &fe1_ptr->element);
    wlmtk_element_destroy(&fe1_ptr->element);

    wlmtk_container_destroy_fake_parent(c_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer DOWN is forwarded to element with pointer focus. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container));
    wlmtk_element_set_visible(&container.super_element, true);

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    elem1_ptr->dimensions.width = 1;
    elem1_ptr->dimensions.height = 1;
    wlmtk_container_add_element(&container, &elem1_ptr->element);

    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 10, 10);
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    wlmtk_container_add_element_atop(&container, NULL, &elem2_ptr->element);

    wlmtk_button_event_t button = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));

    // DOWN events go to the focussed element.
    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, &e));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.left_button_element_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem1_ptr->pointer_button_called);

    // Moves, pointer focus is now on elem2.
    e = (wlmtk_pointer_motion_event_t){ .x = 10, .y = 10 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, &e))
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->element.pointer_inside);

    // The UP event is still received by elem1.
    elem1_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem1_ptr->pointer_button_called);

    // Click will be ignored
    button.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));

    // New DOWN event goes to elem2, though.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_DOWN;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // And UP event now goes to elem2.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // Here, CLICK goes to elem2.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // After removing, further UP events won't be accidentally sent there.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.left_button_element_ptr);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/**
 * Tests @ref wlmtk_container_pointer_grab and
 * @ref wlmtk_container_pointer_grab_release.
 */
void test_pointer_grab(bs_test_t *test_ptr)
{
    wlmtk_container_t c, p;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&p));
    wlmtk_container_add_element(&p, &c.super_element);

    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    wlmtk_container_add_element(&c, &fe1_ptr->element);

    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_container_add_element(&c, &fe2_ptr->element);
    fe1_ptr->pointer_grab_cancel_called = false;
    fe2_ptr->pointer_grab_cancel_called = false;

    // Basic grab/release flow: Will not call pointer_grab_cancel().
    wlmtk_container_pointer_grab(&c, &fe1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, &fe1_ptr->element, c.pointer_grab_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, &c.super_element, p.pointer_grab_element_ptr);
    wlmtk_container_pointer_grab_release(&c, &fe1_ptr->element);
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_grab_cancel_called);
    BS_TEST_VERIFY_FALSE(test_ptr, fe2_ptr->pointer_grab_cancel_called);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.pointer_grab_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_grab_element_ptr);

    // Grab that is taken over by the other element: Must be cancelled.
    wlmtk_container_pointer_grab(&c, &fe1_ptr->element);
    wlmtk_container_pointer_grab(&c, &fe2_ptr->element);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_grab_cancel_called);
    BS_TEST_VERIFY_FALSE(test_ptr, fe2_ptr->pointer_grab_cancel_called);
    BS_TEST_VERIFY_EQ(test_ptr, &fe2_ptr->element, c.pointer_grab_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, &c.super_element, p.pointer_grab_element_ptr);

    // When removing element with the grab: Call cancel first.
    wlmtk_container_remove_element(&c, &fe2_ptr->element);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_grab_cancel_called);
    wlmtk_element_destroy(&fe2_ptr->element);
    BS_TEST_VERIFY_EQ( test_ptr, NULL, c.pointer_grab_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_grab_element_ptr);

    wlmtk_container_remove_element(&p, &c.super_element);
    wlmtk_container_fini(&p);
    wlmtk_container_fini(&c);
}

/* ------------------------------------------------------------------------- */
/** Tests that element with the pointer grab receives pointer events. */
void test_pointer_grab_events(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t enter1 = {}, enter2 = {};
    wlmtk_util_test_listener_t leave1 = {}, leave2 = {};
    wlmtk_container_t c;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    wlmtk_element_set_visible(&c.super_element, true);

    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    wlmtk_element_set_visible(&fe1_ptr->element, true);
    fe1_ptr->dimensions.width = 10;
    fe1_ptr->dimensions.height = 10;
    wlmtk_container_add_element(&c, &fe1_ptr->element);
    wlmtk_util_connect_test_listener(&fe1_ptr->element.events.pointer_enter, &enter1);
    wlmtk_util_connect_test_listener(&fe1_ptr->element.events.pointer_leave, &leave1);

    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_element_set_visible(&fe2_ptr->element, true);
    wlmtk_element_set_position(&fe2_ptr->element, 10, 0);
    fe2_ptr->dimensions.width = 10;
    fe2_ptr->dimensions.height = 10;
    wlmtk_container_add_element(&c, &fe2_ptr->element);
    wlmtk_util_connect_test_listener(&fe2_ptr->element.events.pointer_enter, &enter2);
    wlmtk_util_connect_test_listener(&fe2_ptr->element.events.pointer_leave, &leave2);

    // Move pointer into first element: Must see 'enter' and 'motion'.
    wlmtk_pointer_motion_event_t e = { .x = 5, .y = 5 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &e));
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    fe1_ptr->pointer_accepts_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 1, enter1.calls);
    wlmtk_util_clear_test_listener(&enter1);

    // 2nd element grabs pointer. Axis and button events must go there.
    wlmtk_container_pointer_grab(&c, &fe2_ptr->element);
    // 1st element must get notified to no longer have pointer focus.
    BS_TEST_VERIFY_EQ(test_ptr, 1, leave1.calls);
    wlmtk_util_clear_test_listener(&leave1);
    fe1_ptr->pointer_accepts_motion_called = false;
    wlmtk_button_event_t button_event = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    wlmtk_element_pointer_button(&c.super_element, &button_event);
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_button_called);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_button_called);
    struct wlr_pointer_axis_event axis_event = {};
    wlmtk_element_pointer_axis(&c.super_element, &axis_event);
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_axis_called);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_axis_called);

    // A motion within the 1st element: Trigger an out-of-area motion
    // event to 2nd element.
    e = (wlmtk_pointer_motion_event_t){ .x = 8, .y = 5 };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &e));
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_accepts_motion_called);
    fe2_ptr->pointer_accepts_motion_called = false;

    // A motion into the 2nd element: Trigger motion and enter().
    e = (wlmtk_pointer_motion_event_t){ .x = 13, .y = 5 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &e));
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_accepts_motion_called);
    fe2_ptr->pointer_accepts_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 1, enter2.calls);
    wlmtk_util_clear_test_listener(&enter2);

    // A motion back into the 2nd element: Trigger motion and leave().
    e = (wlmtk_pointer_motion_event_t){ .x = 8, .y = 5 };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &e));
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_accepts_motion_called);
    fe2_ptr->pointer_accepts_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 1, leave2.calls);
    wlmtk_util_clear_test_listener(&leave2);

    // Second element releases the grab. 1st element must receive enter().
    wlmtk_container_pointer_grab_release(&c, &fe2_ptr->element);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &e));
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 1, enter1.calls);

    wlmtk_container_fini(&c);
}

/* ------------------------------------------------------------------------- */
/** Tests that keyboard event are forwarded to element with keyboard focus. */
void test_keyboard_event(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container));
    wlmtk_container_t parent;
    BS_ASSERT(wlmtk_container_init(&parent));
    wlmtk_container_add_element(&parent, &container.super_element);

    struct wlr_keyboard_key_event event = {};
    wlmtk_element_t *parent_elptr = &parent.super_element;

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    wlmtk_container_add_element(&container, &fe_ptr->element);

    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_keyboard_event(parent_elptr, &event));
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->keyboard_event_called);

    // Obtains keyboard focus for the fake element.
    wlmtk_fake_element_grab_keyboard(fe_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_event(parent_elptr, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->keyboard_event_called);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(parent_elptr, 0, 0, 0));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->keyboard_sym_called);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &fe_ptr->element,
        container.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container.super_element,
        parent.keyboard_focus_element_ptr);

    // Release keyboard focus for *not* this element: Retains focus.
    fe_ptr->keyboard_event_called = false;
    fe_ptr->keyboard_sym_called = false;
    wlmtk_container_set_keyboard_focus_element(&container, parent_elptr, false);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_event(parent_elptr, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->keyboard_event_called);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(parent_elptr, 0, 0, 0));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->keyboard_sym_called);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &fe_ptr->element,
        container.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container.super_element,
        parent.keyboard_focus_element_ptr);

    // Release keyboard focus for *this* element: Releases focus.
    fe_ptr->keyboard_event_called = false;
    fe_ptr->keyboard_sym_called = false;
    wlmtk_container_set_keyboard_focus_element(&container, &fe_ptr->element, false);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_keyboard_event(parent_elptr, &event));
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->keyboard_event_called);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_keyboard_sym(parent_elptr, 0, 0, 0));
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->keyboard_sym_called);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, parent.keyboard_focus_element_ptr);

    wlmtk_container_remove_element(&container, &fe_ptr->element);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_container_remove_element(&parent, &container.super_element);
    wlmtk_container_fini(&parent);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Test that keyboard focus is propagated and respects element removal. */
void test_keyboard_focus(bs_test_t *test_ptr)
{
    wlmtk_container_t c, p;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&p));
    wlmtk_container_add_element(&p, &c.super_element);

    // Two child elements to c.
    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    wlmtk_container_add_element(&c, &fe1_ptr->element);

    // One extra child element to p.
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_container_add_element(&p, &fe2_ptr->element);

    // fe1 of c grabs focus. Ensure it is propagated.
    wlmtk_fake_element_grab_keyboard(fe1_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->has_keyboard_focus);
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe1_ptr->element, c.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, &c.super_element, p.keyboard_focus_element_ptr);

    // fe2 of p sets focus. Must disable focus for c.
    wlmtk_fake_element_grab_keyboard(fe2_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->has_keyboard_focus);
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->has_keyboard_focus);
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe2_ptr->element, p.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, c.keyboard_focus_element_ptr);

    // fe1 of c re-gains focus. Must disable focus for fe2.
    wlmtk_fake_element_grab_keyboard(fe1_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->has_keyboard_focus);
    BS_TEST_VERIFY_FALSE(test_ptr, fe2_ptr->has_keyboard_focus);
    BS_TEST_VERIFY_EQ(
        test_ptr, &c.super_element, p.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, &fe1_ptr->element, c.keyboard_focus_element_ptr);

    // Remove fe1. There is no more keyboard focus to fall back to.
    wlmtk_container_remove_element(&c, &fe1_ptr->element);
    wlmtk_element_destroy(&fe1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.keyboard_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.keyboard_focus_element_ptr);

    wlmtk_container_remove_element(&p, &c.super_element);
    wlmtk_container_fini(&c);
    // fe2 is collected during cleanup of &p.
    wlmtk_container_fini(&p);
}

/* ------------------------------------------------------------------------- */
/** Tests that axis events are forwarded to element with pointer focus. */
void test_pointer_axis(bs_test_t *test_ptr)
{
    struct wlr_pointer_axis_event event = {};
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container));
    wlmtk_element_set_visible(&container.super_element, true);

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    elem1_ptr->dimensions.width = 1;
    elem1_ptr->dimensions.height = 1;
    wlmtk_container_add_element(&container, &elem1_ptr->element);

    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 10, 10);
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    wlmtk_container_add_element_atop(&container, NULL, &elem2_ptr->element);

    // Pointer on elem1, axis goes there.
    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, &e));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_axis(&container.super_element, &event));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_axis_called);
    elem1_ptr->pointer_axis_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_axis_called);

    // Pointer on elem2, axis goes there.
    e = (wlmtk_pointer_motion_event_t){ .x = 10, .y = 10 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, &e));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_axis(&container.super_element, &event));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_axis_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_axis_called);

    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);

    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_axis(&container.super_element, &event));

    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests pointer focus. */
static void test_pointer_focus_with_parent(bs_test_t *test_ptr)
{
    wlmtk_container_t p, c;
    wlmtk_util_test_listener_t p_enter, p_leave, c_enter, c_leave,
        fe_enter, fe_leave;

    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&p));
    wlmtk_util_connect_test_listener(
        &p.super_element.events.pointer_enter, &p_enter);
    wlmtk_util_connect_test_listener(
        &p.super_element.events.pointer_leave, &p_leave);
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    wlmtk_util_connect_test_listener(
        &c.super_element.events.pointer_enter, &c_enter);
    wlmtk_util_connect_test_listener(
        &c.super_element.events.pointer_leave, &c_leave);
    wlmtk_element_set_position(&c.super_element, 10, 0);
    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_util_connect_test_listener(
        &fe_ptr->element.events.pointer_enter, &fe_enter);
    wlmtk_util_connect_test_listener(
        &fe_ptr->element.events.pointer_leave, &fe_leave);

    wlmtk_element_set_position(&fe_ptr->element, 0, 20);
    wlmtk_element_set_visible(&fe_ptr->element, true);
    wlmtk_element_set_visible(&c.super_element, true);
    wlmtk_element_set_visible(&p.super_element, true);

    wlmtk_container_add_element(&p, &c.super_element);
    wlmtk_container_add_element(&c, &fe_ptr->element);

    BS_TEST_VERIFY_EQ(test_ptr, 0, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, p_leave.calls);

    // Motion into fe_ptr. Must trigger focus throughout.
    wlmtk_pointer_motion_event_t e = { .x = 11.0, .y = 22.0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&p.super_element, &e));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_TRUE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &fe_ptr->element,
        c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &c.super_element,
        p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, p_leave.calls);

    // Blurring the container must blur throughout.
    wlmtk_element_pointer_blur(&p.super_element);
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_FALSE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, p_leave.calls);

    // Focus it once more, verify.
    wlmtk_element_pointer_focus(&fe_ptr->element, &e);
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_TRUE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &fe_ptr->element,
        c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &c.super_element,
        p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, p_leave.calls);

    // Remove the element. It had focus, so blur throughout.
    wlmtk_container_remove_element(&c, &fe_ptr->element);
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_FALSE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p_leave.calls);

    // TODO(kaeser@gubbe.ch): Remove post the pointer migration.
    p.super_element.last_pointer_motion_event.x = NAN;
    p.super_element.last_pointer_motion_event.y = NAN;
    // Add it back. Container didn't have focus, so won't do anything.
    wlmtk_container_add_element(&c, &fe_ptr->element);
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_FALSE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_leave.calls);

    // Send in a motion. Must enable the element.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&p.super_element, &e));
    BS_TEST_VERIFY_TRUE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_TRUE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &fe_ptr->element,
        c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &c.super_element,
        p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 3, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p_leave.calls);

    // Remove the c. Must send LEAVE to all.
    wlmtk_container_remove_element(&p, &c.super_element);
    BS_TEST_VERIFY_FALSE(test_ptr, fe_ptr->element.pointer_inside);
    BS_TEST_VERIFY_FALSE(test_ptr, c.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, c.pointer_focus_element_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, p.super_element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, p.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 3, fe_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, fe_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, c_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, c_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, p_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 3, p_leave.calls);

    wlmtk_container_remove_element(&c, &fe_ptr->element);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_container_fini(&p);
    wlmtk_container_fini(&c);
}

/* ------------------------------------------------------------------------- */
/** Tests moving pointer focus between children elements. */
void test_pointer_focus_children(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t c_enter, c_leave, e1_enter, e1_leave,
        e2_enter, e2_leave;

    wlmtk_container_t c;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    wlmtk_element_set_visible(&c.super_element, true);
    wlmtk_util_connect_test_listener(
        &c.super_element.events.pointer_enter, &c_enter);
    wlmtk_util_connect_test_listener(
        &c.super_element.events.pointer_leave, &c_leave);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, e1_ptr);
    wlmtk_element_set_position(&e1_ptr->element, 1, 1);
    wlmtk_util_connect_test_listener(
        &e1_ptr->element.events.pointer_enter, &e1_enter);
    wlmtk_util_connect_test_listener(
        &e1_ptr->element.events.pointer_leave, &e1_leave);
    wlmtk_element_set_visible(&e1_ptr->element, true);
    wlmtk_container_add_element(&c, &e1_ptr->element);

    // 1. Move pointer onto the one element: Receives focus.
    wlmtk_pointer_motion_event_t m = { .x = 0.0, .y = 0.0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &m));
    BS_TEST_VERIFY_TRUE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);

    // 2. Add second element, atop: Receives focus, no 'leave' for container.
    BS_TEST_VERIFY_EQ(test_ptr, 0, e2_enter.calls);
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, e2_ptr);
    wlmtk_util_connect_test_listener(
        &e2_ptr->element.events.pointer_enter, &e2_enter);
    wlmtk_util_connect_test_listener(
        &e2_ptr->element.events.pointer_leave, &e2_leave);
    wlmtk_element_set_visible(&e2_ptr->element, true);
    wlmtk_container_add_element(&c, &e2_ptr->element);
    BS_TEST_VERIFY_FALSE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_leave.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e2_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);

    // 3. Make second element invisible. Blur, E1 focus, no container leave.
    wlmtk_element_set_visible(&e2_ptr->element, false);
    BS_TEST_VERIFY_TRUE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_enter.calls);
    BS_TEST_VERIFY_FALSE(test_ptr, e2_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);

    // 4. Make it visible again. Receives focus.
    wlmtk_element_set_visible(&e2_ptr->element, true);
    BS_TEST_VERIFY_FALSE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_leave.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e2_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);

    // 5. Move pointer to where only e1 is. E2 blurs, E1 focus, no C leave.
    m = (wlmtk_pointer_motion_event_t ){ .x = 3.0, .y = 4.0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &m));
    BS_TEST_VERIFY_TRUE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 3, e1_enter.calls);
    BS_TEST_VERIFY_FALSE(test_ptr, e2_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e2_leave.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_enter.calls);

    BS_TEST_VERIFY_EQ(test_ptr, 0, c_leave.calls);
    wlmtk_container_remove_element(&c, &e2_ptr->element);
    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_container_remove_element(&c, &e1_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 1, c_leave.calls);
    wlmtk_container_fini(&c);
}

/* ------------------------------------------------------------------------- */
/** Tests pointer focus when moving elements order (and position). */
void test_pointer_focus_order(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t e1_enter, e1_leave, e2_enter, e2_leave;

    wlmtk_container_t c;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_container_init(&c));
    wlmtk_element_set_visible(&c.super_element, true);

    wlmtk_fake_element_t *e1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, e1_ptr);
    wlmtk_element_set_position(&e1_ptr->element, 1, 1);
    wlmtk_util_connect_test_listener(
        &e1_ptr->element.events.pointer_enter, &e1_enter);
    wlmtk_util_connect_test_listener(
        &e1_ptr->element.events.pointer_leave, &e1_leave);
    wlmtk_element_set_visible(&e1_ptr->element, true);
    wlmtk_container_add_element(&c, &e1_ptr->element);

    // 1. Initial motion. Focus to e1.
    wlmtk_pointer_motion_event_t m = { .x = 0.0, .y = 0.0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&c.super_element, &m));
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_enter.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e1_ptr->element.pointer_inside);

    // 2. Add e2, on top e1. Focus to e2.
    wlmtk_fake_element_t *e2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, e2_ptr);
    wlmtk_util_connect_test_listener(
        &e2_ptr->element.events.pointer_enter, &e2_enter);
    wlmtk_util_connect_test_listener(
        &e2_ptr->element.events.pointer_leave, &e2_leave);
    wlmtk_element_set_visible(&e2_ptr->element, true);
    wlmtk_container_add_element_atop(&c, &e1_ptr->element, &e2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_leave.calls);
    BS_TEST_VERIFY_FALSE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_enter.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e2_ptr->element.pointer_inside);

    // 3. Raise e1. Must re-gain focus.
    wlmtk_container_raise_element_to_top(&c, &e1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e1_leave.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_leave.calls);
    BS_TEST_VERIFY_FALSE(test_ptr, e2_ptr->element.pointer_inside);

    // 4. Move e1 away. Must re-trigger focus computation, e2 gets it.
    wlmtk_element_set_position(&e1_ptr->element, 20, 0);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e1_leave.calls);
    BS_TEST_VERIFY_FALSE(test_ptr, e1_ptr->element.pointer_inside);
    BS_TEST_VERIFY_EQ(test_ptr, 2, e2_enter.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, e2_leave.calls);
    BS_TEST_VERIFY_TRUE(test_ptr, e2_ptr->element.pointer_inside);

    wlmtk_container_remove_element(&c, &e2_ptr->element);
    wlmtk_element_destroy(&e2_ptr->element);
    wlmtk_container_remove_element(&c, &e1_ptr->element);
    wlmtk_element_destroy(&e1_ptr->element);
    wlmtk_container_fini(&c);
}
/* == End of container.c =================================================== */
