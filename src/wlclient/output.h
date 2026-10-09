/* ========================================================================= */
/**
 * @file output.h
 * A module for keeping track of the Wayland client's outputs.
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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
#ifndef __WLMAKER_WLCLIENT_OUTPUT_H__
#define __WLMAKER_WLCLIENT_OUTPUT_H__

#include <libbase/libbase.h>
#include <stdint.h>

#include "wlclient.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmcl_output ;
struct wlmcl_output_interface;

/** Metadata of an output. */
struct wlmcl_output_metadata {
    /** Name of this output, as UTF-8 string. Unique across outputs. */
    char                      *name_ptr;
    /** Description of this output. UTF-8 string. Not required to be unique. */
    char                      *description_ptr;
    /** Textual description of the output's manufacturer. Or NULL. */
    char                      *make_ptr;
    /** Textual description of the output's model. Or NULL. */
    char                      *model_ptr;
    /** Width of current mode, in pixels. */
    int32_t                   width;
    /** Height of current mode, in pixels. */
    int32_t                   height;
};

/** Registers the output interface to the client. */
struct wlmcl_output_interface *wlmcl_output_interface_register(
    wlmcl_client_t *wlclient_ptr);

/** Unregisters the output interface and releases resources. */
void wlmcl_output_interface_unregister(struct wlmcl_output_interface *if_ptr);

/** Tears down one single output. */
void wlmcl_output_teardown(
    void *userdata_ptr,
    uint32_t name);

/** Retrieve output's metadata, pointer to @ref wlmcl_output::metadata. */
const struct wlmcl_output_metadata *wlmcl_output_metadata(
    struct wlmcl_output *output_ptr);

/** Returns the output from the node. */
struct wlmcl_output *wlmcl_output_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/** Unit tests set. */
extern const bs_test_set_t wlmcl_output_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_WLCLIENT_OUTPUT_H__
/* == End of output.h ====================================================== */
