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

/** Forward declaration: Fake content, for tests. */
typedef struct _wlmtk_fake_content_t wlmtk_fake_content_t;

#include "container.h"
#include "popup.h"
#include "surface.h"
#include "util.h"

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

    /**
     * Requests the content to change to the specified size.
     *
     * This may be implemented as an asynchronous implementation. Once the
     * content has committed the adapted size, @ref wlmtk_content_commit should
     * be called with the corresponding serial.
     *
     * @param content_ptr
     * @param width
     * @param height
     *
     * @return XDG toplevel configuration serial.
     */
    uint32_t (*request_size)(wlmtk_content_t *content_ptr,
                             int width,
                             int height);

    /**
     * Requests the content to close.
     *
     * @param content_ptr
     */
    void (*request_close)(wlmtk_content_t *content_ptr);

    /**
     * Sets whether this content as activated (keyboard focus).
     *
     * The implementation must (for the effective contained element) issue a
     * call to  @ref wlmtk_container_set_keyboard_focus_element to claim or
     * release keyboard focus.
     *
     * @param content_ptr
     * @param activated
     */
    void (*set_activated)(wlmtk_content_t *content_ptr, bool activated);
};

/** State of window content. */
struct _wlmtk_content_t {
    /** Super class of the content: A container, holding element & popups. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the content. */
    wlmtk_content_vmt_t       vmt;

    /** Virtual method table of the super element before extending it. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** And the popup container. Contents can contain popups. */
    // TODO(kaeser@gubbe.ch): Re-think whether this better be part of window?
    // To consider: positioning relative to window's content *is* desirable.
    wlmtk_container_t         popup_container;

    /** The principal element of the content. */
    wlmtk_element_t           *element_ptr;
    /** The window this content belongs to. Set when creating the window. */
    wlmtk_window_t            *window_ptr;

    /** The client connected to the @ref wlmtk_content_t. */
    // TODO(kaeser@gubbe.ch): Should not be stored here & this way.
    wlmtk_util_client_t       client;

    /**
     * The parent content, or NULL. Set in @ref wlmtk_content_add_popup,
     * respectively in @ref wlmtk_content_remove_popup.
     */
    wlmtk_content_t           *parent_content_ptr;

    /** Committed width of the content. See @ref wlmtk_content_commit. */
    int                       committed_width;
    /** Committed height of the content. See @ref wlmtk_content_commit. */
    int                       committed_height;

    /** Set of registered popup contents. See @ref wlmtk_content_add_popup. */
    bs_dllist_t               popups;
    /** Connects to the parent's @ref wlmtk_content_t::popups, if a popup. */
    bs_dllist_node_t          dlnode;
};

/**
 * Initializes the content with the given element.
 *
 * @param content_ptr
 * @param element_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_content_init(
    wlmtk_content_t *content_ptr,
    wlmtk_element_t *element_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the content.
 *
 * @param content_ptr
 */
void wlmtk_content_fini(
    wlmtk_content_t *content_ptr);

/**
 * Sets or clears the content's element.
 *
 * @param content_ptr
 * @param element_ptr         Element to set for the content, or NULL.
 */
void wlmtk_content_set_element(
    wlmtk_content_t *content_ptr,
    wlmtk_element_t *element_ptr);

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

/** Requests new size. See @ref wlmtk_content_vmt_t::request_size. */
static inline uint32_t wlmtk_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height) {
    return content_ptr->vmt.request_size(content_ptr, width, height);
}

/** Requests close. See @ref wlmtk_content_vmt_t::request_close. */
static inline void wlmtk_content_request_close(wlmtk_content_t *content_ptr) {
    if (NULL == content_ptr->vmt.request_close) return;
    return content_ptr->vmt.request_close(content_ptr);
}

/** Requests activation. See @ref wlmtk_content_vmt_t::set_activated. */
static inline void wlmtk_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated) {
    if (NULL == content_ptr->vmt.set_activated) return;
    content_ptr->vmt.set_activated(content_ptr, activated);
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

/** Gets size: Returns size from earlier @ref wlmtk_content_commit. */
void wlmtk_content_get_size(
    wlmtk_content_t *content_ptr,
    int *width_ptr,
    int *height_ptr);

/** Commits size and serial: Calls into @ref wlmtk_window_serial. */
void wlmtk_content_commit(
    wlmtk_content_t *content_ptr,
    int width,
    int height,
    uint32_t serial);

/** Returns the superclass' instance of @ref wlmtk_element_t. */
wlmtk_element_t *wlmtk_content_element(wlmtk_content_t *content_ptr);

/**
 * Adds a popup to the content.
 *
 * @param content_ptr
 * @param popup_content_ptr
 */
void wlmtk_content_add_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_content_t *popup_content_ptr);

/**
 * Removes a popup from the content.
 *
 * `popup_content_ptr` must have previously been added to `content_ptr`.
 *
 * @param content_ptr
 * @param popup_content_ptr
 */
void wlmtk_content_remove_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_content_t *popup_content_ptr);

/**
 * Adds a popup to the content.
 *
 * @param content_ptr
 * @param popup_ptr
 */
void wlmtk_content_add_wlmtk_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_popup_t *popup_ptr);

/**
 * Removes a popup from the content.
 *
 * `popup_ptr` must have previously been added to `content_ptr`.
 *
 * @param content_ptr
 * @param popup_ptr
 */
void wlmtk_content_remove_wlmtk_popup(
    wlmtk_content_t *content_ptr,
    wlmtk_popup_t *popup_ptr);

/** @return A pointer to the parent content, or NULL if none. */
wlmtk_content_t *wlmtk_content_get_parent_content(
    wlmtk_content_t *content_ptr);

/** Content's unit tests. */
extern const bs_test_case_t wlmtk_content_test_cases[];

/** Fake content, useful for unit tests. */
struct _wlmtk_fake_content_t {
    /** Superclass: content. */
    wlmtk_content_t           content;
    /** Fake surface, the argument to @ref wlmtk_fake_content_create. */
    wlmtk_fake_surface_t      *fake_surface_ptr;

    /** Reports whether @ref wlmtk_content_request_close was called. */
    bool                      request_close_called;

    /** Serial to return on next request_size call. */
    uint32_t                  serial;
    /** `width` argument eof last @ref wlmtk_content_request_size call. */
    int                       requested_width;
    /** `height` argument of last @ref wlmtk_content_request_size call. */
    int                       requested_height;
    /** Last call to @ref wlmtk_content_set_activated. */
    bool                      activated;
};

/** Creates a fake content, for tests. */
wlmtk_fake_content_t *wlmtk_fake_content_create(
    wlmtk_fake_surface_t *fake_surface_ptr);
/** Destroys the fake content. */
void wlmtk_fake_content_destroy(wlmtk_fake_content_t *fake_content_ptr);
/** Commits the state of last @ref wlmtk_content_request_size call. */
void wlmtk_fake_content_commit(wlmtk_fake_content_t *fake_content_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_CONTENT_H__ */
/* == End of content.h ===================================================== */
