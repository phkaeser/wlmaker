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

#include <libbase/libbase.h>
#include <wayland-server.h>

/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
/** Forward declaration: Container virtual method table. */
typedef struct _wlmtk_container_vmt_t wlmtk_container_vmt_t;

#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Virtual method table of the container. */
struct _wlmtk_container_vmt_t {
    /**
     * Updates the layout of the container elements.
     *
     * Will be called by this, when an element is added or removed.
     * Additionally, this should be invoked by contained elements when
     * the visibility or dimensions change.
     *
     * Each container will propagate a
     * @ref wlmtk_container_vmt_t::update_layout call
     * upwards to it's parent container. The root container will then trigger
     * an update to pointer focus (since by then, the layout is updated).
      */
    void (*update_layout)(wlmtk_container_t *container_ptr);
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
    /** Stores the element which received WLMTK_BUTTON_DOWN for BTN_LEFT. */
    wlmtk_element_t           *left_button_element_ptr;
    /** Stores the element with current keyboard focus. May be NULL. */
    wlmtk_element_t           *keyboard_focus_element_ptr;
};

/**
 * Initializes the container with the provided virtual method table.
 *
 * @param container_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_container_init(wlmtk_container_t *container_ptr,
                          wlmtk_env_t *env_ptr);

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
 * @param env_ptr
 * @param root_wlr_scene_tree_ptr
 *
 * @return true on success.
 */
bool wlmtk_container_init_attached(
    wlmtk_container_t *container_ptr,
    wlmtk_env_t *env_ptr,
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
 * Expects that `container_ptr` is `element_ptr`'s parent container.
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
 * Updates pointer focus of the container.
 *
 * @param container_ptr
 */
void wlmtk_container_update_pointer_focus(wlmtk_container_t *container_ptr);

/**
 * Updates the container's keyboard focus to be at `element_ptr`.
 *
 * @param container_ptr
 * @param element_ptr         The element that will get keyboard focus. Must
 *                            be contained in @ref wlmtk_container_t::elements.
 *                            A value of NULL clears the focus.
 */
void wlmtk_container_update_keyboard_focus(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Updates the layout of the container.
 *
 * @param container_ptr       Container to update. NULL implies a no-op.
 */
static inline void wlmtk_container_update_layout(
    wlmtk_container_t *container_ptr)
{
    container_ptr->vmt.update_layout(container_ptr);
}

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
