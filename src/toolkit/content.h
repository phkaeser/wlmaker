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
/** Forward declaration: Fake content, for tests. */
typedef struct _wlmtk_fake_content_t wlmtk_fake_content_t;


#include "window.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Method table of the content. */
struct _wlmtk_content_impl_t {
    /** Destroys the implementation of the content. */
    void (*destroy)(wlmtk_content_t *content_ptr);
    /** Creates content's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_content_t *content_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);
    /** Requests the content to close. */
    void (*request_close)(wlmtk_content_t *content_ptr);
    /** Sets width and height of the content. Returns serial. */
    uint32_t (*request_size)(wlmtk_content_t *content_ptr,
                             int width, int height);
    /** Sets whether the content is activated (has keyboard focus). */
    void (*set_activated)(wlmtk_content_t *content_ptr, bool activated);
};

/** State of the element. */
struct _wlmtk_content_t {
    /** Temporary: Identifier, to disambiguate from XDG nodes. */
    void                      *identifier_ptr;

    /** Super class of the content: An element. */
    wlmtk_element_t           super_element;

    /** Implementation of abstract virtual methods. */
    wlmtk_content_impl_t      impl;

    /**
     * The window this content belongs to. Will be set when creating
     * the window.
     */
    wlmtk_window_t            *window_ptr;

    /** Back-link to the the associated seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /**
     * Surface associated with this content.
     *
     * TODO(kaeser@gubbe.ch): If we extend 'content' to support different
     * elements (eg. buffer), this should be abstracted away.
     */
    struct wlr_surface        *wlr_surface_ptr;

    /** Committed width of the content. See @ref wlmtk_content_commit_size. */
    unsigned                  committed_width;
    /** Committed height of the content. See @ref wlmtk_content_commit_size. */
    unsigned                  committed_height;
};

/**
 * Initializes the content.
 *
 * @param content_ptr
 * @param content_impl_ptr
 * @param wlr_seat_ptr
 *
 * @return true on success.
 */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    const wlmtk_content_impl_t *content_impl_ptr,
    struct wlr_seat *wlr_seat_ptr);

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
 * Sets the committed size of the content.
 *
 * Size operations on Wayland content are (often) asynchronous. The server
 * should call @ref wlmtk_content_request_size, which (as a virtual method)
 * forwards the request to the content (eg. the Wayland client surface). The
 * client then configures it's surface and commits it. The content needs to
 * catch that commit and call @ref wlmtk_content_commit_size accordingly.
 * This will then update the parent container's (and window's) layout.
 *
 * @param content_ptr
 * @param serial
 * @param width
 * @param height
 */
void wlmtk_content_commit_size(
    wlmtk_content_t *content_ptr,
    uint32_t serial,
    unsigned width,
    unsigned height);

/**
 * Returns committed size of the content.
 *
 * @param content_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr, int *height_ptr);

/**
 * Returns the super Element of the content.
 *
 * @param content_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     content_ptr.
 */
wlmtk_element_t *wlmtk_content_element(wlmtk_content_t *content_ptr);

/** Wraps to @ref wlmtk_content_impl_t::destroy. */
static inline void wlmtk_content_destroy(wlmtk_content_t *content_ptr) {
    content_ptr->impl.destroy(content_ptr);
}
/** Wraps to @ref wlmtk_content_impl_t::request_close. */
static inline void wlmtk_content_request_close(wlmtk_content_t *content_ptr) {
    content_ptr->impl.request_close(content_ptr);
}
/** Wraps to @ref wlmtk_content_impl_t::request_size. */
static inline uint32_t wlmtk_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height) {
    return content_ptr->impl.request_size(content_ptr, width, height);
}
/** Wraps to @ref wlmtk_content_impl_t::set_activated. */
static inline void wlmtk_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated) {
    content_ptr->impl.set_activated(content_ptr, activated);
}

/**
 * Identifying pointer: Value unique to wlmtk_content.
 *
 * TODO(kaeser@gubbe.ch): Remove, once migrated to toolkit.
 */
extern void *wlmtk_content_identifier_ptr;

/** Unit tests for content. */
extern const bs_test_case_t wlmtk_content_test_cases[];

/** Fake content, useful for unit test. */
struct _wlmtk_fake_content_t {
    /** State of the content. */
    wlmtk_content_t           content;
    /** Whether @ref wlmtk_content_request_close was called. */
    bool                      request_close_called;
    /** `width` argument eof last @ref wlmtk_content_request_size call. */
    int                       requested_width;
    /** `height` argument of last @ref wlmtk_content_request_size call. */
    int                       requested_height;
    /** Return value of @ref wlmtk_content_request_size call. */
    uint32_t                  return_request_size;
    /** Argument of last @ref wlmtk_content_set_activated call. */
    bool                      activated;
};

/** Ctor for a fake content. */
wlmtk_fake_content_t *wlmtk_fake_content_create(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_CONTENT_H__ */
/* == End of content.h ===================================================== */
