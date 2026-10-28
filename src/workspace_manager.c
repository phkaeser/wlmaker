/* ========================================================================= */
/**
 * @file workspace_manager.c
 *
 * TODO(kaeser@gubbe.ch):
 * * Update the workspace state for all clients  when workspace changes.
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

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "ext-workspace-server-protocol.h"
#include "toolkit/toolkit.h"

struct wl_client;
struct wl_resource;

/* == Declarations ========================================================= */

/** Manages workspaces, and handles `ext-workspace-v1` protocol. */
struct wlm_workspace_manager {
    /** The global holding the workspace protocol interface. */
    struct wl_global          *wl_global_ptr;

    /** Clients, through @ref _wlm_wsmgr_client::dlnode. */
    bs_dllist_t               clients;
    /** Workspaces, through @ref _wlm_wsmgr_workspace::dlnode. */
    bs_dllist_t               workspaces;
    /** Tracks all outputs, so they can be reported to clients. */
    wlmtk_output_tracker_t    *output_tracker_ptr;

    // -- Elemeents below not owned by workspace manager ------------

    /** The output manager, for reporting outputs per workspace group. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** The desktop, for adding/removing workspaces. */
    wlmtk_desktop_t           *desktop_ptr;
};

/** Workspace managed by the workspace manager. */
struct _wlm_wsmgr_workspace {
    /** Element of @ref wlm_workspace_manager::workspaces. */
    bs_dllist_node_t          dlnode;
    /** The actual @ref wlmtk_workspace_t connected here. */
    wlmtk_workspace_t         *wlmtk_workspace_ptr;
    /** ID of the workspace. We use 'workspaceNN' for NN in [1, 99]. */
    char                      *id_ptr;
    /**
     * List of workspace handles across the connected clients. Linked through
     * @ref _wlm_wsmgr_workspace_handle::workspace_dlnode.
     */
    bs_dllist_t               workspace_handles;
};

/** Resources associated to a client of the workspace manager. */
struct _wlm_wsmgr_client {
    /** Element of @ref wlm_workspace_manager::clients. */
    bs_dllist_node_t          dlnode;
    /** The associated client resource for the workspace manager. */
    struct wl_resource        *manager_wl_resource_ptr;
    /** The resource for the workspace group. We support one per client only. */
    struct wl_resource        *workspace_group_wl_resource_ptr;
    /**
     * List of workspace handles for this client. Linkes through
     * @ref _wlm_wsmgr_workspace_handle::client_dlnode.
     */
    bs_dllist_t               workspace_handles;

    /** Whether the client sent "finished" (no further updates be sent). */
    bool                      finished;

    // -- Elements below are not owned by the client ----------------

    /** Back-link to the workspace manager. */
    struct wlm_workspace_manager *workspace_manager_ptr;
    /** The actual client connected here. */
    struct wl_client          *wl_client_ptr;
};

/** Workspace handle for a given client and workspace. */
struct _wlm_wsmgr_workspace_handle {
    /** Element of client, through @ref _wlm_wsmgr_client::workspace_handles. */
    bs_dllist_node_t          client_dlnode;
    /** The workspace, through @ref _wlm_wsmgr_workspace::workspace_handles. */
    bs_dllist_node_t          workspace_dlnode;
    /** The wayland resource for this client and workspace's handle. */
    struct wl_resource        *wl_resource_ptr;

    // -- Elements below are not owned by the workspace handle ------

    /** The @ref _wlm_wsmgr_client this handle relates to. NULL if none. */
    struct _wlm_wsmgr_client  *client_ptr;
    /** The @ref _wlm_wsmgr_workspace this handle relates to. NULL if none. */
    struct _wlm_wsmgr_workspace *workspace_ptr;
};

/** Holds the configuration from the state dict. */
struct _wlm_wsmgr_workspace_state_config {
    /** Workspace name. */
    char            name[32];
};

static void _wlm_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);
static void _wlm_workspace_manager_add_client(
    struct wlm_workspace_manager *workspace_manager_ptr,
    struct _wlm_wsmgr_client *client_ptr);
static void _wlm_workspace_manager_remove_client(
    struct wlm_workspace_manager *workspace_manager_ptr,
    struct _wlm_wsmgr_client *client_ptr);
static void *_wlm_workspace_manager_output_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);
static void _wlm_workspace_manager_output_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

static struct _wlm_wsmgr_workspace *_wlm_wsmgr_workspace_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr,
    size_t id);
static void _wlm_wsmgr_workspace_destroy(
    struct _wlm_wsmgr_workspace *workspace_ptr);
static void _wlm_wsmgr_workspace_destroy_from_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static struct _wlm_wsmgr_client *_wlm_wsmgr_client_create(
    struct wl_client *wl_client_ptr,
    uint32_t version,
    uint32_t id);
static void _wlm_wsmgr_client_destroy(
    struct _wlm_wsmgr_client *client_ptr);
static void _wlm_wsmgr_client_destroy_from_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlm_wsmgr_client_destroy_from_resource(
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_client_destroy_from_group_resource(
    struct wl_resource *wl_resource_ptr);
struct _wlm_wsmgr_client *_wlm_wsmgr_client_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);
static void _wlm_wsmgr_client_finish(
    struct _wlm_wsmgr_client *client_ptr);
static void _wlm_wsmgr_client_output_do(
    struct _wlm_wsmgr_client *client_ptr,
    struct wlr_output *wlr_output_ptr,
    void (*func)(struct wl_resource *res_ptr, struct wl_resource *output_ptr));
static void _wlm_wsmgr_client_output_tracker_iterator_enter(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr,
    void *arg_ptr);
