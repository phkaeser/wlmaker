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

#include <conf/decode.h>
#include <conf/model.h>
#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

#define WLR_USE_UNSTABLE
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Implementation of the wlr output manager. */
struct _wlmbe_output_manager_t {
    /** Points to wlroots `struct wlr_output_manager_v1`. */
    struct wlr_output_manager_v1 *wlr_output_manager_v1_ptr;

    /** Listener for wlr_output_manager_v1::events.destroy. */
    struct wl_listener        wlr_om_destroy_listener;
    /** Listener for wlr_output_manager_v1::events.apply. */
    struct wl_listener        apply_listener;
    /** Listener for wlr_output_manager_v1::events.test. */
    struct wl_listener        test_listener;

    /** Points to wlroots 'struct wlr_xdg_output_manager_v1`. */
    struct wlr_xdg_output_manager_v1 *wlr_xdg_output_manager_v1_ptr;
    struct wl_listener        xdg_om_destroy_listener;

    /** Listener for wlr_output_layout::events.destroy. */
    struct wl_listener        output_layout_destroy_listener;
    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;

    // Below: Not owned by @ref wlmbe_output_manager_t.
    /** The scene. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Points to struct wlr_backend. */
    struct wlr_backend        *wlr_backend_ptr;
};

/** Argument to @ref _wlmaker_output_manager_config_head_apply. */
typedef struct {
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout *wlr_output_layout_ptr;
    /** Whether to test only, or to apply "really". */
    bool really;
} _wlmaker_output_manager_config_head_apply_arg_t;

static bool _wlmbe_output_manager_update_output_configuration(
    struct wl_list *link_ptr,
    void *ud_ptr);
static bool _wlmaker_output_manager_config_head_apply(
    struct wl_list *link_ptr,
    void *ud_ptr);

static bool _wlmbe_output_manager_apply(
    wlmbe_output_manager_t *output_manager_ptr,
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr,
    bool really);

