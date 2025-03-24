/* ========================================================================= */
/**
 * @file output_manager.c
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

#include "output_manager.h"

#include <libbase/libbase.h>
#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/backend/x11.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Implementation of the wlr output manager. */
struct _wlmaker_output_manager_t {
    /** Points to wlroots `struct wlr_output_manager_v1`. */
    struct wlr_output_manager_v1 *wlr_output_manager_v1_ptr;
    /** Points to wlroots 'struct wlr_xdg_output_manager_v1`. */
    struct wlr_xdg_output_manager_v1 *wlr_xdg_output_manager_v1_ptr;

    /** Points to struct wlr_backend. */
    struct wlr_backend *wlr_backend_ptr;
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout *wlr_output_layout_ptr;


    /** Listener for wlr_output_manager_v1::events.destroy. */
    struct wl_listener        destroy_listener;

    /** Listener for wlr_output_manager_v1::events.apply. */
    struct wl_listener        apply_listener;
    /** Listener for wlr_output_manager_v1::events.test. */
    struct wl_listener        test_listener;
};

/** Argument to @ref _wlmaker_output_manager_add_dlnode_output. */
typedef struct {
    /** Links to the output manager. */
    wlmaker_output_manager_t  *output_manager_ptr;
    /** The output configuration to update. */
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr;
} _wlmaker_output_manager_add_dlnode_output_arg_t;

/** Argument to @ref _wlmaker_output_config_head_apply. */
typedef struct {
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout *wlr_output_layout_ptr;
    /** Whether to test only, or to apply "really". */
    bool really;
} _wlmaker_output_config_head_apply_arg_t;

static void _wlmaker_output_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_output_handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_output_manager_destroy(
    wlmaker_output_manager_t *output_manager_ptr);

