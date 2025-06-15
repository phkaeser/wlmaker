/* ========================================================================= */
/**
 * @file container.h
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
#ifndef __WLMTK_CONTAINER_H__
#define __WLMTK_CONTAINER_H__

#include <stdbool.h>
#include <wayland-server-core.h>

#include "libbase/libbase.h"

struct _wlmtk_container_t;
struct _wlmtk_container_vmt_t;
struct wlr_scene_tree;
/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
/** Forward declaration: Container virtual method table. */
typedef struct _wlmtk_container_vmt_t wlmtk_container_vmt_t;

#include "element.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Virtual method table of the container. */
struct _wlmtk_container_vmt_t {
    /**
     * Updates the layout of the container elements.
     *
     * @param container_ptr
     *
     * @return true if there was a change to the layout, eg. if elements got
     * re-positioned.
     */
    bool (*update_layout)(wlmtk_container_t *container_ptr);
};

/** State of the container. */
struct _wlmtk_container_t {
    /** Super class of the container. */
    wlmtk_element_t           super_element;
    /** Virtual method table of the super element before extending it. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Virtual method table for the container. */
    wlmtk_container_vmt_t     vmt;

    /**
     * Elements contained here.
     *
     * `head_ptr` is the topmost element, and `tail_ptr` the bottom-most one.
     */
    bs_dllist_t               elements;

    /** Scene tree. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Listener for the `destroy` signal of `wlr_scene_tree_ptr->node`. */
    struct wl_listener        wlr_scene_tree_node_destroy_listener;

    /** Stores the element with current pointer focus. May be NULL. */
    wlmtk_element_t           *pointer_focus_element_ptr;
    /** Stores the element with current pointer grab. May be NULL. */
    wlmtk_element_t           *pointer_grab_element_ptr;
    /** Stores the element which received WLMTK_BUTTON_DOWN for BTN_LEFT. */
    wlmtk_element_t           *left_button_element_ptr;
    /** Stores the element with current keyboard focus. May be NULL. */
    wlmtk_element_t           *keyboard_focus_element_ptr;

    /** @private inhibitor, to prevent recursive layout updates. */
    bool                      inhibit_layout_update;
};

/**
 * Initializes the container with the provided virtual method table.
 *
 * @param container_ptr
 *
 * @return true on success.
 */
bool wlmtk_container_init(wlmtk_container_t *container_ptr);

/**
 * Extends the container's virtual methods.
 *
 * @param container_ptr
 * @param container_vmt_ptr
 *
 * @return The previous virtual method table.
 */
wlmtk_container_vmt_t wlmtk_container_extend(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_vmt_t *container_vmt_ptr);

/**
 * Initializes the container, and attach to WLR sene graph.
 *
 * @param container_ptr
 * @param root_wlr_scene_tree_ptr
 *
 * @return true on success.
 */
bool wlmtk_container_init_attached(
    wlmtk_container_t *container_ptr,
    struct wlr_scene_tree *root_wlr_scene_tree_ptr);

/**
 * Un-initializes the container.
 *
 * Any element still in `elements` will be destroyed.
 *
 * @param container_ptr
 */
void wlmtk_container_fini(
    wlmtk_container_t *container_ptr);

/**
 * Adds `element_ptr` to the container.
 *
 * Requires that `element_ptr` is not added to a container yet. The element
 * will be added at the top of the container.
 *
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Adds `element_ptr` to the container atop the reference's position.
 *
 * If reference_element_ptr is NULL, the element will be added at the back.
 *
 * @param container_ptr
 * @param reference_element_ptr Must be an element of this container.
 * @param element_ptr
 */
void wlmtk_container_add_element_atop(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *reference_element_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Removes `element_ptr` from the container.
 *
 * Expects that `container_ptr` is `element_ptr`'s parent container. In case
 * `element_ptr` holds a pointer grab, @ref wlmtk_element_pointer_grab_cancel
 * will be called.
 *
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_remove_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Places `element_ptr` at the top (head) of the container.
 *
 * Expects that `container_ptr` is `element_ptr`'s parent container.
 *
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_raise_element_to_top(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Requests that `element_ptr` be given pointer focus from `container_ptr`.
 *
 * @param container_ptr
 * @param element_ptr         Must be in @ref wlmtk_container_t::elements.
 * @param motion_event_ptr    The motion event (in particular: pointer) that
 *                            led to request pointer focus.
 *                            FIXME -> Must change to pointer!
 *
 * @return false if the request fails.
 */
// TODO(kaeser@gubbe.ch): Unify handling with pointer grab.
bool wlmtk_container_request_pointer_focus(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr,
    wlmtk_pointer_motion_event_t *motion_event_ptr);

/**
 * Requests a pointer grab from `container_ptr` for `element_ptr`.
 *
 * Will cancel any existing grab held by elements other than `element_ptr`, and
 * propagates the grab to the parent container.
 * When a pointer grab is held, pointer events will be routed exclusively to
 * the element holding the pointer grab.
 *
 * @param container_ptr
 * @param element_ptr         Must be a child of this container.
 *
 */
// TODO(kaeser@gubbe.ch): Merge with @ref wlmtk_container_request_pointer_focus.
void wlmtk_container_pointer_grab(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Releases a pointer grab in `container_ptr` held by `element_ptr`.
 *
 * If the grab is held by an element other than `element_ptr`, nothing is done.
 * Otherwise, the pointer grab is released, and the release is propagated to
 * the parent container.
 *
 * @param container_ptr
 * @param element_ptr         Must be a child of this container.
 */
void wlmtk_container_pointer_grab_release(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);


/**
 * Sets or disables keyboard focus for `element_ptr` for the container.
 *
 * If `enabled`, calls @ref wlmtk_element_keyboard_blur for the element
 * currently having keyboard focus, and updates
 * @ref wlmtk_container_t::keyboard_focus_element_ptr to `element_ptr`.
 * Otherwise, evaluates whether the focus is currently held by `element_ptr`,
 * and (if yes) clears this container's focus.
 * Propagates the event to the container's parent.
 *
 * @param container_ptr
 * @param element_ptr
 * @param enabled
 */
void wlmtk_container_set_keyboard_focus_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr,
    bool enabled);

/**
 * Updates the layout of the container, and recomputes pointer focus as needed.
 *
 * Must be called if an element is added or removed, or if any of the child
 * elements changes visibility or dimensions. Propagates to the parent
 * container(s).
 *
 * If needed: Trigger a poitner focus computation.
 *
 * @param container_ptr
 */
void wlmtk_container_update_layout_and_pointer_focus(
    wlmtk_container_t *container_ptr);

/**
 * Returns the wlroots scene graph tree for this node.
 *
 * Private: Should be called only by wlmtk_element_t.
 *
 * @param container_ptr
 *
 * @return The scene tree, or NULL if not attached to a scene graph.
 */
struct wlr_scene_tree *wlmtk_container_wlr_scene_tree(
    wlmtk_container_t *container_ptr);

/** Unit tests for the container. */
extern const bs_test_case_t wlmtk_container_test_cases[];

/** Constructor for a fake container with a scene tree. */
wlmtk_container_t *wlmtk_container_create_fake_parent(void);
/** Destructor for that fake container. */
void wlmtk_container_destroy_fake_parent(wlmtk_container_t *container_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_CONTAINER_H__ */
/* == End of container.h =================================================== */
