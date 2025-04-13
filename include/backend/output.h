/* ========================================================================= */
/**
 * @file output.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMBE_OUTPUT_H__
#define __WLMBE_OUTPUT_H__

#include <wayland-client-protocol.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>

/** Handle for an output device. */
typedef struct _wlmbe_output_t wlmbe_output_t;

/** Forward declaration. */
typedef struct _wlmbe_output_config_t wlmbe_output_config_t;

struct wlr_output;
struct wlr_allocator;
struct wlr_renderer;
struct wlr_scene;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Output configuration. */
struct _wlmbe_output_config_t {
    /** Name of this output. */
    char                      *name_ptr;
    /** Whether a 'Name' entry was present. */
    bool                      has_name;

    /** Attributes of the output. */
    struct {
        /** Default transformation for the output(s). */
        enum wl_output_transform  transformation;
        /** Default scaling factor to use for the output(s). */
        double                    scale;
    } attr;
};

/**
 * Creates an output device from `wlr_output_ptr`.
 *
 * @param wlr_output_ptr
 * @param wlr_allocator_ptr
 * @param wlr_renderer_ptr
 * @param wlr_scene_ptr
 * @param config_ptr
 * @param width
 * @param height
 *
 * @return The output device handle or NULL on error.
 */
wlmbe_output_t *wlmbe_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    wlmbe_output_config_t *config_ptr,
    int width,
    int height);

/**
 * Destroys the output device handle, as created by @ref wlmbe_output_create.
 *
 * @param output_ptr
 */
void wlmbe_output_destroy(wlmbe_output_t *output_ptr);

/** Returns @ref wlmbe_output_t::wlr_output_ptr. */
struct wlr_output *wlmbe_wlr_output_from_output(wlmbe_output_t *output_ptr);

/** Returns a pointer to @ref wlmbe_output_t::dlnode. */
bs_dllist_node_t *wlmbe_dlnode_from_output(wlmbe_output_t *output_ptr);

/** Returns the @ref wlmbe_output_t for @ref wlmbe_output_t::dlnode. */
wlmbe_output_t *wlmbe_output_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/** Initializes the configuration from the source. */
bool wlmbe_output_config_init_from_config(
    wlmbe_output_config_t *config_ptr,
    const wlmbe_output_config_t *source_config_ptr);

/** Initializes the configuration from a plist. */
bool wlmbe_output_config_init_from_plist(
    wlmbe_output_config_t *config_ptr,
    bspl_dict_t *dict_ptr);

/** Initializes the configuration from a wlr_output. */
bool wlmbe_output_config_init_from_wlr(
    wlmbe_output_config_t *config_ptr,
    struct wlr_output *wlr_output_ptr);

/** Un-initializes the configuration. */
void wlmbe_output_config_fini(wlmbe_output_config_t *config_ptr);

/** Unit tests for the output module. */
extern const bs_test_case_t wlmbe_output_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_OUTPUT_H__ */
/* == End of output.h ====================================================== */
