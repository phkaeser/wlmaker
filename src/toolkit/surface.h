/* ========================================================================= */
/**
 * @file surface.h
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
#ifndef __WLMTK_SURFACE_H__
#define __WLMTK_SURFACE_H__

#include <libbase/libbase.h>

/** Forward declaration: State of a toolkit's WLR surface. */
typedef struct _wlmtk_surface_t wlmtk_surface_t;
/** Forward declaration: Virtual method table of the toolkit's WLR surface. */
typedef struct _wlmtk_surface_vmt_t wlmtk_surface_vmt_t;
/** Forward declaration: State of fake surface, for tests. */
typedef struct _wlmtk_fake_surface_t wlmtk_fake_surface_t;

#include "element.h"
#include "env.h"
#include "window.h"

/** Forward declaration. */
struct wlr_surface;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Virtual method table of the surface. */
struct _wlmtk_surface_vmt_t {
    /** Abstract: Requests width and height of the surface. Returns serial. */
    uint32_t (*request_size)(wlmtk_surface_t *surface_ptr,
                             int width,
                             int height);
    /** Abstract: Requests the surface to close. */
    void (*request_close)(wlmtk_surface_t *surface_ptr);
    /** Abstract: Sets whether the surface is activated (keyboard focus). */
    void (*set_activated)(wlmtk_surface_t *surface_ptr, bool activated);
};

/** State of a `struct wlr_surface`, encapsuled for toolkit. */
struct _wlmtk_surface_t {
    /** Super class of the surface: An element. */
    wlmtk_element_t           super_element;
    /** Virtual method table of the super element before extending it. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** The surface's virtual method table. */
    wlmtk_surface_vmt_t       vmt;

    /** The `struct wlr_surface` wrapped. */
    struct wlr_surface        *wlr_surface_ptr;

    /** Committed width of the surface, in pixels. */
    int                       committed_width;
    /** Committed height of the surface, in pixels. */
    int                       committed_height;
};

/**
 * Initializes the surface.
 *
 * @param surface_ptr
 * @param wlr_surface_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_surface_init(
    wlmtk_surface_t *surface_ptr,
    struct wlr_surface *wlr_surface_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the surface.
 *
 * @param surface_ptr
 */
void wlmtk_surface_fini(wlmtk_surface_t *surface_ptr);

/**
 * Extends the surface's virtual methods.
 *
 * @param surface_ptr
 * @param surface_vmt_ptr
 *
 * @return The earlier virtual method table.
 */
wlmtk_surface_vmt_t wlmtk_surface_extend(
    wlmtk_surface_t *surface_ptr,
    const wlmtk_surface_vmt_t *surface_vmt_ptr);

/**
 * Returns a pointer to the surface's element superclass instance.
 *
 * @param surface_ptr
 *
 * @return Pointer to the corresponding @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_surface_element(wlmtk_surface_t *surface_ptr);

/**
 * Virtual method: Request a new size and height for the surface.
 *
 * Wraps to @ref wlmtk_surface_vmt_t::request_size.
 *
 * @param surface_ptr
 * @param width
 * @param height
 */
static inline uint32_t wlmtk_surface_request_size(
    wlmtk_surface_t *surface_ptr,
    int width,
    int height)
{
    return surface_ptr->vmt.request_size(surface_ptr, width, height);
}

/** Wraps to @ref wlmtk_surface_vmt_t::request_close. */
static inline void wlmtk_surface_request_close(wlmtk_surface_t *surface_ptr)
{
    surface_ptr->vmt.request_close(surface_ptr);
}

/** Wraps to @ref wlmtk_surface_vmt_t::set_activated. */
static inline void wlmtk_surface_set_activated(
    wlmtk_surface_t *surface_ptr,
    bool activated)
{
    if (NULL == surface_ptr->vmt.set_activated) return;
    surface_ptr->vmt.set_activated(surface_ptr, activated);
}

/**
 * Returns committed size of the surface.
 *
 * @param surface_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmtk_surface_get_size(
    wlmtk_surface_t *surface_ptr,
    int *width_ptr,
    int *height_ptr);

/**
 * Commits the given dimensions for the surface.
 *
 * @param surface_ptr
 * @param serial
 * @param width
 * @param height
 */
void wlmtk_surface_commit_size(
    wlmtk_surface_t *surface_ptr,
    uint32_t serial,
    int width,
    int height);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_surface_test_cases[];

/** Fake surface, useful for unit test. */
struct _wlmtk_fake_surface_t {
    /** Superclass: surface. */
    wlmtk_surface_t           surface;

    /** Serial to return on next request_size call. */
    uint32_t                  serial;
    /** `width` argument eof last @ref wlmtk_surface_request_size call. */
    int                       requested_width;
    /** `height` argument of last @ref wlmtk_surface_request_size call. */
    int                       requested_height;

    /** Whether @ref wlmtk_surface_request_close was called. */
    bool                      request_close_called;
    /** Argument of last @ref wlmtk_surface_set_activated call. */
    bool                      activated;
};

/** Ctor for the fake surface.*/
wlmtk_fake_surface_t *wlmtk_fake_surface_create(void);

/** Dtor for the fake surface.*/
void wlmtk_fake_surface_destroy(wlmtk_fake_surface_t *fake_surface_ptr);

/** Commits the earlier @ref wlmtk_surface_request_size data. */
void wlmtk_fake_surface_commit(wlmtk_fake_surface_t *fake_surface_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SURFACE_H__ */
/* == End of surface.h ===================================================== */
