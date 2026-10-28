/* ========================================================================= */
/**
 * @file workspace.c
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

#include "workspace.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stddef.h>

#include "ext-workspace-v1-client-protocol.h"
#include "wlclient/wlclient.h"

struct ext_workspace_group_handle_v1;
struct ext_workspace_handle_v1;
struct ext_workspace_manager_v1;
struct wl_output;

/* == Declarations ========================================================= */

/** State of the ext-workspace-v1 client. */
struct wlmtool_workspace_state {
    struct ext_workspace_manager_v1 *workspace_manager_ptr;
};

void _wlmtool_workspace_setup(
    void *userdata_ptr,
    void *bound_interface_ptr,
    uint32_t name);

static void _wlmtool_workspace_handle_workspace_group(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1,
    struct ext_workspace_group_handle_v1 *workspace_group);
static void _wlmtool_workspace_handle_workspace(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1,
    struct ext_workspace_handle_v1 *workspace);
static void _wlmtool_workspace_handle_done(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1);
static void _wlmtool_workspace_handle_finished(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1);

static void _wlmtool_workspace_group_handle_capabilities(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    uint32_t capabilities);
static void _wlmtool_workspace_group_handle_output_enter(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct wl_output *wl_output_ptr);
static void _wlmtool_workspace_group_handle_output_leave(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct wl_output *wl_output_ptr);
static void _wlmtool_workspace_group_handle_workspace_enter(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct ext_workspace_handle_v1 *workspace);
static void _wlmtool_workspace_group_handle_workspace_leave(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct ext_workspace_handle_v1 *workspace);
static void _wlmtool_workspace_group_handle_removed(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr);

/* == Data ================================================================= */

static const struct ext_workspace_manager_v1_listener
_wlmtool_workspace_listener = {
    .workspace_group = _wlmtool_workspace_handle_workspace_group,
    .workspace = _wlmtool_workspace_handle_workspace,
    .done = _wlmtool_workspace_handle_done,
    .finished = _wlmtool_workspace_handle_finished
};

static const struct ext_workspace_group_handle_v1_listener
_wlmtool_workspace_group_listener = {
    .capabilities = _wlmtool_workspace_group_handle_capabilities,
    .output_enter = _wlmtool_workspace_group_handle_output_enter,
    .output_leave = _wlmtool_workspace_group_handle_output_leave,
    .workspace_enter = _wlmtool_workspace_group_handle_workspace_enter,
    .workspace_leave = _wlmtool_workspace_group_handle_workspace_leave,
    .removed = _wlmtool_workspace_group_handle_removed
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtool_ext_workspace(
    __UNUSED__ int argc,
    __UNUSED__ const char **argv)
{
    wlmcl_client_t *client_ptr = wlmcl_client_create("wlmaker.wlmtool");
    if (NULL == client_ptr) return false;
    struct wlmtool_workspace_state ws_state = {};

    wlmcl_client_register_interface(
        client_ptr,
        &ext_workspace_manager_v1_interface,
        /* desired_version */ 1,
        /* required */ true,
        _wlmtool_workspace_setup,
        &ws_state);

    wlmcl_client_run(client_ptr);

    wlmcl_client_destroy(client_ptr);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Setup callback for binding the ext-workspace-v1 interface. */
void _wlmtool_workspace_setup(
    void *userdata_ptr,
    void *bound_interface_ptr,
    __UNUSED__ uint32_t name)
{
    struct wlmtool_workspace_state *ws_state_ptr = userdata_ptr;
    ws_state_ptr->workspace_manager_ptr = bound_interface_ptr;

    ext_workspace_manager_v1_add_listener(
        ws_state_ptr->workspace_manager_ptr,
        &_wlmtool_workspace_listener,
        ws_state_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_handle_workspace_group(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr)
{
    struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace manager %p, announced group %p",
           ext_workspace_manager_v1_ptr,
           ext_workspace_group_handle_v1_ptr);

    ext_workspace_group_handle_v1_add_listener(
        ext_workspace_group_handle_v1_ptr,
        &_wlmtool_workspace_group_listener,
        ws_state_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_handle_workspace(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1_ptr,
    struct ext_workspace_handle_v1 *workspace_ptr)
{
    struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_ERROR, "FIXME: %p - %p, workspace %p",
           ext_workspace_manager_v1_ptr, ws_state_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_handle_done(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1_ptr)
{
    struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_ERROR, "FIXME: %p - %p, done",
           ext_workspace_manager_v1_ptr, ws_state_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_handle_finished(
    void *data_ptr,
    struct ext_workspace_manager_v1 *ext_workspace_manager_v1_ptr)
{
    struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_ERROR, "FIXME: %p - %p, finished",
           ext_workspace_manager_v1_ptr, ws_state_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_capabilities(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    uint32_t capabilities)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p, capabilities %"PRIx32,
           ext_workspace_group_handle_v1_ptr, capabilities);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_output_enter(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct wl_output *wl_output_ptr)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p, output enter %p",
           ext_workspace_group_handle_v1_ptr, wl_output_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_output_leave(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct wl_output *wl_output_ptr)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p, output leave %p",
           ext_workspace_group_handle_v1_ptr, wl_output_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_workspace_enter(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct ext_workspace_handle_v1 *workspace_ptr)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p, workspace enter %p",
           ext_workspace_group_handle_v1_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_workspace_leave(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr,
    struct ext_workspace_handle_v1 *workspace_ptr)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p, workspace leave %p",
           ext_workspace_group_handle_v1_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_workspace_group_handle_removed(
    void *data_ptr,
    struct ext_workspace_group_handle_v1 *ext_workspace_group_handle_v1_ptr)
{
    __UNUSED__ struct wlmtool_workspace_state *ws_state_ptr = data_ptr;

    bs_log(BS_INFO, "Workspace group %p removed.",
           ext_workspace_group_handle_v1_ptr);
    ext_workspace_group_handle_v1_destroy(ext_workspace_group_handle_v1_ptr);
}

/* == Unit Tests =========================================================== */

/* == End of workspace.c =================================================== */
