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


/** Forward declaration: Content state. */
typedef struct _wlmtk_content_t wlmtk_content_t;
/** Forward declaration: Content virtual method table. */
typedef struct _wlmtk_content_vmt_t wlmtk_content_vmt_t;
/** Forward declaration: Window. */
typedef struct _wlmtk_window_t wlmtk_window_t
;/** Forward declaration: State of a toolkit's WLR surface. */
typedef struct _wlmtk_surface_t wlmtk_surface_t;

#include "container.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Virtual method table of @ref wlmtk_content_t. */
struct _wlmtk_content_vmt_t {
    /**
     * Requests the content to be set to maximized mode.
     *
     * Once the content has changed to `maximized` mode (potentially an
     * asynchronous operation), @ref wlmtk_window_commit_maximized ought to be
     * called, if the content belongs to a window.
     *
     * @param content_ptr
     * @param maximized
     *
     * @return XDG toplevel configuration serial.
     */
    uint32_t (*request_maximized)(wlmtk_content_t *content_ptr,
                                  bool maximized);

    /**
     * Requests the content to be set to fullscreen mode.
     *
     * Some contents may adjust the decoration suitably. Once the content has
     * changed to fullscreen mode (potentially an asynchronous operation),
     * @ref wlmtk_window_commit_fullscreen ought to be called, if the content
     * belongs to a window.
     *
     * @param content_ptr
     * @param fullscreen
     *
     * @return XDG toplevel configuration serial.
     */
    uint32_t (*request_fullscreen)(wlmtk_content_t *content_ptr,
                                   bool fullscreen);
};

/** State of window content. */
struct _wlmtk_content_t {
    /** Temporary: Identifier, to disambiguate from XDG nodes. */
    void                      *identifier_ptr;

    /** Super class of the content: A container, holding surface & popups. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the content. */
    wlmtk_content_vmt_t       vmt;

    /** The principal surface of the content. */
    wlmtk_surface_t           *surface_ptr;
    /** The window this content belongs to. Set when creating the window. */
    wlmtk_window_t            *window_ptr;
};

/**
 * Identifying pointer: Value unique to wlmtk_content.
 *
 * TODO(kaeser@gubbe.ch): Remove, once migrated to toolkit.
 */
extern void *wlmtk_content_identifier_ptr;

/**
 * Initializes the content with the given surface.
 *
 * @param content_ptr
 * @param surface_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    wlmtk_surface_t *surface_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the content.
 *
 * @param content_ptr
 */
void wlmtk_content_fini(
    wlmtk_content_t *content_ptr);

/**
 * Extends the content by specifying virtual methods.
 *
 * @param content_ptr
 * @param content_vmt_ptr
 *
 * @return The original virtual method table.
 */
wlmtk_content_vmt_t wlmtk_content_extend(
    wlmtk_content_t *content_ptr,
    const wlmtk_content_vmt_t *content_vmt_ptr);

/** Requests size: Forwards to @ref wlmtk_surface_request_size. */
uint32_t wlmtk_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);

/** Requests maximized. See @ref wlmtk_content_vmt_t::request_maximized. */
static inline uint32_t wlmtk_content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized) {
    if (NULL == content_ptr->vmt.request_maximized) return 0;
    return content_ptr->vmt.request_maximized(content_ptr, maximized);
}

/** Requests fullscreen. See @ref wlmtk_content_vmt_t::request_fullscreen. */
static inline uint32_t wlmtk_content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen) {
    if (NULL == content_ptr->vmt.request_fullscreen) return 0;
    return content_ptr->vmt.request_fullscreen(content_ptr, fullscreen);
}

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

/** Requests close: Forwards to @ref wlmtk_surface_request_close. */
void wlmtk_content_request_close(wlmtk_content_t *content_ptr);

/** Set activated: Forwards to @ref wlmtk_surface_set_activated. */
void wlmtk_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

/** Gets size: Forwards to @ref wlmtk_surface_get_size. */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr);

/** Commits size: Forwards to @ref wlmtk_surface_commit_size. */
void wlmtk_content_commit_size(
    wlmtk_content_t *content_ptr,
    uint32_t serial,
    int width,
    int height);

/** Returns the superclass' instance of @ref wlmtk_element_t. */
wlmtk_element_t *wlmtk_content_element(wlmtk_content_t *content_ptr);

/** Content's unit tests. */
extern const bs_test_case_t wlmtk_content_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_CONTENT_H__ */
/* == End of content.h ===================================================== */
