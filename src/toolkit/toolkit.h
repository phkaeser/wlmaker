/* ========================================================================= */
/**
 * @file toolkit.h
 *
 * See @ref toolkit_page for documentation.
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
#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include "gfxbuf.h"
#include "primitives.h"
#include "style.h"
#include "util.h"

#include <libbase/libbase.h>
#include <wayland-server.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlr_scene_tree;

/** Forward declaration: Element. */
typedef struct _wlmtk_element_t wlmtk_element_t;
/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t;
/** Forward declaration: Window content. */
typedef struct _wlmtk_content_t wlmtk_content_t;

/** Forward declaration: Element virtual method implementations. */
typedef struct _wlmtk_element_impl_t wlmtk_element_impl_t;
/** Forward declaration: Container virtual method implementations. */
typedef struct _wlmtk_container_impl_t wlmtk_container_impl_t;
/** Forward declaration: Content virtual method implementations. */
typedef struct _wlmtk_content_impl_t wlmtk_content_impl_t;

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

    /** Notifies that the pointer is within the element's area, at x,y. */
    void (*motion)(wlmtk_element_t *element_ptr,
                   double x, double y);
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
} wlmtk_fake_element_t;

/** Ctor for the fake element. */
wlmtk_fake_element_t *wlmtk_fake_element_create(void);

/** Implementation table of a "fake" element for tests. */
extern const wlmtk_element_impl_t wlmtk_fake_element_impl;

/* ========================================================================= */

/** State of the container. */
struct _wlmtk_container_t {
    /** Super class of the container. */
    wlmtk_element_t           super_element;

    /** Elements contained here. */
    bs_dllist_t               elements;

    /** Implementation of the container's virtual methods. */
    const wlmtk_container_impl_t *impl_ptr;

    /** Scene tree. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Listener for the `destroy` signal of `wlr_scene_tree_ptr->node`. */
    struct wl_listener        wlr_scene_tree_node_destroy_listener;
};

/** Virtual method table of the container. */
struct _wlmtk_container_impl_t {
    /** dtor. */
    void (*destroy)(wlmtk_container_t *container_ptr);
};

/**
 * Initializes the container with the provided virtual method table.
 *
 * @param container_ptr
 * @param container_impl_ptr
 *
 * @return true on success.
 */
bool wlmtk_container_init(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_impl_t *container_impl_ptr);

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
 * Requires that `element_ptr` is not added to a container yet.
 *
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
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

/** Virtual method: Calls the dtor of the container's implementation. */
static inline void wlmtk_container_destroy(
    wlmtk_container_t *container_ptr) {
    container_ptr->impl_ptr->destroy(container_ptr);
}

/** Unit tests for the container. */
extern const bs_test_case_t wlmtk_container_test_cases[];

/** Implementation table of a "fake" container for tests. */
extern const wlmtk_container_impl_t wlmtk_container_fake_impl;

/** Constructor for a fake container with a scene tree. */
wlmtk_container_t *wlmtk_container_create_fake_parent(void);

/* ========================================================================= */

/** State of the workspace. */
typedef struct _wlmtk_workspace_t wlmtk_workspace_t;

/**
 * Creates a workspace.
 *
 * TODO(kaeser@gubbe.ch): Consider replacing the interface with a container,
 * and permit a "toplevel" container that will be at the server level.
 *
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the workspace state, or NULL on error. Must be free'd
 *     via @ref wlmtk_workspace_destroy.
 */
wlmtk_workspace_t *wlmtk_workspace_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/**
 * Destroys the workspace. Will destroy any stil-contained element.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr);

/**
 * Maps the window: Adds it to the workspace container and makes it visible.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_map_window(wlmtk_workspace_t *workspace_ptr,
                                wlmtk_window_t *window_ptr);

/**
 * Unmaps the window: Sets it as invisible and removes it from the container.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr);

/**
 * Type conversion: Returns the @ref wlmtk_workspace_t from the container_ptr
 * pointing to wlmtk_workspace_t::super_container.
 *
 * Asserts that the container is indeed from a wlmtk_workspace_t.
 *
 * @return Pointer to the workspace.
 */
wlmtk_workspace_t *wlmtk_workspace_from_container(
    wlmtk_container_t *container_ptr);

/** Unit tests for the workspace. */
extern const bs_test_case_t wlmtk_workspace_test_cases[];

/* ========================================================================= */

/**
 * Creates a window for the given content.
 *
 * @param content_ptr
 *
 * @return Pointer to the window state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_window_destroy.
 */
wlmtk_window_t *wlmtk_window_create(wlmtk_content_t *content_ptr);

/**
 * Destroys the window.
 *
 * @param window_ptr
 */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr);

/**
 * Returns the super Element of the window.
 *
 * TODO(kaeser@gubbe.ch): Re-evaluate whether to work with accessors, or
 *     whether to keep the ancestry members public.
 *
 * @param window_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     window_ptr.
 */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr);

/** Unit tests for window. */
extern const bs_test_case_t wlmtk_window_test_cases[];

/* ========================================================================= */

/** State of the element. */
struct _wlmtk_content_t {
    /** Super class of the content: An element. */
    wlmtk_element_t           super_element;

    /** Implementation of abstract virtual methods. */
    const wlmtk_content_impl_t *impl_ptr;

    /**
     * The window this content belongs to. Will be set when creating
     * the window.
     */
    wlmtk_window_t            *window_ptr;
};

/** Method table of the content. */
struct _wlmtk_content_impl_t {
    /** Destroys the implementation of the content. */
    void (*destroy)(wlmtk_content_t *content_ptr);
    /** Creates content's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_content_t *content_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);
};

/**
 * Initializes the content.
 *
 * @param content_ptr
 * @param content_impl_ptr
 *
 * @return true on success.
 */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    const wlmtk_content_impl_t *content_impl_ptr);

/**
 * Cleans up the content.
 *
 * @param content_ptr
 */
void wlmtk_content_fini(wlmtk_content_t *content_ptr);

/**
 * Sets the window for the content.
 *
 * Private: Should only be called by Window ctor (a friend).
 *
 * @param content_ptr
 * @param window_ptr
 */
void wlmtk_content_set_window(
    wlmtk_content_t *content_ptr,
    wlmtk_window_t *window_ptr);

/**
 * Returns the super Element of the content.
 *
 * @param content_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     content_ptr.
 */
wlmtk_element_t *wlmtk_content_element(wlmtk_content_t *content_ptr);

/** Unit tests for content. */
extern const bs_test_case_t wlmtk_content_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TOOLKIT_H__ */
/* == End of toolkit.h ===================================================== */
