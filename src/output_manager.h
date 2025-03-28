/* ========================================================================= */
/**
 * @file output_manager.h
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
#ifndef __WLMAKER_OUTPUT_MANAGER_H__
#define __WLMAKER_OUTPUT_MANAGER_H__

#include <wayland-server-core.h>

/** Handle for an output device. */
typedef struct _wlmaker_output_t wlmaker_output_t;
/** Forward declaration: Handle for output manager. */
typedef struct _wlmaker_output_manager_t wlmaker_output_manager_t;
/** Forward declaration. */
typedef struct _wlmaker_output_config_t wlmaker_output_config_t;

/** Options for the output manager. */
typedef struct {
    /** Preferred output width, for windowed mode. */
    uint32_t                  width;
    /** Preferred output height, for windowed mode. */
    uint32_t                  height;
} wlmaker_output_manager_options_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Handle for a compositor output device. */
struct _wlmaker_output_t {
    /** List node for insertion to server's list of outputs. */
    bs_dllist_node_t          node;
    /** Back-link to the output manager, if the output is added to one. */
    wlmaker_output_manager_t  *output_manager_ptr;

    /** Refers to the compositor output region, from wlroots. */
    struct wlr_output         *wlr_output_ptr;
    /** Refers to the allocator of the server. */
    struct wlr_allocator      *wlr_allocator_ptr;
    /** Refers to the renderer used for the server. */
    struct wlr_renderer       *wlr_renderer_ptr;
    /** Refers to the scene graph used. */
    struct wlr_scene          *wlr_scene_ptr;

    /** Listener for `destroy` signals raised by `wlr_output`. */
    struct wl_listener        output_destroy_listener;
    /** Listener for `frame` signals raised by `wlr_output`. */
    struct wl_listener        output_frame_listener;
    /** Listener for `request_state` signals raised by `wlr_output`. */
    struct wl_listener        output_request_state_listener;

    /** Default transformation for the output(s). */
    enum wl_output_transform  transformation;
    /** Default scaling factor to use for the output(s). */
    double                    scale;
};

/** Ctor. */
wlmaker_output_manager_t *wlmaker_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    bs_dllist_t *server_outputs_ptr,
    const wlmaker_output_manager_options_t *options_ptr,
    wlmcfg_dict_t *config_dict_ptr);

/** Accessor for @ref wlmaker_output_manager_t::wlr_output_layout_ptr. */
struct wlr_output_layout *wlmaker_output_manager_wlr_output_layout(
    wlmaker_output_manager_t *output_manager_ptr);

/**
 * Returns the primary output. Currently that is the first output added.
 *
 * @param output_manager_ptr
 *
 * @return A pointer to the `struct wlr_output` for the primary output.
 */
struct wlr_output *wlmaker_output_manager_get_primary_output(
    wlmaker_output_manager_t *output_manager_ptr);

/**
 * Creates an output device from |wlr_output_ptr|.
 *
 * @param wlr_output_ptr
 * @param wlr_allocator_ptr
 * @param wlr_renderer_ptr
 * @param wlr_scene_ptr
 * @param width
 * @param height
 * @param config_ptr
 *
 * @return The output device handle or NULL on error.
 */
wlmaker_output_t *wlmaker_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    uint32_t width,
    uint32_t height,
    wlmaker_output_config_t *config_ptr);

/**
 * Destroys the output device handle, as created by wlmaker_output_create().
 *
 * @param output_ptr
 */
void wlmaker_output_destroy(wlmaker_output_t *output_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_OUTPUT_MANAGER_H__ */
/* == End of output_manager.h ============================================== */
