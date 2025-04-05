/* ========================================================================= */
/**
 * @file buffer.h
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
#ifndef __WLMTK_BUFFER_H__
#define __WLMTK_BUFFER_H__

#include <stdbool.h>

/** Forward declaration: Buffer state. */
typedef struct _wlmtk_buffer_t wlmtk_buffer_t;

#include "element.h"

/** Forward declaration. */
struct wlr_buffer;
/** Forward declaration. */
struct wlr_scene_buffer;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of a texture-backed buffer. */
struct _wlmtk_buffer_t {
    /** Super class of the buffer: An element. */
    wlmtk_element_t           super_element;
    /** Virtual method table of the super element before extending it. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** WLR buffer holding the contents. */
    struct wlr_buffer        *wlr_buffer_ptr;
    /** Scene graph API node. Only set after calling `create_scene_node`. */
    struct wlr_scene_buffer  *wlr_scene_buffer_ptr;

    /** Listener for the `destroy` signal of `wlr_scene_buffer_ptr->node`. */
    struct wl_listener        wlr_scene_buffer_node_destroy_listener;
};

/**
 * Initializes the buffer.
 *
 * @param buffer_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_buffer_init(wlmtk_buffer_t *buffer_ptr,
                       wlmtk_env_t *env_ptr);

/**
 * Cleans up the buffer.
 *
 * @param buffer_ptr
 */
void wlmtk_buffer_fini(wlmtk_buffer_t *buffer_ptr);

/**
 * Sets (or updates) buffer contents.
 *
 * @param buffer_ptr
 * @param wlr_buffer_ptr      A WLR buffer to use for the update. That buffer
 *                            will get locked by @ref wlmtk_buffer_t for the
 *                            duration of it's use.
 */
void wlmtk_buffer_set(
    wlmtk_buffer_t *buffer_ptr,
    struct wlr_buffer *wlr_buffer_ptr);

/** @return the superclass' @ref wlmtk_element_t of `buffer_ptr`. */
wlmtk_element_t *wlmtk_buffer_element(wlmtk_buffer_t *buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BUFFER_H__ */
/* == End of buffer.h ====================================================== */
