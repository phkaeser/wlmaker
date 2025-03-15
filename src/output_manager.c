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
#include <wlr/types/wlr_output_management_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Implementation of the wlr output manager. */
struct _wlmaker_output_manager_t {
    /** Points to wlroots `struct wlr_output_manager_v1`. */
    struct wlr_output_manager_v1 *wlr_output_manager_v1_ptr;
    /** Points to struct wlr_backend. */
    struct wlr_backend *wlr_backend_ptr;

    /** Listener for wlr_output_manager_v1::events.destroy. */
    struct wl_listener        destroy_listener;

    /** Listener for wlr_output_manager_v1::events.apply. */
    struct wl_listener        apply_listener;
    /** Listener for wlr_output_manager_v1::events.test. */
    struct wl_listener        test_listener;
};

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

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_output_manager_t *wlmaker_output_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr)
{
    wlmaker_output_manager_t *output_manager_ptr = logged_calloc(
        1, sizeof(wlmaker_output_manager_t));
    if (NULL == output_manager_ptr) return NULL;
    output_manager_ptr->wlr_backend_ptr = wlr_backend_ptr;

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

    return output_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_output_manager_update_config(
    wlmaker_output_manager_t *output_manager_ptr,
    wlmaker_server_t *server_ptr)
{
    struct wlr_output_configuration_v1 *config_ptr =
        wlr_output_configuration_v1_create();

    bs_dllist_for_each(
        &server_ptr->outputs,
        _wlmaker_output_manager_add_dlnode_output,
        config_ptr);

    wlr_output_manager_v1_set_configuration(
        output_manager_ptr->wlr_output_manager_v1_ptr,
        config_ptr);
}

/* == Local (static) methods =============================================== */

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
    struct wlr_output_configuration_v1 *config_ptr = ud_ptr;
    wlr_output_configuration_head_v1_create(
        config_ptr, output_ptr->wlr_output_ptr);
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
    bool *really_ptr = ud_ptr;

    wlr_output_head_v1_state_apply(&head_v1_ptr->state, &state);

    if (!wlr_output_test_state(head_v1_ptr->state.output, &state)) {
        return false;
    }

    if (*really_ptr &&
        !wlr_output_commit_state(head_v1_ptr->state.output, &state)) {
        return false;
    }

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
    if (!wlmtk_util_wl_list_for_each(
            &wlr_output_configuration_v1_ptr->heads,
            _wlmaker_output_config_head_apply,
            &really)) {
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
