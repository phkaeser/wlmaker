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

/** Forward declaration: Element. */
typedef struct _wlmtk_element_t wlmtk_element_t;
/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;

/** Forward declaration: Element virtual method implementations. */
typedef struct _wlmtk_element_impl_t wlmtk_element_impl_t;
/** Forward declaration: Container virtual method implementations. */
typedef struct _wlmtk_container_impl_t wlmtk_container_impl_t;

/** State of an element. */
struct _wlmtk_element_t {
    /** X position of the element, relative to the container. */
    int x;
    /** Y position of the element, relative to the container. */
    int y;

    /** wlroots scene graph API node. Only set when mapped. */
    struct wlr_scene_node_t   *wlr_scene_node_ptr;
    /** The container this element belongs to, if any. */
    wlmtk_container_t         *container_ptr;
    /** The node of elements. */
    bs_dllist_node_t          dlnode;

    /** Implementation of abstract virtual methods. */
    const wlmtk_element_impl_t *impl_ptr;
};

/** Pointers to the implementation of Element's virtual methods. */
struct _wlmtk_element_impl_t {
    /** Destroys the implementation of the element. */
    void (*destroy)(wlmtk_element_t *element_ptr);
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
 * Should only be called by wlmtk_container_add_element, respectively
 * wlmtk_container_remove_element.
 *
 * @param element_ptr
 * @param parent_container_ptr Pointer to the parent contqainer, or NULL if
 *     the parent should be cleared.
 */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr);

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
 * @param container_ptr
 * @param element_ptr
 */
void wlmtk_container_remove_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr);

/** Virtual method: Calls the dtor of the container's implementation. */
static inline void wlmtk_container_destroy(
    wlmtk_container_t *container_ptr) {
    container_ptr->impl_ptr->destroy(container_ptr);
}


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TOOLKIT_H__ */
/* == End of toolkit.h ===================================================== */