static void _wlmbe_output_manager_handle_wlr_om_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_manager_handle_apply(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_manager_handle_test(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_manager_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_manager_handle_output_layout_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_manager_handle_xdg_om_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_output_manager_t *wlmbe_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_backend *wlr_backend_ptr)
{
    wlmbe_output_manager_t *output_manager_ptr = logged_calloc(
        1, sizeof(wlmbe_output_manager_t));
    if (NULL == output_manager_ptr) return NULL;
    output_manager_ptr->wlr_backend_ptr = wlr_backend_ptr;
    output_manager_ptr->wlr_scene_ptr = wlr_scene_ptr;
    output_manager_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    output_manager_ptr->wlr_output_manager_v1_ptr =
        wlr_output_manager_v1_create(wl_display_ptr);
    if (NULL == output_manager_ptr->wlr_output_manager_v1_ptr) {
        wlmbe_output_manager_destroy(output_manager_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.destroy,
        &output_manager_ptr->wlr_om_destroy_listener,
        _wlmbe_output_manager_handle_wlr_om_destroy);
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.apply,
        &output_manager_ptr->apply_listener,
        _wlmbe_output_manager_handle_apply);
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_output_manager_v1_ptr->events.test,
        &output_manager_ptr->test_listener,
        _wlmbe_output_manager_handle_test);

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &output_manager_ptr->output_layout_change_listener,
        _wlmbe_output_manager_handle_output_layout_change);
    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.destroy,
        &output_manager_ptr->output_layout_destroy_listener,
        _wlmbe_output_manager_handle_output_layout_destroy);

    output_manager_ptr->wlr_xdg_output_manager_v1_ptr =
        wlr_xdg_output_manager_v1_create(
            wl_display_ptr,
            wlr_output_layout_ptr);
    if (NULL == output_manager_ptr->wlr_xdg_output_manager_v1_ptr) {
        wlmbe_output_manager_destroy(output_manager_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &output_manager_ptr->wlr_xdg_output_manager_v1_ptr->events.destroy,
        &output_manager_ptr->xdg_om_destroy_listener,
        _wlmbe_output_manager_handle_xdg_om_destroy);

    // Initializes the output manager from current output layout.
    _wlmbe_output_manager_handle_output_layout_change(
        &output_manager_ptr->output_layout_change_listener,
        wlr_output_layout_ptr);
    return output_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_manager_destroy(
    wlmbe_output_manager_t *output_manager_ptr)
{
     wlmtk_util_disconnect_listener(
        &output_manager_ptr->output_layout_change_listener);
     wlmtk_util_disconnect_listener(
        &output_manager_ptr->output_layout_destroy_listener);

     _wlmbe_output_manager_handle_wlr_om_destroy(
         &output_manager_ptr->wlr_om_destroy_listener, NULL);
     _wlmbe_output_manager_handle_xdg_om_destroy(
         &output_manager_ptr->xdg_om_destroy_listener, NULL);
    free(output_manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Iterator for the `struct wlr_output_layout_output` referenced from
 * `struct wlr_output_layout::outputs`.
 *
 * Adds the configuration head for the given output to the provided output
 * configuration.
 *
 * @param link_ptr            struct wlr_output_layout_output::link.
 * @param ud_ptr              The output configuration, a pointer to
 *                            `struct wlr_output_configuration_v1`.
 *
 * @return true on success.
 */
bool _wlmbe_output_manager_update_output_configuration(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr =
        ud_ptr;

    struct wlr_output_configuration_head_v1 *head_v1_ptr =
        wlr_output_configuration_head_v1_create(
            wlr_output_configuration_v1_ptr,
            wlr_output_layout_output_ptr->output);
    if (NULL == head_v1_ptr) {
        bs_log(BS_ERROR,
               "Failed wlr_output_configuration_head_v1_create(%p, %p)",
               wlr_output_configuration_v1_ptr,
               wlr_output_layout_output_ptr->output);
        return false;
    }

    struct wlr_box box;
    wlr_output_layout_get_box(
        wlr_output_layout_output_ptr->layout,
        wlr_output_layout_output_ptr->output,
        &box);
    head_v1_ptr->state.x = box.x;
    head_v1_ptr->state.y = box.y;
    return true;
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
static bool _wlmaker_output_manager_config_head_apply(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_configuration_head_v1 *head_v1_ptr  = BS_CONTAINER_OF(
        link_ptr, struct wlr_output_configuration_head_v1, link);
    struct wlr_output_state state = {};
    _wlmaker_output_manager_config_head_apply_arg_t *arg_ptr = ud_ptr;

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
bool _wlmbe_output_manager_apply(
    wlmbe_output_manager_t *output_manager_ptr,
    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr,
    bool really)
{
    _wlmaker_output_manager_config_head_apply_arg_t arg = {
        .wlr_output_layout_ptr = output_manager_ptr->wlr_output_layout_ptr,
        .really = really
    };
    if (!wlmtk_util_wl_list_for_each(
            &wlr_output_configuration_v1_ptr->heads,
            _wlmaker_output_manager_config_head_apply,
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
/** Handler for wlr_output_manager_v1::events.destroy. Detaches. */
void _wlmbe_output_manager_handle_wlr_om_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_manager_t *output_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_manager_t, wlr_om_destroy_listener);

    if (NULL == output_manager_ptr->wlr_output_manager_v1_ptr) return;

     wlmtk_util_disconnect_listener(
         &output_manager_ptr->test_listener);
     wlmtk_util_disconnect_listener(
         &output_manager_ptr->apply_listener);
     wlmtk_util_disconnect_listener(
         &output_manager_ptr->wlr_om_destroy_listener);
    output_manager_ptr->wlr_output_manager_v1_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_xdg_output_manager_v1::events.destroy. Detaches. */
void _wlmbe_output_manager_handle_xdg_om_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_manager_t *output_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_manager_t, xdg_om_destroy_listener);

    if (NULL == output_manager_ptr->wlr_xdg_output_manager_v1_ptr) return;

     wlmtk_util_disconnect_listener(
         &output_manager_ptr->xdg_om_destroy_listener);
    output_manager_ptr->wlr_xdg_output_manager_v1_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.apply. Cleans up. */
void _wlmbe_output_manager_handle_apply(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmbe_output_manager_t *om_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_manager_t, apply_listener);
    struct wlr_output_configuration_v1 *wlr_output_config_ptr = data_ptr;

    if (_wlmbe_output_manager_apply(om_ptr, wlr_output_config_ptr, true)) {
        wlr_output_configuration_v1_send_succeeded(wlr_output_config_ptr);
    } else {
        wlr_output_configuration_v1_send_failed(wlr_output_config_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handler for wlr_output_manager_v1::events.test. */
void _wlmbe_output_manager_handle_test(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmbe_output_manager_t *om_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_manager_t, test_listener);
    struct wlr_output_configuration_v1 *wlr_output_config_ptr = data_ptr;

    if (_wlmbe_output_manager_apply(om_ptr, wlr_output_config_ptr, false)) {
        wlr_output_configuration_v1_send_succeeded(wlr_output_config_ptr);
    } else {
        wlr_output_configuration_v1_send_failed(wlr_output_config_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles dtor for @ref wlmbe_output_manager_t::wlr_output_layout_ptr. */
void _wlmbe_output_manager_handle_output_layout_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_manager_t *output_manager_ptr = BS_CONTAINER_OF(
        listener_ptr,
        wlmbe_output_manager_t,
        output_layout_destroy_listener);

    wlmbe_output_manager_destroy(output_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles layout change events. */
void _wlmbe_output_manager_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_manager_t *output_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_manager_t, output_layout_change_listener);

    // Guard clause.
    if (NULL == output_manager_ptr->wlr_output_manager_v1_ptr) return;

    struct wlr_output_configuration_v1 *wlr_output_configuration_v1_ptr =
        wlr_output_configuration_v1_create();

    if (NULL == wlr_output_configuration_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_output_configuration_v1_create().");
        return;
    }

    if (wlmtk_util_wl_list_for_each(
            &output_manager_ptr->wlr_output_layout_ptr->outputs,
            _wlmbe_output_manager_update_output_configuration,
            wlr_output_configuration_v1_ptr)) {
        wlr_output_manager_v1_set_configuration(
            output_manager_ptr->wlr_output_manager_v1_ptr,
            wlr_output_configuration_v1_ptr);
        return;
    }

    wlr_output_configuration_v1_destroy(wlr_output_configuration_v1_ptr);
}

/* == End of output_manager.c ============================================== */
