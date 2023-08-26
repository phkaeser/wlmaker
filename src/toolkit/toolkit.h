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

#include <libbase/libbase.h>

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

    /** The container this element belongs to, if any. */
    wlmtk_container_t         *parent_container_ptr;
    /** The node of elements. */
    bs_dllist_node_t          dlnode;

    /** Implementation of abstract virtual methods. */
    const wlmtk_element_impl_t *impl_ptr;

    /** Points to the wlroots scene graph API node. Is set when mapped. */
    struct wlr_scene_node     *wlr_scene_node_ptr;
};

/** Pointers to the implementation of Element's virtual methods. */
struct _wlmtk_element_impl_t {
    /** Destroys the implementation of the element. */
    void (*destroy)(wlmtk_element_t *element_ptr);
    /** Creates element's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_element_t *element_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);
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
 * Private: Should only be called by wlmtk_container_add_element, respectively
 * wlmtk_container_remove_element ("friends"). Will unmap the element, if the
 * parent container changes.
 *
 * @param element_ptr
 * @param parent_container_ptr Pointer to the parent container, or NULL if
 *     the parent should be cleared.
 */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr);

/**
 * Maps the element.
 *
 * Requires a parent container to be set. Will call `create_scene_node` to
 * build the scene graph API node attached to the parent container's tree.
 *
 * @param element_ptr
 */
void wlmtk_element_map(wlmtk_element_t *element_ptr);

/**
 * Unmaps the element.
 *
 * @param element_ptr
 */
void wlmtk_element_unmap(wlmtk_element_t *element_ptr);

/** Returns whether this element is currently mapped. */
static inline bool wlmtk_element_mapped(wlmtk_element_t *element_ptr)
{
    return NULL != element_ptr->wlr_scene_node_ptr;
}

/** Virtual method: Calls the dtor of the element's implementation. */
static inline void wlmtk_element_destroy(
    wlmtk_element_t *element_ptr) {
    element_ptr->impl_ptr->destroy(element_ptr);
}

/** Unit tests for the element. */
extern const bs_test_case_t wlmtk_element_test_cases[];

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
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Removes `element_ptr` from the container.
 *
 * Expects that `container_ptr` is `element_ptr`'s parent container. Will unmap
 * the element, in case it is currently mapped.
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
 * Requires this container's element to be mapped. Should be called only from
 * members of `elements`.
 *
 * @param container_ptr
 *
 * @return The scene tree.
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

/* ========================================================================= */

/** State of the workspace. */
typedef struct _wlmtk_workspace_t wlmtk_workspace_t;

/**
 * Creates a workspace.
 *
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the workspace state, or NULL on error. Must be free'd
 *     via @ref wlmtk_workspace_destroy.
 */
wlmtk_workspace_t *wlmtk_workspace_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr);

/**
 * Destroys the workspace. Will destroy any still-mapped element.
 *
 * @param workspace_ptr
 */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr);

/**
 * Maps the window: Adds it to the workspace container and maps it.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_map_window(wlmtk_workspace_t *workspace_ptr,
                                wlmtk_window_t *window_ptr);

/**
 * Maps the window: Unmaps the window and removes it from the workspace
 * container.
 *
 * @param workspace_ptr
 * @param window_ptr
 */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr);

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
