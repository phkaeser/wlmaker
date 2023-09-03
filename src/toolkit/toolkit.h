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

/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t;
/** Forward declaration: Window content. */
typedef struct _wlmtk_content_t wlmtk_content_t;

/** Forward declaration: Content virtual method implementations. */
typedef struct _wlmtk_content_impl_t wlmtk_content_impl_t;

#include "element.h"
#include "container.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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
