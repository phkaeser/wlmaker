/* ========================================================================= */
/**
 * @file output.c
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

#include "output.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <toolkit/toolkit.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Handle for a compositor output device. */
struct _wlmbe_output_t {
    /** List node for insertion in @ref wlmbe_backend_t::outputs. */
    bs_dllist_node_t          dlnode;

    /** Listener for `destroy` signals raised by `wlr_output`. */
    struct wl_listener        output_destroy_listener;
    /** Listener for `frame` signals raised by `wlr_output`. */
    struct wl_listener        output_frame_listener;
    /** Listener for `request_state` signals raised by `wlr_output`. */
    struct wl_listener        output_request_state_listener;

    /** Descriptive name, showing manufacturer, model and serial. */
    char                      *description_ptr;

    // Below: Not owned by @ref wlmbe_output_t.
    /** Refers to the compositor output region, from wlroots. */
    struct wlr_output         *wlr_output_ptr;
    /** Refers to the scene graph used. */
    struct wlr_scene          *wlr_scene_ptr;
    /** This output's configuration. */
    wlmbe_output_config_t     *output_config_ptr;
};

static void _wlmbe_output_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_output_t *wlmbe_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    wlmbe_output_config_t *config_ptr,
    int width,
    int height)
{
    wlmbe_output_t *output_ptr = logged_calloc(1, sizeof(wlmbe_output_t));
    if (NULL == output_ptr) return NULL;
    output_ptr->wlr_output_ptr = wlr_output_ptr;
    output_ptr->wlr_scene_ptr = wlr_scene_ptr;
    output_ptr->output_config_ptr = config_ptr;
    output_ptr->wlr_output_ptr->data = output_ptr;

    output_ptr->description_ptr = bs_strdupf(
        "\"%s\": Manufacturer: \"%s\", Model \"%s\", Serial \"%s\"",
        wlr_output_ptr->name,
        wlr_output_ptr->make ? wlr_output_ptr->make : "Unknown",
        wlr_output_ptr->model ? wlr_output_ptr->model : "Unknown",
        wlr_output_ptr->serial ? wlr_output_ptr->serial : "Unknown");
    if (NULL == output_ptr->description_ptr) {
        wlmbe_output_destroy(output_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.destroy,
        &output_ptr->output_destroy_listener,
        _wlmbe_output_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.frame,
        &output_ptr->output_frame_listener,
        _wlmbe_output_handle_frame);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.request_state,
        &output_ptr->output_request_state_listener,
        _wlmbe_output_handle_request_state);

    // From tinwywl: Configures the output created by the backend to use our
    // allocator and our renderer. Must be done once, before commiting the
    // output.
    if (!wlr_output_init_render(
            output_ptr->wlr_output_ptr,
            wlr_allocator_ptr,
            wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        return NULL;
    }

    const wlmbe_output_config_attributes_t *attr_ptr =
        wlmbe_output_config_attributes(output_ptr->output_config_ptr);
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, attr_ptr->enabled);
    bs_log(BS_ERROR, "FIXME: set scale %.2f", attr_ptr->scale);
    wlr_output_state_set_scale(&state, attr_ptr->scale);

    // Issue #97: Found that X11 and transformations do not translate
    // cursor coordinates well. Force it to 'Normal'.
    enum wl_output_transform transformation =
        attr_ptr->transformation;
    if (wlr_output_is_x11(wlr_output_ptr) &&
        transformation != WL_OUTPUT_TRANSFORM_NORMAL) {
        bs_log(BS_WARNING, "X11 backend: Transformation changed to 'Normal'.");
        transformation = WL_OUTPUT_TRANSFORM_NORMAL;
    }
    wlr_output_state_set_transform(&state, transformation);

    // Set modes for backends that have them.
    if (attr_ptr->has_mode) {
        wlr_output_state_set_custom_mode(
            &state,
            attr_ptr->mode.width,
            attr_ptr->mode.height,
            attr_ptr->mode.refresh);
    } else {
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
    }

    if ((wlr_output_is_x11(wlr_output_ptr) ||
         wlr_output_is_wl(wlr_output_ptr))
        && 0 < width && 0 < height) {
        bs_log(BS_INFO, "Overriding output dimensions to %"PRIu32"x%"PRIu32,
               width, height);
        wlr_output_state_set_custom_mode(
            &state, width, height, 0);
    }

    if (!wlr_output_test_state(output_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_ERROR, "Failed wlr_output_test_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        wlr_output_state_finish(&state);
        return NULL;
    }

    // Enable the output and commit.
    bool rv = wlr_output_commit_state(output_ptr->wlr_output_ptr, &state);
    wlr_output_state_finish(&state);
    if (!rv) {
        bs_log(BS_ERROR, "Failed wlr_output_commit_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        return NULL;
    }

    return output_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_destroy(wlmbe_output_t *output_ptr)
{
    if (NULL != output_ptr->wlr_output_ptr) {
        bs_log(BS_INFO, "Destroy output %s", output_ptr->wlr_output_ptr->name);
    }

    _wlmbe_output_handle_destroy(&output_ptr->output_destroy_listener, NULL);

    if (NULL != output_ptr->description_ptr) {
        free(output_ptr->description_ptr);
        output_ptr->description_ptr = NULL;
    }
    free(output_ptr);
}

/* ------------------------------------------------------------------------- */
const char *wlmbe_output_description(wlmbe_output_t *output_ptr)
{
    return output_ptr->description_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmbe_wlr_output_from_output(wlmbe_output_t *output_ptr)
{
    return output_ptr->wlr_output_ptr;
}

/* ------------------------------------------------------------------------- */
const wlmbe_output_config_attributes_t *wlmbe_output_attributes(
    wlmbe_output_t *output_ptr)
{
    return wlmbe_output_config_attributes(output_ptr->output_config_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_update_attributes(
    wlmbe_output_t *output_ptr,
    int x,
    int y,
    bool has_position)
{
    wlmbe_output_config_update_attributes(
        output_ptr->output_config_ptr,
        output_ptr->wlr_output_ptr,
        x, y, has_position);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmbe_dlnode_from_output(wlmbe_output_t *output_ptr)
{
    return &output_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_t *wlmbe_output_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmbe_output_t, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmbe_output_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_destroy_listener);

    wl_list_remove(&output_ptr->output_request_state_listener.link);
    wl_list_remove(&output_ptr->output_frame_listener.link);
    wl_list_remove(&output_ptr->output_destroy_listener.link);
    output_ptr->wlr_output_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `frame` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmbe_output_handle_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_frame_listener);

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
void _wlmbe_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_request_state_listener);
    const struct wlr_output_event_request_state *event_ptr = data_ptr;

    if (wlr_output_commit_state(output_ptr->wlr_output_ptr,
                                event_ptr->state)) {
        wlmbe_output_update_attributes(output_ptr, 0, 0, false);
    } else {
        bs_log(BS_WARNING, "Failed wlr_output_commit_state('%s', %p)",
               output_ptr->wlr_output_ptr->name, event_ptr->state);
    }
}

/* == End of output.c ====================================================== */
