/* ========================================================================= */
/**
 * @file output.c
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

/// clock_gettime(2) is a POSIX extension, needs this macro.
#define _POSIX_C_SOURCE 199309L

#include "output.h"

#include "toolkit/toolkit.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

static void handle_output_destroy(struct wl_listener *listener_ptr,
                                  void *data_ptr);
static void handle_output_frame(struct wl_listener *listener_ptr,
                                void *data_ptr);
static void handle_request_state(struct wl_listener *listener_ptr,
                                 void *data_ptr);

/* == Data ================================================================= */

/** Name of the plist dict describing the (default) output configuration. */
static const char *_wlmaker_output_dict_name = "Output";

/** Descriptor for output transformations. */
static const wlmcfg_enum_desc_t _wlmaker_output_transformation_desc[] = {
    WLMCFG_ENUM("Normal", WL_OUTPUT_TRANSFORM_NORMAL),
    WLMCFG_ENUM("Rotate90", WL_OUTPUT_TRANSFORM_90),
    WLMCFG_ENUM("Rotate180", WL_OUTPUT_TRANSFORM_180),
    WLMCFG_ENUM("Rotate270", WL_OUTPUT_TRANSFORM_270),
    WLMCFG_ENUM("Flip", WL_OUTPUT_TRANSFORM_FLIPPED),
    WLMCFG_ENUM("FlipAndRotate90", WL_OUTPUT_TRANSFORM_FLIPPED_90),
    WLMCFG_ENUM("FlipAndRotate180", WL_OUTPUT_TRANSFORM_FLIPPED_180),
    WLMCFG_ENUM("FlipAndRotate270", WL_OUTPUT_TRANSFORM_FLIPPED_270),
    WLMCFG_ENUM_SENTINEL(),
};

/** Descriptor for the output configuration. */
static const wlmcfg_desc_t _wlmaker_output_config_desc[] = {
    WLMCFG_DESC_ENUM("Transformation", true, wlmaker_output_t, transformation,
                     WL_OUTPUT_TRANSFORM_NORMAL,
                     _wlmaker_output_transformation_desc),
    WLMCFG_DESC_DOUBLE("Scale", true, wlmaker_output_t, scale, 1.0),
    WLMCFG_DESC_SENTINEL()
};

/* == Exported Methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_output_t *wlmaker_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_output_t *output_ptr = logged_calloc(1, sizeof(wlmaker_output_t));
    if (NULL == output_ptr) return NULL;
    output_ptr->wlr_output_ptr = wlr_output_ptr;
    output_ptr->wlr_allocator_ptr = wlr_allocator_ptr;
    output_ptr->wlr_renderer_ptr = wlr_renderer_ptr;
    output_ptr->wlr_scene_ptr = wlr_scene_ptr;
    output_ptr->server_ptr = server_ptr;

    wlmcfg_dict_t *output_dict_ptr = wlmcfg_dict_get_dict(
        server_ptr->config_dict_ptr, _wlmaker_output_dict_name);
    if (NULL == output_dict_ptr) {
        bs_log(BS_ERROR, "No '%s' dict.", _wlmaker_output_dict_name);
        wlmaker_output_destroy(output_ptr);
        return NULL;
    }
    bool decoded = wlmcfg_decode_dict(
        output_dict_ptr,
        _wlmaker_output_config_desc,
        output_ptr);
    wlmcfg_dict_unref(output_dict_ptr);
    if (!decoded) {
        bs_log(BS_ERROR, "Failed to decode '%s' dict",
               _wlmaker_output_dict_name);
        wlmaker_output_destroy(output_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.destroy,
        &output_ptr->output_destroy_listener,
        handle_output_destroy);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.frame,
        &output_ptr->output_frame_listener,
        handle_output_frame);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.request_state,
        &output_ptr->output_request_state_listener,
        handle_request_state);

    // From tinwywl: Configures the output created by the backend to use our
    // allocator and our renderer. Must be done once, before commiting the
    // output.
    if (!wlr_output_init_render(
            output_ptr->wlr_output_ptr,
            output_ptr->wlr_allocator_ptr,
            output_ptr->wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmaker_output_destroy(output_ptr);
        return NULL;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_state_set_transform(&state, output_ptr->transformation);
    wlr_output_state_set_scale(&state, output_ptr->scale);

    // Set modes for backends that have them.
    if (!wl_list_empty(&output_ptr->wlr_output_ptr->modes)) {
        struct wlr_output_mode *mode_ptr = wlr_output_preferred_mode(
            output_ptr->wlr_output_ptr);
        bs_log(BS_INFO, "Setting mode %dx%d @ %.2fHz",
               mode_ptr->width, mode_ptr->height, 1e-3 * mode_ptr->refresh);
        wlr_output_state_set_mode(&state, mode_ptr);
    } else {
        bs_log(BS_INFO, "No modes available on %s",
               output_ptr->wlr_output_ptr->name);
    }

    if (!wlr_output_test_state(output_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_ERROR, "Failed wlr_output_test_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmaker_output_destroy(output_ptr);
        wlr_output_state_finish(&state);
        return NULL;
    }

    // Enable the output and commit.
    bool rv = wlr_output_commit_state(output_ptr->wlr_output_ptr, &state);
    wlr_output_state_finish(&state);
    if (!rv) {
        bs_log(BS_ERROR, "Failed wlr_output_commit_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmaker_output_destroy(output_ptr);
        return NULL;
    }

    bs_log(BS_INFO, "Created output %s", output_ptr->wlr_output_ptr->name);
    return output_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_output_destroy(wlmaker_output_t *output_ptr)
{
    if (NULL != output_ptr->wlr_output_ptr) {
        bs_log(BS_INFO, "Destroy output %s", output_ptr->wlr_output_ptr->name);
    }

    wl_list_remove(&output_ptr->output_request_state_listener.link);
    wl_list_remove(&output_ptr->output_frame_listener.link);
    wl_list_remove(&output_ptr->output_destroy_listener.link);

    free(output_ptr);
}

/* == Local Methods ======================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_output_destroy(struct wl_listener *listener_ptr,
                           __UNUSED__ void *data_ptr)
{
    wlmaker_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_t, output_destroy_listener);
    wlmaker_server_output_remove(output_ptr->server_ptr, output_ptr);
    wlmaker_output_destroy(output_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `frame` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_output_frame(struct wl_listener *listener_ptr,
                         __UNUSED__ void *data_ptr)
{
    wlmaker_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_t, output_frame_listener);

    struct wlr_scene_output *wlr_scene_output_ptr = wlr_scene_get_scene_output(
        output_ptr->wlr_scene_ptr,
        output_ptr->wlr_output_ptr);
    wlr_scene_output_commit(wlr_scene_output_ptr, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(wlr_scene_output_ptr, &now);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `request_state` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_request_state(struct wl_listener *listener_ptr,
                          void *data_ptr)
{
    wlmaker_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_t, output_request_state_listener);

    const struct wlr_output_event_request_state *event_ptr = data_ptr;
    wlr_output_commit_state(output_ptr->wlr_output_ptr, event_ptr->state);
}

/* == End of output.c ====================================================== */
