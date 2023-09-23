/* ========================================================================= */
/**
 * @file content.h
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
#ifndef __WLMTK_CONTENT_H__
#define __WLMTK_CONTENT_H__

/** Forward declaration: Window content. */
typedef struct _wlmtk_content_t wlmtk_content_t;

/** Forward declaration: Content virtual method implementations. */
typedef struct _wlmtk_content_impl_t wlmtk_content_impl_t;

#include "window.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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
    /** Gets width and height of the content. */
    void (*get_size)(wlmtk_content_t *content_ptr,
                     int *width_ptr, int *height_ptr);
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

/** Fake content, useful for unit test. */
typedef struct {
    /** State of the content. */
    wlmtk_content_t           content;
    /** Width to return on a wlmtk_content_impl_t::get_size call. */
    int                       width;
    /** Height to return on a wlmtk_content_impl_t::get_size call. */
    int                       height;
} wlmtk_fake_content_t;

/** Ctor for a fake content. */
wlmtk_fake_content_t *wlmtk_fake_content_create(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_CONTENT_H__ */
/* == End of content.h ===================================================== */