static void _wlmaker_output_manager_add_dlnode_output(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool _wlmaker_output_config_head_apply(
    struct wl_list *link_ptr,
    void *ud_ptr);

static bool _wlmaker_output_manager_apply(
    wlmaker_output_manager_t *output_manager_ptr,
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr,
    bool really);

static void _wlmaker_output_manager_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_output_manager_handle_apply(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_output_manager_handle_test(
    struct wl_listener *listener_ptr,
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

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_output_t *wlmaker_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    uint32_t width,
    uint32_t height,
    wlmaker_server_t *server_ptr)
{
    wlmaker_output_t *output_ptr = logged_calloc(1, sizeof(wlmaker_output_t));
    if (NULL == output_ptr) return NULL;
    output_ptr->wlr_output_ptr = wlr_output_ptr;
    output_ptr->wlr_allocator_ptr = wlr_allocator_ptr;
    output_ptr->wlr_renderer_ptr = wlr_renderer_ptr;
    output_ptr->wlr_scene_ptr = wlr_scene_ptr;
    output_ptr->server_ptr = server_ptr;

    wlmcfg_dict_t *output_dict_ptr = wlmcfg_dict_ref(
        wlmcfg_dict_get_dict(server_ptr->config_dict_ptr,
                             _wlmaker_output_dict_name));
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
        _wlmaker_output_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.frame,
        &output_ptr->output_frame_listener,
        _wlmaker_output_handle_frame);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.request_state,
        &output_ptr->output_request_state_listener,
        _wlmaker_output_handle_request_state);

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
    wlr_output_state_set_scale(&state, output_ptr->scale);

    // Issue #97: Found that X11 and transformations do not translate
    // cursor coordinates well. Force it to 'Normal'.
    if (wlr_output_is_x11(wlr_output_ptr) &&
        output_ptr->transformation != WL_OUTPUT_TRANSFORM_NORMAL) {
        const char *name_ptr = "Unknown";
        wlmcfg_enum_value_to_name(
            _wlmaker_output_transformation_desc,
            output_ptr->transformation,
            &name_ptr);
        bs_log(BS_WARNING, "Found X11 backend with Output.Transformation "
               "'%s'. Overriding to 'Normal'.", name_ptr);
        output_ptr->transformation = WL_OUTPUT_TRANSFORM_NORMAL;
    }
    wlr_output_state_set_transform(&state, output_ptr->transformation);

    const char *transformation_name_ptr = "Unknown";
    wlmcfg_enum_value_to_name(
        _wlmaker_output_transformation_desc,
        output_ptr->transformation,
        &transformation_name_ptr);
    bs_log(BS_INFO, "Configured transformation '%s' and scale %.2f on %s",
           transformation_name_ptr, output_ptr->scale,
           output_ptr->wlr_output_ptr->name);

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

    if ((wlr_output_is_x11(wlr_output_ptr) ||
         wlr_output_is_wl(wlr_output_ptr))
        && 0 < width && 0 < height) {
        bs_log(BS_INFO, "Overriding output dimensions to %"PRIu32"x%"PRIu32,
               width, height);
        wlr_output_state_set_custom_mode(&state, width, height, 0);
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

/* ------------------------------------------------------------------------- */
wlmaker_output_manager_t *wlmaker_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    wlmaker_output_manager_t *output_manager_ptr = logged_calloc(
        1, sizeof(wlmaker_output_manager_t));
    if (NULL == output_manager_ptr) return NULL;
    output_manager_ptr->wlr_backend_ptr = wlr_backend_ptr;
    output_manager_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    output_manager_ptr->wlr_output_manager_v1_ptr =
        wlr_output_manager_v1_create(wl_display_ptr);
    if (NULL == output_manager_ptr->wlr_output_manager_v1_ptr) {
        _wlmaker_output_manager_destroy(output_manager_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.destroy,
        &output_manager_ptr->destroy_listener,
        _wlmaker_output_manager_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.apply,
        &output_manager_ptr->apply_listener,
        _wlmaker_output_manager_handle_apply);
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.test,
        &output_manager_ptr->test_listener,
        _wlmaker_output_manager_handle_test);

    output_manager_ptr->wlr_xdg_output_manager_v1_ptr =
        wlr_xdg_output_manager_v1_create(
            wl_display_ptr,
            wlr_output_layout_ptr);


    return output_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_output_manager_update_config(
    wlmaker_output_manager_t *output_manager_ptr,
    wlmaker_server_t *server_ptr)
{
    _wlmaker_output_manager_add_dlnode_output_arg_t arg = {
        .output_manager_ptr = output_manager_ptr,
        .wlr_output_configuration_v1_ptr = wlr_output_configuration_v1_create()
    };
    if (NULL == arg.wlr_output_configuration_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_output_configuration_v1_create()");
        return;
    }

    bs_dllist_for_each(
        &server_ptr->outputs,
        _wlmaker_output_manager_add_dlnode_output,
        &arg);

    wlr_output_manager_v1_set_configuration(
        output_manager_ptr->wlr_output_manager_v1_ptr,
        arg.wlr_output_configuration_v1_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_output_handle_destroy(
    struct wl_listener *listener_ptr,
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
void _wlmaker_output_handle_frame(
    struct wl_listener *listener_ptr,
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
void _wlmaker_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_t, output_request_state_listener);

    const struct wlr_output_event_request_state *event_ptr = data_ptr;
    wlr_output_commit_state(output_ptr->wlr_output_ptr, event_ptr->state);
}

/* ------------------------------------------------------------------------- */
/** Dtor. */
void _wlmaker_output_manager_destroy(wlmaker_output_manager_t *output_manager_ptr)
{
    if (NULL != output_manager_ptr->wlr_output_manager_v1_ptr) {
        wlmtk_util_disconnect_listener(
            &output_manager_ptr->test_listener);
        wlmtk_util_disconnect_listener(
            &output_manager_ptr->apply_listener);
        wlmtk_util_disconnect_listener(
            &output_manager_ptr->destroy_listener);
        output_manager_ptr->wlr_output_manager_v1_ptr = NULL;
    }

    free(output_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Iterator callback: Adds the output to the configuration.
 *
 * @param dlnode_ptr         Pointer to @ref wlmaker_output_t::node.
 * @param ud_ptr             Pointer to struct wlr_output_configuration_v1.
 */
void _wlmaker_output_manager_add_dlnode_output(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmaker_output_t *output_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_output_t, node);
    _wlmaker_output_manager_add_dlnode_output_arg_t *arg_ptr = ud_ptr;

    struct wlr_output_configuration_head_v1 *head_v1_ptr =
        wlr_output_configuration_head_v1_create(
            arg_ptr->wlr_output_configuration_v1_ptr,
            output_ptr->wlr_output_ptr);
    if (NULL == head_v1_ptr) {
        bs_log(BS_ERROR,
               "Failed wlr_output_configuration_head_v1_create(%p, %p)",
               arg_ptr->wlr_output_configuration_v1_ptr,
               output_ptr->wlr_output_ptr);
        return;
    }

    struct wlr_box box;
    wlr_output_layout_get_box(
        arg_ptr->output_manager_ptr->wlr_output_layout_ptr,
        output_ptr->wlr_output_ptr,
        &box);
    head_v1_ptr->state.x = box.x;
    head_v1_ptr->state.y = box.y;
}

/* ------------------------------------------------------------------------- */
/**
 * Applies the heads's output configuration.
 *
 * Callback for @ref wlmtk_util_wl_list_for_each.
 *
 * @param link_ptr
 * @param ud_ptr
 *
 * @return true if the tests & apply methods succeeded.
 */
static bool _wlmaker_output_config_head_apply(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_configuration_head_v1 *head_v1_ptr  = BS_CONTAINER_OF(
        link_ptr, struct wlr_output_configuration_head_v1, link);
    struct wlr_output_state state = {};
    _wlmaker_output_config_head_apply_arg_t *arg_ptr = ud_ptr;

    // Convenience pointers. Guard against accidental misses.
    struct wlr_output *wlr_output_ptr = head_v1_ptr->state.output;
    if (NULL == wlr_output_ptr) {
        bs_log(BS_ERROR, "Unexpected NULL output in head %p", head_v1_ptr);
        return false;
    }

    wlr_output_head_v1_state_apply(&head_v1_ptr->state, &state);
    if (!wlr_output_test_state(wlr_output_ptr, &state)) return false;
    if (!arg_ptr->really) return true;

    if (!wlr_output_commit_state(wlr_output_ptr, &state)) return false;

    int x = head_v1_ptr->state.x, y = head_v1_ptr->state.y;
    struct wlr_output_layout *wlr_output_layout_ptr =
        arg_ptr->wlr_output_layout_ptr;
    if (head_v1_ptr->state.enabled &&
        !wlr_output_layout_add(wlr_output_layout_ptr, wlr_output_ptr, x, y)) {
        bs_log(BS_ERROR, "Failed wlr_output_layout_add(%p, %p, %d, %d)",
               wlr_output_layout_ptr, wlr_output_ptr, x, y);
        return false;
    } else if (!head_v1_ptr->state.enabled) {
        wlr_output_layout_remove(wlr_output_layout_ptr, wlr_output_ptr);
    }

    bs_log(BS_INFO, "Applied: Output '%s' %s to %dx%d@%.2f position (%d,%d)",
           wlr_output_ptr->name,
           head_v1_ptr->state.enabled ? "enabled" : "disabled",
           wlr_output_ptr->width, wlr_output_ptr->height,
           1e-3 * wlr_output_ptr->refresh, x, y);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Tests and applies an output configuration.
 *
 * @param output_manager_ptr
 * @param wlr_output_configuration_v1_ptr
 * @param really              Whether to not just test, but also apply it.
 *
 * @return true on success.
 */
bool _wlmaker_output_manager_apply(
    wlmaker_output_manager_t *output_manager_ptr,
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr,
    bool really)
{
    _wlmaker_output_config_head_apply_arg_t arg = {
        .wlr_output_layout_ptr = output_manager_ptr->wlr_output_layout_ptr,
        .really = really
    };
    if (!wlmtk_util_wl_list_for_each(
            &wlr_output_configuration_v1_ptr->heads,
            _wlmaker_output_config_head_apply,
            &arg)) {
        return false;
    }

    size_t states_len;
    struct wlr_backend_output_state *wlr_backend_output_state_ptr =
        wlr_output_configuration_v1_build_state(
            wlr_output_configuration_v1_ptr, &states_len);
    if (NULL == wlr_backend_output_state_ptr) {
        bs_log(BS_ERROR,
               "Failed wlr_output_configuration_v1_build_state(%p, %p)",
               wlr_output_configuration_v1_ptr, &states_len);
        return false;
    }

    bool rv = wlr_backend_test(
        output_manager_ptr->wlr_backend_ptr,
        wlr_backend_output_state_ptr,
        states_len);
    if (rv && really) {
        rv = wlr_backend_commit(
            output_manager_ptr->wlr_backend_ptr,
            wlr_backend_output_state_ptr,
            states_len);
    }
    free(wlr_backend_output_state_ptr);

    return rv;
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.destroy. Cleans up. */
void _wlmaker_output_manager_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_output_manager_t *output_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_manager_t, destroy_listener);
    _wlmaker_output_manager_destroy(output_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.apply. Cleans up. */
void _wlmaker_output_manager_handle_apply(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_output_manager_t *om_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_manager_t, apply_listener);
    struct wlr_output_configuration_v1 *wlr_output_config_ptr = data_ptr;

    if (_wlmaker_output_manager_apply(om_ptr, wlr_output_config_ptr, true)) {
        wlr_output_configuration_v1_send_succeeded(wlr_output_config_ptr);
    } else {
        wlr_output_configuration_v1_send_failed(wlr_output_config_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.test. */
void _wlmaker_output_manager_handle_test(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_output_manager_t *om_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_output_manager_t, test_listener);
    struct wlr_output_configuration_v1 *wlr_output_config_ptr = data_ptr;

    if (_wlmaker_output_manager_apply(om_ptr, wlr_output_config_ptr, false)) {
        wlr_output_configuration_v1_send_succeeded(wlr_output_config_ptr);
    } else {
        wlr_output_configuration_v1_send_failed(wlr_output_config_ptr);
    }
}

/* == End of output_manager.c ============================================== */
