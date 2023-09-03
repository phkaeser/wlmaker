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
#include <wayland-server.h>

/** Forward declaration: Element. */
typedef struct _wlmtk_element_t wlmtk_element_t;
/** Forward declaration: Element virtual method implementations. */
typedef struct _wlmtk_element_impl_t wlmtk_element_impl_t;

/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
struct wlr_scene_tree;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of an element. */
struct _wlmtk_element_t {
    /** X position of the element, relative to the container. */
    int x;
    /** Y position of the element, relative to the container. */
    int y;
    /** Width of the element, in pixels. */
    int width;
    /** Height of the element, in pixels. */
    int height;

    /** The container this element belongs to, if any. */
    wlmtk_container_t         *parent_container_ptr;
    /** The node of elements. */
    bs_dllist_node_t          dlnode;

    /** Implementation of abstract virtual methods. */
    const wlmtk_element_impl_t *impl_ptr;

    /** Points to the wlroots scene graph API node, if attached. */
    struct wlr_scene_node     *wlr_scene_node_ptr;

    /** Whether the element is visible (drawn, when part of a scene graph). */
    bool                      visible;

    /** Listener for the `destroy` signal of `wlr_scene_node_ptr`. */
    struct wl_listener        wlr_scene_node_destroy_listener;
};

/** Pointers to the implementation of Element's virtual methods. */
struct _wlmtk_element_impl_t {
    /** Destroys the implementation of the element. */
    void (*destroy)(wlmtk_element_t *element_ptr);
    /** Creates element's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_element_t *element_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);

    /** Indicates pointer motion into or within the element area to (x,y). */
    void (*motion)(wlmtk_element_t *element_ptr,
                   double x, double y);
    /** Indicates the pointer has left the element's area. */
    void (*leave)(wlmtk_element_t *element_ptr);
};

/**
 * Initializes the element.
 *
 * @param element_ptr
 * @param element_impl_ptr
 *
 * @return true on success.
 */
bool wlmtk_element_init(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_impl_t *element_impl_ptr);

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
 * or invoke wlmtk_element_impl_t::create_scene_node to create and attach a
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
 * Returns the size of the element, in pixels.
 *
 * @param element_ptr
 * @param width_ptr           Optional, may be NULL.
 * @param height_ptr          Optional, may be NULL.
 */
void wlmtk_element_get_size(
    wlmtk_element_t *element_ptr,
    int *width_ptr,
    int *height_ptr);

/** Virtual method: Calls 'motion' for the element's implementation. */
static inline void wlmtk_element_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y) {
    if (NULL != element_ptr->impl_ptr->motion) {
        element_ptr->impl_ptr->motion(element_ptr, x, y);
    }
}

/** Virtual method: Calls 'leave' for the element's implementation. */
static inline void wlmtk_element_leave(
    wlmtk_element_t *element_ptr)
{
    if (NULL != element_ptr->impl_ptr->leave) {
        element_ptr->impl_ptr->leave(element_ptr);
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
    element_ptr->impl_ptr->destroy(element_ptr);
}

/** Unit tests for the element. */
extern const bs_test_case_t wlmtk_element_test_cases[];

/** Fake element, useful for unit test. */
typedef struct {
    /** State of the element. */
    wlmtk_element_t           element;

    /** Indicates that Element::motion() was called. */
    bool                      motion_called;
    /** The x argument of the last Element::motion() call. */
    double                    motion_x;
    /** The y arguemnt of the last element::motion() call. */
    double                    motion_y;

    /** Indicates that Element::leave() was called. */
    bool                      leave_called;
} wlmtk_fake_element_t;

/** Ctor for the fake element. */
wlmtk_fake_element_t *wlmtk_fake_element_create(void);

/** Implementation table of a "fake" element for tests. */
extern const wlmtk_element_impl_t wlmtk_fake_element_impl;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ELEMENT_H__ */
/* == End of element.h ===================================================== */
