/* ========================================================================= */
/**
 * @file output_config.h
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
#ifndef __WLMBE_OUTPUT_CONFIG_H__
#define __WLMBE_OUTPUT_CONFIG_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client-protocol.h>

/** Forward declaration. */
typedef struct _wlmbe_output_config_t wlmbe_output_config_t;

struct wlr_output;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** An output's position. */
typedef struct {
    /** Horizontal, in pixels. */
    int32_t                   x;
    /** Vertical, in pixels. */
    int32_t                   y;
} wlmbe_output_config_position_t;

/** An output's position. */
typedef struct {
    /** Width, in pixels. */
    int32_t                   width;
    /** Height, in pixels. */
    int32_t                   height;
    /** Refresh rate, in mHz. Seet 0, to let backend pick a preferred value. */
    int32_t                   refresh;
} wlmbe_output_config_mode_t;

/** Attributes of the output. */
typedef struct {
    /** Default transformation for the output(s). */
    enum wl_output_transform  transformation;
    /** Default scaling factor to use for the output(s). */
    double                    scale;

    /** Whether this output is enabled. */
    bool                      enabled;

    /** Position of this output. */
    wlmbe_output_config_position_t position;
    /** Whether the 'Position' field was present. */
    bool                      has_position;

    /** Mode of this output. */
    wlmbe_output_config_mode_t mode;
    /** Whether the 'Mode' field was present. */
    bool                      has_mode;
} wlmbe_output_config_attributes_t;

/** Returns the base pointer from the  @ref wlmbe_output_config_t::dlnode. */
wlmbe_output_config_t *wlmbe_output_config_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/** Returns the base pointer from the  @ref wlmbe_output_config_t::dlnode. */
bs_dllist_node_t *wlmbe_dlnode_from_output_config(
    wlmbe_output_config_t *config_ptr);

/** Returns pointer to @ref wlmbe_output_config_t::attributes. */
wlmbe_output_config_attributes_t *wlmbe_output_config_attributes(
    wlmbe_output_config_t *config_ptr);

/**
 * Creates a new output config from `wlr_output`.
 *
 * @param wlr_output_ptr
 *
 * @return New output configuration or NULL on error.
 */
wlmbe_output_config_t *wlmbe_output_config_create_from_wlr(
    struct wlr_output *wlr_output_ptr);

/**
 * Creates a new output config from the plist dictionnary `dict_ptr`.
 *
 * @param dict_ptr
 *
 * @return New output configuration or NULL on error.
 */
wlmbe_output_config_t *wlmbe_output_config_create_from_plist(
    bspl_dict_t *dict_ptr);

/** Destroys the output configuration. */
void wlmbe_output_config_destroy(wlmbe_output_config_t *config_ptr);

/**
 * Returns whether the backend configuration equals the wlr_output attributes.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_t::dlnode.
 * @param ud_ptr              To a `struct wlr_output`.
 *
 * @return true if it equals.
 */
bool wlmbe_output_config_equals(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/**
 * Returns if the backend configuration fnmatches the wlr_output attributes.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_t::dlnode.
 * @param ud_ptr              To a `struct wlr_output`.
 *
 * @return true if it matches.
 */
bool wlmbe_output_config_fnmatches(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/** Unit tests for the output module. */
extern const bs_test_case_t wlmbe_output_config_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMBE_OUTPUT_CONFIG_H__ */
/* == End of output_config.h ================================================== */
