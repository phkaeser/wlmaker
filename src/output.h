/* ========================================================================= */
/**
 * @file output.h
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
#ifndef __WLMAKER_OUTPUT_H__
#define __WLMAKER_OUTPUT_H__

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#undef WLR_USE_UNSTABLE

/** Handle for a compositor output device. */
typedef struct _wlmaker_output_t wlmaker_output_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Handle for a compositor output device. */
struct _wlmaker_output_t {
    /** List node for insertion to server's list of outputs. */
    bs_dllist_node_t          node;
    /** Back-link to the server this output belongs to. */
    wlmaker_server_t          *server_ptr;

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
};

/**
 * Creates an output device from |wlr_output_ptr|.
 *
 * @param wlr_output_ptr
 * @param wlr_allocator_ptr
 * @param wlr_renderer_ptr
 * @param wlr_scene_ptr
 * @param width
 * @param height
 * @param server_ptr
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
    wlmaker_server_t *server_ptr);

/**
 * Destroys the output device handle, as created by wlmaker_output_create().
 *
 * @param output_ptr
 */
void wlmaker_output_destroy(wlmaker_output_t *output_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_OUTPUT_H__ */
/* == End of output.h ====================================================== */