static void _wlm_wsmgr_client_dlnode_output_enter(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlm_wsmgr_client_dlnode_output_leave(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlm_wsmgr_client_dlnode_done(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static void _wlm_wsmgr_client_manager_handle_commit(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_client_manager_handle_stop(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

static void _wlm_wsmgr_client_group_handle_create_workspace(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    const char *workspace_name_ptr);
static void _wlm_wsmgr_client_group_handle_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

static struct _wlm_wsmgr_workspace_handle *_wlm_wsmgr_workspace_handle_create(
    struct _wlm_wsmgr_client *client_ptr,
    struct _wlm_wsmgr_workspace *workspace_ptr);
static void _wlm_wsmgr_workspace_handle_destroy(
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr);
static void _wlm_wsmgr_workspace_handle_destroy_from_client_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlm_wsmgr_workspace_handle_destroy_from_workspace_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlm_wsmgr_workspace_handle_destroy_from_resource(
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_workspace_handle_send_state(
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr);

static void _wlm_wsmgr_workspace_handle_handle_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_workspace_handle_handle_activate(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_workspace_handle_handle_deactivate(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlm_wsmgr_workspace_handle_handle_assign(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    struct wl_resource *workspace_group_ptro);
static void _wlm_wsmgr_workspace_handle_handle_remove(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

/* == Data ================================================================= */

/** Style descriptor for the "Workspace" dict of wlmaker-state.plist. */
static const bspl_desc_t _wlm_wsmgr_workspace_state_config_desc[] = {
    BSPL_DESC_CHARBUF(
        "Name", true, struct _wlm_wsmgr_workspace_state_config, name, name, 32, NULL),
    BSPL_DESC_SENTINEL()
};

/** Lookup table for ext-workspace-manager-v1 method implementation. */
static const struct ext_workspace_manager_v1_interface
_wlm_workspace_manager_interface = {
    .commit = _wlm_wsmgr_client_manager_handle_commit,
    .stop = _wlm_wsmgr_client_manager_handle_stop
};

/** Lookup table for ext-workspace-group-handle-v1 method implementation. */
static const struct ext_workspace_group_handle_v1_interface
_wlm_workspace_group_interface = {
    .create_workspace = _wlm_wsmgr_client_group_handle_create_workspace,
    .destroy = _wlm_wsmgr_client_group_handle_destroy,
};

/** Handlers for `ext-workspace-v1` `ext_workspace_handle_v1` methods. */
static const struct ext_workspace_handle_v1_interface
_wlm_workspace_interface = {
    .destroy = _wlm_wsmgr_workspace_handle_handle_destroy,
    .activate = _wlm_wsmgr_workspace_handle_handle_activate,
    .deactivate = _wlm_wsmgr_workspace_handle_handle_deactivate,
    .assign =  _wlm_wsmgr_workspace_handle_handle_assign,
    .remove = _wlm_wsmgr_workspace_handle_handle_remove
};

/** Maximum number of workspaces supported. */
static const unsigned _wlm_wsmgr_max_workspaces = 99;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlm_workspace_manager *wlm_workspace_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_desktop_t *desktop_ptr)
{
    struct wlm_workspace_manager *workspace_manager_ptr =
        logged_calloc(1, sizeof(*workspace_manager_ptr));
    workspace_manager_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    workspace_manager_ptr->desktop_ptr = desktop_ptr;

    workspace_manager_ptr->output_tracker_ptr = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        workspace_manager_ptr,
        _wlm_workspace_manager_output_create,
        NULL /* update */,
        _wlm_workspace_manager_output_destroy);
    if (NULL == workspace_manager_ptr) {
        wlm_workspace_manager_destroy(workspace_manager_ptr);
        return NULL;
    }

    workspace_manager_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &ext_workspace_manager_v1_interface,
        EXT_WORKSPACE_MANAGER_V1_WORKSPACE_SINCE_VERSION,
        workspace_manager_ptr,
        _wlm_workspace_manager_bind);
    if (NULL == workspace_manager_ptr->wl_global_ptr) {
        bs_log(BS_ERROR, "Failed wl_global_create for 'ext-workspace-v1'");
        wlm_workspace_manager_destroy(workspace_manager_ptr);
        return NULL;
    }

    return workspace_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlm_workspace_manager_destroy(
    struct wlm_workspace_manager *workspace_manager_ptr)
{
    bs_dllist_for_each(
        &workspace_manager_ptr->workspaces,
        _wlm_wsmgr_workspace_destroy_from_dlnode,
        NULL);
    workspace_manager_ptr->workspaces = (bs_dllist_t){};

    bs_dllist_for_each(
        &workspace_manager_ptr->clients,
        _wlm_wsmgr_client_destroy_from_dlnode,
        NULL);
    workspace_manager_ptr->clients = (bs_dllist_t){};

    if (NULL != workspace_manager_ptr->wl_global_ptr) {
        wl_global_destroy(workspace_manager_ptr->wl_global_ptr);
        workspace_manager_ptr->wl_global_ptr = NULL;
    }

    if (NULL != workspace_manager_ptr->output_tracker_ptr) {
        wlmtk_output_tracker_destroy(
            workspace_manager_ptr->output_tracker_ptr);
        workspace_manager_ptr->output_tracker_ptr = NULL;
    }

    free(workspace_manager_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlm_workspace_manager_apply_state(
    struct wlm_workspace_manager *workspace_manager_ptr,
    bspl_dict_t *state_dict_ptr)
{
    bspl_array_t *array_ptr = bspl_dict_get_array(
        state_dict_ptr, "Workspaces");
    if (NULL == array_ptr) {
        bs_log(BS_ERROR, "No \"Workspaces\" found in state dict.");
        return false;
    }

    for (size_t i = 0; i < bspl_array_size(array_ptr); ++i) {
        bspl_dict_t *d = bspl_dict_from_object(
            bspl_array_at(array_ptr, i));
        if (NULL == d) {
            bs_log(BS_ERROR, "Element %zu in \"Workspaces\" not a dict", i);
            return false;
        }

        struct _wlm_wsmgr_workspace_state_config c = {};
        if (!bspl_decode_dict(d, _wlm_wsmgr_workspace_state_config_desc, &c)) {
            bs_log(BS_ERROR,
                   "Failed decoding element %zu in \"Workspaces\"", i);
            return false;
        }

        if (!wlm_workspace_manager_add_workspace(
                workspace_manager_ptr, c.name)) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlm_workspace_manager_add_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr,
    const char *name_ptr)
{
    if (_wlm_wsmgr_max_workspaces <=
        bs_dllist_size(&workspace_manager_ptr->workspaces)) {
        bs_log(BS_WARNING, "Already 99 workspaces registered.");
        return false;
    }

    struct _wlm_wsmgr_workspace *workspace_ptr = _wlm_wsmgr_workspace_create(
        workspace_manager_ptr->wlr_output_layout_ptr,
        name_ptr,
        bs_dllist_size(&workspace_manager_ptr->workspaces) + 1);
    if (NULL == workspace_ptr) return false;

    wlmtk_desktop_add_workspace(
        workspace_manager_ptr->desktop_ptr,
        workspace_ptr->wlmtk_workspace_ptr);
    bs_dllist_push_back(
        &workspace_manager_ptr->workspaces,
        &workspace_ptr->dlnode);

    // Add the workspace to each already-connected client.
    for (bs_dllist_node_t *node_ptr = workspace_manager_ptr->clients.head_ptr;
         node_ptr != NULL;
         node_ptr = bs_dllist_node_iterator_forward(node_ptr)) {
        struct _wlm_wsmgr_client *client_ptr =
            _wlm_wsmgr_client_from_dlnode(node_ptr);
        struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr =
            _wlm_wsmgr_workspace_handle_create(client_ptr, workspace_ptr);
        if (NULL == workspace_handle_ptr) continue;
    }

    bs_dllist_for_each(
        &workspace_manager_ptr->clients,
        _wlm_wsmgr_client_dlnode_done,
        NULL);
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlm_workspace_manager_destroy_last_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr)
{
    struct _wlm_wsmgr_workspace *workspace_ptr = BS_CONTAINER_OF(
        workspace_manager_ptr->workspaces.tail_ptr,
        struct _wlm_wsmgr_workspace,
        dlnode);
    wlmtk_workspace_t *curr_ws_ptr = wlmtk_desktop_get_current_workspace(
        workspace_manager_ptr->desktop_ptr);
    if (NULL == workspace_ptr ||
        workspace_ptr->wlmtk_workspace_ptr == curr_ws_ptr) return false;

    bs_dllist_node_t *dlnode_ptr = bs_dllist_pop_back(
        &workspace_manager_ptr->workspaces);
    BS_ASSERT(dlnode_ptr == &workspace_ptr->dlnode);
    _wlm_wsmgr_workspace_destroy(workspace_ptr);

    bs_dllist_for_each(
        &workspace_manager_ptr->clients,
        _wlm_wsmgr_client_dlnode_done,
        NULL);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Binds a client implementing the workspace interface. */
void _wlm_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wlm_workspace_manager *workspace_manager_ptr = data_ptr;

    struct _wlm_wsmgr_client *client_ptr =
        _wlm_wsmgr_client_create(wl_client_ptr, version, id);
    if (NULL == client_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
    _wlm_workspace_manager_add_client(
        workspace_manager_ptr, client_ptr);
    _wlm_wsmgr_client_dlnode_done(&client_ptr->dlnode, NULL);
}

/* ------------------------------------------------------------------------- */
/** Adds a client to the workspace manager. Creates the workspace handles. */
void _wlm_workspace_manager_add_client(
    struct wlm_workspace_manager *workspace_manager_ptr,
    struct _wlm_wsmgr_client *client_ptr)
{
    BS_ASSERT(NULL == client_ptr->workspace_manager_ptr);
    client_ptr->workspace_manager_ptr = workspace_manager_ptr;
    bs_dllist_push_back(
        &workspace_manager_ptr->clients,
        &client_ptr->dlnode);

    // Adds a workspace handle for each of already-existing workspaces.
    for (bs_dllist_node_t *node_ptr = workspace_manager_ptr->workspaces.head_ptr;
         node_ptr != NULL;
         node_ptr = bs_dllist_node_iterator_forward(node_ptr)) {
        struct _wlm_wsmgr_workspace *workspace_ptr = BS_CONTAINER_OF(
            node_ptr, struct _wlm_wsmgr_workspace, dlnode);
        struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr =
            _wlm_wsmgr_workspace_handle_create(client_ptr, workspace_ptr);
        if (NULL == workspace_handle_ptr) continue;
    }

    wlmtk_output_tracker_for_each(
        workspace_manager_ptr->output_tracker_ptr,
        _wlm_wsmgr_client_output_tracker_iterator_enter,
        client_ptr);
}

/* ------------------------------------------------------------------------- */
/** Removes the client, and destroys the relevant workspace handles. */
void _wlm_workspace_manager_remove_client(
    struct wlm_workspace_manager *workspace_manager_ptr,
    struct _wlm_wsmgr_client *client_ptr)
{
    BS_ASSERT(workspace_manager_ptr == client_ptr->workspace_manager_ptr);

    bs_dllist_for_each(
        &client_ptr->workspace_handles,
        _wlm_wsmgr_workspace_handle_destroy_from_client_dlnode,
        NULL);
    client_ptr->workspace_handles = (bs_dllist_t){};

    bs_dllist_remove(
        &workspace_manager_ptr->clients,
        &client_ptr->dlnode);
    client_ptr->workspace_manager_ptr = NULL;

    if (NULL != client_ptr->manager_wl_resource_ptr) {
        ext_workspace_manager_v1_send_done(client_ptr->manager_wl_resource_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles new output: Call `output_enter` for all clients. */
void *_wlm_workspace_manager_output_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    struct wlm_workspace_manager *workspace_manager_ptr = ud_ptr;
    bs_dllist_for_each(
        &workspace_manager_ptr->clients,
        _wlm_wsmgr_client_dlnode_output_enter,
        wlr_output_ptr);
    return workspace_manager_ptr;
}

/* ------------------------------------------------------------------------- */
/** Handles removal of an output: Call `output_leave` for all clients. */
void _wlm_workspace_manager_output_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    struct wlm_workspace_manager *workspace_manager_ptr = ud_ptr;
    BS_ASSERT(workspace_manager_ptr == output_ptr);
    bs_dllist_for_each(
        &workspace_manager_ptr->clients,
        _wlm_wsmgr_client_dlnode_output_leave,
        wlr_output_ptr);
}

/* == Methods on struct _wlm_wsmgr_workpace ================================ */

/* ------------------------------------------------------------------------- */
/** Creates the workspace. */
struct _wlm_wsmgr_workspace *_wlm_wsmgr_workspace_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr,
    size_t id)
{
    struct _wlm_wsmgr_workspace *workspace_ptr = logged_calloc(
        1, sizeof(*workspace_ptr));
    if (NULL == workspace_ptr) return NULL;

    workspace_ptr->id_ptr = bs_strdupf("workspace%02zu", id);
    if (NULL == workspace_ptr->id_ptr) {
        bs_log(BS_ERROR, "Failed bs_strdupf(\"workspace\%%02zu\", %zu)", id);
        _wlm_wsmgr_workspace_destroy(workspace_ptr);
        return NULL;
    }

    struct wlmtk_tile_style ts = {};
    workspace_ptr->wlmtk_workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, name_ptr, &ts);
    if (NULL == workspace_ptr->wlmtk_workspace_ptr) {
        _wlm_wsmgr_workspace_destroy(workspace_ptr);
        return NULL;
    }
    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the workspace. */
void _wlm_wsmgr_workspace_destroy(struct _wlm_wsmgr_workspace *workspace_ptr)
{
    if (NULL != workspace_ptr->wlmtk_workspace_ptr) {
        wlmtk_desktop_remove_workspace(
            wlmtk_workspace_get_desktop(workspace_ptr->wlmtk_workspace_ptr),
            workspace_ptr->wlmtk_workspace_ptr);
        wlmtk_workspace_destroy(workspace_ptr->wlmtk_workspace_ptr);
        workspace_ptr->wlmtk_workspace_ptr = NULL;
    }

    if (NULL != workspace_ptr->id_ptr) {
        free(workspace_ptr->id_ptr);
        workspace_ptr->id_ptr = NULL;
    }

    bs_dllist_for_each(
        &workspace_ptr->workspace_handles,
        _wlm_wsmgr_workspace_handle_destroy_from_workspace_dlnode,
        NULL);
    workspace_ptr->workspace_handles = (bs_dllist_t){};

    free(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys @ref _wlm_wsmgr_workspace, from it's dlnode. */
void _wlm_wsmgr_workspace_destroy_from_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct _wlm_wsmgr_workspace *workspace_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct _wlm_wsmgr_workspace, dlnode);
    _wlm_wsmgr_workspace_destroy(workspace_ptr);
}

/* == Methods for struct _wlm_wsmgr_client ================================= */

/* ------------------------------------------------------------------------- */
/** Creates a client and sets up the associated wayland resources. */
struct _wlm_wsmgr_client *_wlm_wsmgr_client_create(
    struct wl_client *wl_client_ptr,
    uint32_t version,
    uint32_t id)
{
    struct _wlm_wsmgr_client *client_ptr = logged_calloc(
        1, sizeof(*client_ptr));
    if (NULL == client_ptr) return NULL;
    client_ptr->wl_client_ptr = wl_client_ptr;

    // Create the `ext_workspace_manager_v1` resource for this client.
    if (NULL != wl_client_ptr) {
        client_ptr->manager_wl_resource_ptr = wl_resource_create(
            wl_client_ptr,
            &ext_workspace_manager_v1_interface,
            version, id);
        if (NULL == client_ptr->manager_wl_resource_ptr) {
            _wlm_wsmgr_client_destroy(client_ptr);
            return NULL;
        }
        wl_resource_set_implementation(
            client_ptr->manager_wl_resource_ptr,
            &_wlm_workspace_manager_interface,
            client_ptr,
            _wlm_wsmgr_client_destroy_from_resource);

        // Create the `ext_workspace_group_handle_v1` resource for this client.
        client_ptr->workspace_group_wl_resource_ptr = wl_resource_create(
            wl_client_ptr,
            &ext_workspace_group_handle_v1_interface,
            version, 0 /* create new ID */ );
        if (NULL == client_ptr->workspace_group_wl_resource_ptr) {
            _wlm_wsmgr_client_destroy(client_ptr);
            return NULL;
        }
        wl_resource_set_implementation(
            client_ptr->workspace_group_wl_resource_ptr,
            &_wlm_workspace_group_interface,
            client_ptr,
            _wlm_wsmgr_client_destroy_from_group_resource);

        // Setup for the client:
        // (1) Send the one workspace group,
        // (2) Set the workspace group's capabilities,
        ext_workspace_manager_v1_send_workspace_group(
            client_ptr->manager_wl_resource_ptr,
            client_ptr->workspace_group_wl_resource_ptr);
        ext_workspace_group_handle_v1_send_capabilities(
            client_ptr->workspace_group_wl_resource_ptr, 0);
    }

    bs_log(BS_INFO, "Workspace manager created client %p (v%"PRIu32") with "
           "resource %p (groupo %p) at %p",  wl_client_ptr, version,
           client_ptr->manager_wl_resource_ptr,
           client_ptr->workspace_group_wl_resource_ptr, client_ptr);
    return client_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the client. */
void _wlm_wsmgr_client_destroy(
    struct _wlm_wsmgr_client *client_ptr)
{
    bs_log(BS_INFO, "Workspace manager destroy client%p for resource %p",
           client_ptr, client_ptr->manager_wl_resource_ptr);

    if (NULL != client_ptr->workspace_group_wl_resource_ptr) {
        ext_workspace_group_handle_v1_send_removed(
            client_ptr->workspace_group_wl_resource_ptr);
        wl_resource_set_user_data(
            client_ptr->workspace_group_wl_resource_ptr, NULL);
        wl_resource_destroy(client_ptr->workspace_group_wl_resource_ptr);
        client_ptr->workspace_group_wl_resource_ptr = NULL;
    }

    _wlm_wsmgr_client_finish(client_ptr);

    if (NULL != client_ptr->manager_wl_resource_ptr) {
        wl_resource_set_user_data(client_ptr->manager_wl_resource_ptr, NULL);
        wl_resource_destroy(client_ptr->manager_wl_resource_ptr);
        client_ptr->manager_wl_resource_ptr = NULL;
    }

    free(client_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys the client, from @ref _wlm_wsmgr_client::dlnode. */
void _wlm_wsmgr_client_destroy_from_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct _wlm_wsmgr_client *client_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct _wlm_wsmgr_client, dlnode);
    _wlm_wsmgr_client_destroy(client_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys the client, triggered from the wl_resource dtor. */
void _wlm_wsmgr_client_destroy_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    if (NULL == client_ptr) return;

    client_ptr->manager_wl_resource_ptr = NULL;

    _wlm_workspace_manager_remove_client(
        client_ptr->workspace_manager_ptr, client_ptr);
    _wlm_wsmgr_client_destroy(client_ptr);
}

/* ------------------------------------------------------------------------- */
/** The workspace group resource is destroyed. Remove the reference. */
static void _wlm_wsmgr_client_destroy_from_group_resource(
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    if (NULL == client_ptr) return;

    client_ptr->workspace_group_wl_resource_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Returns the @ref _wlm_wsmgr_client holding `dlnode_ptr`. */
struct _wlm_wsmgr_client *_wlm_wsmgr_client_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(
        dlnode_ptr,
        struct _wlm_wsmgr_client,
        dlnode);
}

/* ------------------------------------------------------------------------- */
/** Finish: Clears the workspace handles and marks the client as finished. */
void _wlm_wsmgr_client_finish(struct _wlm_wsmgr_client *client_ptr)
{
    // No further updates desired: Clear all the workspace handles.
    bs_dllist_for_each(
        &client_ptr->workspace_handles,
        _wlm_wsmgr_workspace_handle_destroy_from_client_dlnode,
        NULL);
    client_ptr->workspace_handles = (bs_dllist_t){};

    if (!client_ptr->finished &&
        NULL != client_ptr->manager_wl_resource_ptr) {
        ext_workspace_manager_v1_send_finished(
            client_ptr->manager_wl_resource_ptr);
    }
    client_ptr->finished = true;
}

/* ------------------------------------------------------------------------- */
/** Runs `func` for the output's resource for this client, if available. */
void _wlm_wsmgr_client_output_do(
    struct _wlm_wsmgr_client *client_ptr,
    struct wlr_output *wlr_output_ptr,
    void (*func)(struct wl_resource *res_ptr, struct wl_resource *output_ptr))
{
    // Guard clause: Already finished, or not initialized.
    if (client_ptr->finished ||
        NULL == client_ptr->workspace_group_wl_resource_ptr) return;

    // Find the output resource for the particular client. It may not exist.
    struct wl_resource *output_wl_resource_ptr = NULL, *res_ptr;
    wl_resource_for_each(res_ptr, &wlr_output_ptr->resources) {
        if (wl_resource_get_client(res_ptr) == client_ptr->wl_client_ptr) {
            output_wl_resource_ptr = res_ptr;
            break;
        }
    }
    if (NULL == output_wl_resource_ptr) return;

    func(client_ptr->workspace_group_wl_resource_ptr, output_wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** @ref wlmtk_output_tracker_for_each callback. Send `output_enter`. */
void _wlm_wsmgr_client_output_tracker_iterator_enter(
    struct wlr_output *wlr_output_ptr,
    __UNUSED__ void *ud_ptr,
    __UNUSED__ void *output_ptr,
    void *arg_ptr)
{
    _wlm_wsmgr_client_output_do(
        arg_ptr,
        wlr_output_ptr,
        ext_workspace_group_handle_v1_send_output_enter);
}

/* ------------------------------------------------------------------------- */
/** Sends `output_enter` for this client and the `struct wlr_output*`. */
void _wlm_wsmgr_client_dlnode_output_enter(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    _wlm_wsmgr_client_output_do(
        _wlm_wsmgr_client_from_dlnode(dlnode_ptr),
        ud_ptr,
        ext_workspace_group_handle_v1_send_output_enter);
}

/* ------------------------------------------------------------------------- */
/** Sends `output_leave` for this client and the `struct wlr_output*`. */
void _wlm_wsmgr_client_dlnode_output_leave(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    _wlm_wsmgr_client_output_do(
        _wlm_wsmgr_client_from_dlnode(dlnode_ptr),
        ud_ptr,
        ext_workspace_group_handle_v1_send_output_leave);
}

/* ------------------------------------------------------------------------- */
/** Sends `done` for this client's workspace manager. */
void _wlm_wsmgr_client_dlnode_done(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct _wlm_wsmgr_client *client_ptr =
        _wlm_wsmgr_client_from_dlnode(dlnode_ptr);
    if (NULL != client_ptr->manager_wl_resource_ptr) {
        ext_workspace_manager_v1_send_done(
            client_ptr->manager_wl_resource_ptr);
    }

}

/* ------------------------------------------------------------------------- */
/** The client commits: All updates shall be applied now. */
void _wlm_wsmgr_client_manager_handle_commit(
    struct wl_client *wl_client_ptr,
    __UNUSED__ struct wl_resource *wl_resource_ptr)
{
    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_INFO, "Client %p requests commit.", wl_client_ptr);
}

/* ------------------------------------------------------------------------- */
/** The client indicates it no longer desires to receive updates. */
void _wlm_wsmgr_client_manager_handle_stop(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    if (NULL == client_ptr) return;
    _wlm_wsmgr_client_finish(client_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Requets to create a workspace. Currently not supported for `wlmaker`.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param workspace_name_ptr
 */
void _wlm_wsmgr_client_group_handle_create_workspace(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    const char *workspace_name_ptr)
{
    bs_log(BS_WARNING, "Client %p request workspace \"%s\", ignored.",
           wl_client_ptr, workspace_name_ptr);
    wl_resource_post_error(wl_resource_ptr, 0, "Not a supported capability");
}

/* ------------------------------------------------------------------------- */
/**
 * Destroy the workspace group handle.
 *
 * Sent by the client when not wanting to use the workspace group handle any
 * longer. Clears the reference to the associated `wl_resource`.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 */
void _wlm_wsmgr_client_group_handle_destroy(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    client_ptr->workspace_group_wl_resource_ptr = NULL;
}

/* == Workspace handle methods ============================================= */

/* ------------------------------------------------------------------------- */
/**
 * Creates the workspace handle for the given client and workspace.
 *
 * @param client_ptr
 * @param workspace_ptr
 *
 * @return Workspace handle for this client and workspace, or NULL on error.
 */
struct _wlm_wsmgr_workspace_handle *_wlm_wsmgr_workspace_handle_create(
    struct _wlm_wsmgr_client *client_ptr,
    struct _wlm_wsmgr_workspace *workspace_ptr)
{
    if (client_ptr->finished) return NULL;

    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr =
        logged_calloc(1, sizeof(*workspace_handle_ptr));
    if (NULL == workspace_handle_ptr) return NULL;
    workspace_handle_ptr->workspace_ptr = workspace_ptr;

    if (NULL != client_ptr->wl_client_ptr) {
        workspace_handle_ptr->wl_resource_ptr = wl_resource_create(
            client_ptr->wl_client_ptr,
            &ext_workspace_handle_v1_interface,
            1 /* version */,
            0 /* create new ID */ );
        if (NULL == workspace_handle_ptr->wl_resource_ptr) {
            _wlm_wsmgr_workspace_handle_destroy(workspace_handle_ptr);
            return NULL;
        }
        wl_resource_set_implementation(
            workspace_handle_ptr->wl_resource_ptr,
            &_wlm_workspace_interface,
            workspace_handle_ptr,
            _wlm_wsmgr_workspace_handle_destroy_from_resource);

        // Announce the workspace handle to this client's workspace manager.
        ext_workspace_manager_v1_send_workspace(
            client_ptr->manager_wl_resource_ptr,
            workspace_handle_ptr->wl_resource_ptr);

        // Supported: Currently nothing.
        // TODO(kaeser@gubbe.ch): Add wiring and support for
        // EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE.
        ext_workspace_handle_v1_send_capabilities(
            workspace_handle_ptr->wl_resource_ptr, 0);
    }

    // Link it up with both the client and the workspace.
    bs_dllist_push_back(
        &client_ptr->workspace_handles,
        &workspace_handle_ptr->client_dlnode);
    workspace_handle_ptr->client_ptr = client_ptr;
    bs_dllist_push_back(
        &workspace_ptr->workspace_handles,
        &workspace_handle_ptr->workspace_dlnode);
    workspace_handle_ptr->workspace_ptr = workspace_ptr;

    _wlm_wsmgr_workspace_handle_send_state(workspace_handle_ptr);
    return workspace_handle_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the workspace handle. */
void _wlm_wsmgr_workspace_handle_destroy(
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr)
{
    // It it was linkeed up, remove from both client and workspace.
    if (NULL != workspace_handle_ptr->client_ptr) {
        bs_dllist_remove(
            &workspace_handle_ptr->client_ptr->workspace_handles,
            &workspace_handle_ptr->client_dlnode);
        workspace_handle_ptr->client_ptr = NULL;
    }
    if (NULL != workspace_handle_ptr->workspace_ptr) {
        bs_dllist_remove(
            &workspace_handle_ptr->workspace_ptr->workspace_handles,
            &workspace_handle_ptr->workspace_dlnode);
        workspace_handle_ptr->workspace_ptr = NULL;
    }

    // Announce the removal to the client, then clean up the resource.
    if (NULL != workspace_handle_ptr->wl_resource_ptr) {
        ext_workspace_handle_v1_send_removed(
            workspace_handle_ptr->wl_resource_ptr);
        wl_resource_set_user_data(workspace_handle_ptr->wl_resource_ptr, NULL);
        wl_resource_destroy(workspace_handle_ptr->wl_resource_ptr);
        workspace_handle_ptr->wl_resource_ptr = NULL;
    }

    free(workspace_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys workspace handle, identified from the client's iterator.
 *
 * @param dlnode_ptr Point to @ref _wlm_wsmgr_workspace_handle::client_dlnode.
 * @param ud_ptr
 */
void _wlm_wsmgr_workspace_handle_destroy_from_client_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct _wlm_wsmgr_workspace_handle, client_dlnode);
    _wlm_wsmgr_workspace_handle_destroy(workspace_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys workspace handle, identified from the workspace's iterator.
 *
 * @param dlnode_ptr Point to @ref _wlm_wsmgr_workspace_handle::workspace_dlnode.
 * @param ud_ptr
 */
void _wlm_wsmgr_workspace_handle_destroy_from_workspace_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct _wlm_wsmgr_workspace_handle, workspace_dlnode);
    _wlm_wsmgr_workspace_handle_destroy(workspace_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys handle, from @ wlm_wsmgr_workspace_handle::wl_resource_ptr. */
void _wlm_wsmgr_workspace_handle_destroy_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    if (NULL == workspace_handle_ptr) return;

    workspace_handle_ptr->wl_resource_ptr = NULL;
    _wlm_wsmgr_workspace_handle_destroy(workspace_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Sends the workspace's state to this workspace handle. */
void _wlm_wsmgr_workspace_handle_send_state(
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr)
{
    if (NULL == workspace_handle_ptr->wl_resource_ptr) return;

    const char *name_ptr;
    int index;
    wlmtk_workspace_get_details(
        workspace_handle_ptr->workspace_ptr->wlmtk_workspace_ptr,
        &name_ptr, &index);

    ext_workspace_handle_v1_send_id(
        workspace_handle_ptr->wl_resource_ptr,
        workspace_handle_ptr->workspace_ptr->id_ptr);
    ext_workspace_handle_v1_send_name(
        workspace_handle_ptr->wl_resource_ptr,
        name_ptr != NULL ? name_ptr : "");

    int32_t coordinate = index;
    struct wl_array coordinates_array = {
        .size = sizeof(coordinate),
        .alloc = sizeof(coordinate),
        .data = &coordinate
    };
    ext_workspace_handle_v1_send_coordinates(
        workspace_handle_ptr->wl_resource_ptr,
        &coordinates_array);

    uint32_t state = 0;
    if (wlmtk_workspace_enabled(
            workspace_handle_ptr->workspace_ptr->wlmtk_workspace_ptr)) {
        state = EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
    }
    ext_workspace_handle_v1_send_state(
        workspace_handle_ptr->wl_resource_ptr, state);
}

/* ------------------------------------------------------------------------- */
/** The client no longer wants to use the workspace handle object. */
void _wlm_wsmgr_workspace_handle_handle_destroy(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    struct _wlm_wsmgr_workspace_handle *workspace_handle_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    if (NULL == workspace_handle_ptr) return;
    _wlm_wsmgr_workspace_handle_destroy(workspace_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** The client requests to activate the workspace. Support pending. */
void _wlm_wsmgr_workspace_handle_handle_activate(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_WARNING, "Client %p requests to activate workspace handle %p",
           wl_client_ptr, wl_resource_get_user_data(wl_resource_ptr));
}

/* ------------------------------------------------------------------------- */
/** The client requests to deactivate the workspace. Not supported. */
void _wlm_wsmgr_workspace_handle_handle_deactivate(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_INFO, "Ignoring client %p request to deactivate workspace %p",
           wl_client_ptr, wl_resource_get_user_data(wl_resource_ptr));
}

/* ------------------------------------------------------------------------- */
/** Client requests to assign the workspace to a group. Not supported. */
void _wlm_wsmgr_workspace_handle_handle_assign(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    struct wl_resource *workspace_group_ptr)
{
    bs_log(BS_INFO, "Ignoring client %p request to assign workspace %p to "
           "group resource %p", wl_client_ptr,
           wl_resource_get_user_data(wl_resource_ptr),
           workspace_group_ptr);
}

/* ------------------------------------------------------------------------- */
/** Client requests this workspace be removed (destroyed). Not supported. */
void _wlm_wsmgr_workspace_handle_handle_remove(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_INFO, "Ignoring client %p request to remove workspace %p",
           wl_client_ptr,
           wl_resource_get_user_data(wl_resource_ptr));
}

/* == Unit Tests =========================================================== */

/** Context for unit tests. */
struct _wlm_wsmgr_test_context {
    /** Wayland display. */
    struct wl_display         *wl_display_ptr;
    /** Wlroots output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Wlroots scene. */
    struct wlr_scene          *wlr_scene_ptr;
    /** The toolkit desktop, for attaching the workspaces. */
    wlmtk_desktop_t           *desktop_ptr;
    /** One output to add here. */
    struct wlr_output         output;
};

static void *_wlm_wsmgr_test_setup(void);
static void _wlm_wsmgr_test_teardown(void *ctx_ptr);

static void _wlm_workspace_manager_test_workspaces(bs_test_t *test_ptr);
static void _wlm_workspace_manager_test_client(bs_test_t *test_ptr);

/** Unit test cases for workspace manager. */
static const bs_test_case_t _wlm_workspace_manager_test_cases[] = {
    { true, "workspaces", _wlm_workspace_manager_test_workspaces },
    { true, "client", _wlm_workspace_manager_test_client },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlm_workspace_manager_test_set = BS_TEST_SET_CONTEXT(
    true, "workspace_manager", _wlm_workspace_manager_test_cases,
    _wlm_wsmgr_test_setup, _wlm_wsmgr_test_teardown);

/* ------------------------------------------------------------------------- */
/** Creates objects of the @ref _wlm_wsmgr_test_context. */
void *_wlm_wsmgr_test_setup(void)
{
    struct _wlm_wsmgr_test_context *c = logged_calloc(1, sizeof(*c));
    if (NULL == c) return NULL;

    c->wl_display_ptr = wl_display_create();
    c->wlr_output_layout_ptr = wlr_output_layout_create(c->wl_display_ptr);
    c->wlr_scene_ptr = wlr_scene_create();
    struct wlmtk_desktop_style ds = {};
    c->desktop_ptr = wlmtk_desktop_create(
        c->wlr_scene_ptr, &ds, c->wlr_output_layout_ptr);

    if (NULL == c->wl_display_ptr ||
        NULL == c->wlr_output_layout_ptr ||
        NULL == c->wlr_scene_ptr ||
        NULL == c->desktop_ptr) {
        _wlm_wsmgr_test_teardown(c);
        return NULL;
    }

    c->output = (struct wlr_output){ .width = 800, .height = 600, .scale = 1 };
    wlmtk_test_wlr_output_init(&c->output);
    wlr_output_layout_add(c->wlr_output_layout_ptr, &c->output, 0, 0);
    return c;
}

/* ------------------------------------------------------------------------- */
/** Destroys the objects of @ref _wlm_wsmgr_test_context. */
void _wlm_wsmgr_test_teardown(void *_ctx_ptr)
{
    struct _wlm_wsmgr_test_context *c = _ctx_ptr;

    if (NULL != c->desktop_ptr) {
        wlr_output_layout_remove(c->wlr_output_layout_ptr, &c->output);
        wlmtk_desktop_destroy(c->desktop_ptr);
    }

    if (c->wlr_scene_ptr) wlr_scene_node_destroy(&c->wlr_scene_ptr->tree.node);
    if (c->wlr_output_layout_ptr) {
        wlr_output_layout_destroy(c->wlr_output_layout_ptr);
    }
    if (c->wl_display_ptr) wl_display_destroy(c->wl_display_ptr);

    free(c);
}

/* ------------------------------------------------------------------------- */
/** Tests adding and removing workspaces. */
void _wlm_workspace_manager_test_workspaces(bs_test_t *test_ptr)
{
    struct _wlm_wsmgr_test_context *c = bs_test_context(test_ptr);
    struct wlm_workspace_manager *wsm_ptr = wlm_workspace_manager_create(
        c->wl_display_ptr, c->wlr_output_layout_ptr, c->desktop_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wsm_ptr);

    // No workspace, cannot destroy last.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlm_workspace_manager_destroy_last_workspace(wsm_ptr));

    // Add one workspace. Cannot destroy it, since active.
    bspl_dict_t *d = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Workspaces=({Name=test0;})}"));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_apply_state(wsm_ptr, d));
    bspl_dict_unref(d);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlm_workspace_manager_destroy_last_workspace(wsm_ptr));

    // Add one further workspace. Can destroy it, since not active.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_add_workspace(wsm_ptr, "test1"));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_destroy_last_workspace(wsm_ptr));

    // Add maximal many workspaces. Must work. The next one must fail.
    for (size_t i = 0; i < _wlm_wsmgr_max_workspaces - 1; ++i) {
        BS_TEST_VERIFY_TRUE(
            test_ptr,
            wlm_workspace_manager_add_workspace(wsm_ptr, "test"));
    }
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlm_workspace_manager_add_workspace(wsm_ptr, "too-much"));

    // Will (implicitly) destroy the first workspace.
    wlm_workspace_manager_destroy(wsm_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that workgroup handles are created for each client+workspace. */
void _wlm_workspace_manager_test_client(bs_test_t *test_ptr)
{
    struct _wlm_wsmgr_test_context *c = bs_test_context(test_ptr);
    struct wlm_workspace_manager *wsm_ptr = wlm_workspace_manager_create(
        c->wl_display_ptr, c->wlr_output_layout_ptr, c->desktop_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wsm_ptr);

    // Add a first client, then verify that workspace handles got added.
    struct _wlm_wsmgr_client *c0 = _wlm_wsmgr_client_create(0, 1, 0);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c0);
    _wlm_workspace_manager_add_client(wsm_ptr, c0);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_add_workspace(wsm_ptr, "ws0"));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_add_workspace(wsm_ptr, "ws1"));
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&c0->workspace_handles));

    // A second client, verify the workspace handles.
    struct _wlm_wsmgr_client *c1 = _wlm_wsmgr_client_create(0, 1, 0);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c1);
    _wlm_workspace_manager_add_client(wsm_ptr, c1);
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&c1->workspace_handles));

    // Later addition of a workspace, check that handles got added.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_add_workspace(wsm_ptr, "ws2"));
    BS_TEST_VERIFY_EQ(test_ptr, 3, bs_dllist_size(&c0->workspace_handles));
    BS_TEST_VERIFY_EQ(test_ptr, 3, bs_dllist_size(&c1->workspace_handles));

    // Add another output.
    struct wlr_output output2 = { .width = 640, .height = 480, .scale = 2 };
    wlmtk_test_wlr_output_init(&output2);
    wlr_output_layout_add_auto(c->wlr_output_layout_ptr, &output2);

    // Some removals, as leak detection.
    _wlm_workspace_manager_remove_client(wsm_ptr, c1);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(&c1->workspace_handles));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlm_workspace_manager_destroy_last_workspace(wsm_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&c0->workspace_handles));

    // Re-add.
    _wlm_workspace_manager_add_client(wsm_ptr, c1);
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&c1->workspace_handles));

    _wlm_wsmgr_client_finish(c0);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(&c0->workspace_handles));
    BS_TEST_VERIFY_TRUE(test_ptr, c0->finished);

    _wlm_workspace_manager_remove_client(wsm_ptr, c0);
    wlr_output_layout_remove(c->wlr_output_layout_ptr, &output2);

    // Leave c1 as member of the workspace manager, must be destroyed on
    // cleanup.
    _wlm_wsmgr_client_destroy(c0);

    wlm_workspace_manager_destroy(wsm_ptr);
}

/* == End of workspace_manager.c =========================================== */
