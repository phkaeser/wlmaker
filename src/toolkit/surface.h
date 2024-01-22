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
/** Forward declaration. */
struct wlr_scene_surface;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of a `struct wlr_surface`, encapsuled for toolkit. */
struct _wlmtk_surface_t {
    /** Super class of the surface: An element. */
    wlmtk_element_t           super_element;
    /** Virtual method table of the super element before extending it. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** Toolkit environment. See @ref wlmtk_surface_create. */
    wlmtk_env_t               *env_ptr;

    /** The `struct wlr_surface` wrapped. */
    struct wlr_surface        *wlr_surface_ptr;

    /** The scene API node displaying a surface and all it's sub-surfaces. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Committed width of the surface, in pixels. */
    int                       committed_width;
    /** Committed height of the surface, in pixels. */
    int                       committed_height;

    /** Listener for the `events.commit` signal of `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

    /** Whether this surface is activated, ie. has keyboard focus. */
    bool                      activated;
};

/**
 * Creates a toolkit surface from the `wlr_surface_ptr`.
 *
 * @param wlr_surface_ptr
 * @param env_ptr
 *
 * @return A pointer to the @ref wlmtk_surface_t. Must be destroyed by calling
 *     @ref wlmtk_surface_destroy.
 */
wlmtk_surface_t *wlmtk_surface_create(
    struct wlr_surface *wlr_surface_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the toolkit surface.
 *
 * @param surface_ptr
 */
void wlmtk_surface_destroy(wlmtk_surface_t *surface_ptr);


/**
 * Returns a pointer to the surface's element superclass instance.
 *
 * @param surface_ptr
 *
 * @return Pointer to the corresponding @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_surface_element(wlmtk_surface_t *surface_ptr);

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
 * FIXME: Should no longer be required externally.
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

/**
 * Activates the surface.
 *
 * @param surface_ptr
 * @param activated
 */
void wlmtk_surface_set_activated(
    wlmtk_surface_t *surface_ptr,
    bool activated);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_surface_test_cases[];

/** Fake surface, useful for unit test. */
struct _wlmtk_fake_surface_t {
    /** Superclass: surface. */
    wlmtk_surface_t           surface;
};

/** Ctor for the fake surface.*/
wlmtk_fake_surface_t *wlmtk_fake_surface_create(void);

/** Dtor for the fake surface.*/
void wlmtk_fake_surface_destroy(wlmtk_fake_surface_t *fake_surface_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SURFACE_H__ */
/* == End of surface.h ===================================================== */
