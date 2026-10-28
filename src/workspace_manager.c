/* ========================================================================= */
/**
 * @file workspace_manager.c
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

#include "workspace_manager.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <libbase/libbase.h>
#include <wayland-server-core.h>

#include "ext-workspace-server-protocol.h"

struct wl_client;
struct wl_resource;

/* == Declarations ========================================================= */

/** Manages workspaces, and handles `ext-workspace-v1` protocol. */
struct wlmaker_workspace_manager {
    /** The global holding the workspace protocol interface. */
    struct wl_global          *wl_global_ptr;

    /** Clients, through @ref wlmaker_workspace_manager_client::dlnode. */
    bs_dllist_t               clients;
};

/** Resources associated to a client of the workspace manager. */
struct wlmaker_workspace_manager_client {
    /** Element of @ref wlmaker_workspace_manager::clients. */
    bs_dllist_node_t          dlnode;

    /** The associated resource. */
    struct wl_resource        *wl_resource_ptr;
    /** Back-link to the workspace manager. */
    struct wlmaker_workspace_manager *workspace_manager_ptr;
};

static void _wlmaker_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);
static void _wlmaker_workspace_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr);

static void _wlmaker_workspace_handle_commit(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlmaker_workspace_handle_stop(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

/* == Data ================================================================= */

static const struct ext_workspace_manager_v1_interface
_wlmaker_workspace_manager_interface = {
    .commit = _wlmaker_workspace_handle_commit,
    .stop = _wlmaker_workspace_handle_stop
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmaker_workspace_manager *wlmaker_workspace_manager_create(
    struct wl_display *wl_display_ptr)
{
    struct wlmaker_workspace_manager *workspace_manager_ptr =
        logged_calloc(1, sizeof(workspace_manager_ptr));

    workspace_manager_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &ext_workspace_manager_v1_interface,
        EXT_WORKSPACE_MANAGER_V1_WORKSPACE_SINCE_VERSION,
        workspace_manager_ptr,
        _wlmaker_workspace_manager_bind);
    if (NULL == workspace_manager_ptr->wl_global_ptr) {
        bs_log(BS_ERROR, "Failed wl_global_create for 'ext-workspace-v1'");
        wlmaker_workspace_manager_destroy(workspace_manager_ptr);
        return NULL;
    }


    return workspace_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_workspace_manager_destroy(
    struct wlmaker_workspace_manager *workspace_manager_ptr)
{
    if (NULL != workspace_manager_ptr->wl_global_ptr) {
        wl_global_destroy(workspace_manager_ptr->wl_global_ptr);
        workspace_manager_ptr->wl_global_ptr = NULL;
    }

    free(workspace_manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Binds a client implementing the workspace interface. */
void _wlmaker_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wlmaker_workspace_manager *workspace_manager_ptr = data_ptr;

    struct wlmaker_workspace_manager_client *client_ptr = logged_calloc(
        1, sizeof(*client_ptr));
    if (NULL == client_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
    client_ptr->workspace_manager_ptr = workspace_manager_ptr;

    client_ptr->wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &ext_workspace_manager_v1_interface,
        version, id);
    if (NULL == client_ptr->wl_resource_ptr) {
        free(client_ptr);
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }

    wl_resource_set_implementation(
        client_ptr->wl_resource_ptr,
        &_wlmaker_workspace_manager_interface,
        client_ptr,
        _wlmaker_workspace_handle_resource_destroy);
    bs_dllist_push_back(
        &workspace_manager_ptr->clients,
        &client_ptr->dlnode);

    bs_log(BS_INFO, "Workspace manager %p bound client %p on resource %p",
           workspace_manager_ptr, client_ptr, client_ptr->wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys the client's resources. */
void _wlmaker_workspace_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr)
{
    struct wlmaker_workspace_manager_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);

    bs_log(BS_INFO, "Workspace manager %p destroy client %p on resource %p",
           client_ptr->workspace_manager_ptr,
           client_ptr,
           client_ptr->wl_resource_ptr);

    bs_dllist_remove(
        &client_ptr->workspace_manager_ptr->clients,
        &client_ptr->dlnode);

    client_ptr->wl_resource_ptr = NULL;
    free(client_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmaker_workspace_handle_commit(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_ERROR, "FIXME: commit %p - %p", wl_client_ptr, wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmaker_workspace_handle_stop(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_ERROR, "FIXME: stop %p - %p", wl_client_ptr, wl_resource_ptr);
}

/* == Unit Tests =========================================================== */

/* == End of workspace_manager.c =========================================== */
