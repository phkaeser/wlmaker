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
#define WLR_USE_UNSTABLE
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

    // FIXME
    // - must have a workspace group, to permit creating workspace
    //
    // - workspace group must track adding/removing outputs (output tracker?)

    /** Clients, through @ref wlm_workspace_manager_client::dlnode. */
    bs_dllist_t               clients;

    /** Workspaces, through @ref wlm_workspace_manager_workspace::dlnode. */
    bs_dllist_t               workspaces;

    // FIXME - not owned
    struct wlr_output_layout  *wlr_output_layout_ptr;
    wlmtk_desktop_t           *desktop_ptr;

    struct wlmtk_tile_style   tile_style;  // FIXME - remove.
};

/** Workspaces manager by the workspace manager. */
struct wlm_workspace_manager_workspace {
    /** Element of @ref wlm_workspace_manager::workspaces. */
    bs_dllist_node_t          dlnode;
    /** The actual @ref wlmtk_workspace_t connected here. */
    wlmtk_workspace_t         *wlmtk_workspace_ptr;
    /**
     * List of workspace handles across the connected clients. Linked through
     * @ref wlm_workspace_manager_client_workspace::workspace_dlnode.
     */
    bs_dllist_t               client_workspaces;
};

/** Resources associated to a client of the workspace manager. */
struct wlm_workspace_manager_client {
    /** Element of @ref wlm_workspace_manager::clients. */
    bs_dllist_node_t          dlnode;

    /** The associated client resource for the workspace manager. */
    struct wl_resource        *manager_wl_resource_ptr;

    struct wl_resource        *workspace_group_wl_resource_ptr;

    /**
     * List of workspace handles for this client. Linkes through
     * @ref wlm_workspace_manager_client_workspace::client_dlnode.
     */
    bs_dllist_t               client_workspaces;

    /** Back-link to the workspace manager. */
    struct wlm_workspace_manager *workspace_manager_ptr;
};

/** Workspace handle for a given client. */
struct wlm_workspace_manager_client_workspace {
    /** Element of client, through @ref wlm_workspace_manager_client::FIXME. */
    bs_dllist_node_t          client_dlnode;

    /** The workspace, through @ref wlm_workspace_manager_workspace::client_workspaces. */
    bs_dllist_node_t          workspace_dlnode;

    /** The wayland resource for this client and workspace's handle. */
    struct wl_resource        *workspace_wl_resource_ptr;
};

struct wlm_workspace_state_config {
    /** Workspace name. */
    char            name[32];
};

struct wlm_workspace_manager_client *_wlm_workspace_manager_client_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

void _wlm_workspace_manager_destroy_workspace_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

    static void _wlm_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void _wlm_workspace_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr);
static void _wlm_workspace_handle_commit(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void _wlm_workspace_handle_stop(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

static void _wlm_workspace_group_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr);
static void _wlm_workspace_group_handle_create_workspace(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    const char *workspace_name_ptr);
static void _wlm_workspace_group_handle_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

/* == Data ================================================================= */

/** Style descriptor for the "Workspace" dict of wlmaker-state.plist. */
static const bspl_desc_t wlm_workspace_state_config_desc[] = {
    BSPL_DESC_CHARBUF(
        "Name", true, struct wlm_workspace_state_config, name, name, 32, NULL),
    BSPL_DESC_SENTINEL()
};

static const struct ext_workspace_manager_v1_interface
_wlm_workspace_manager_interface = {
    .commit = _wlm_workspace_handle_commit,
    .stop = _wlm_workspace_handle_stop
};

static const struct ext_workspace_group_handle_v1_interface
_wlm_workspace_group_interface = {
    .create_workspace = _wlm_workspace_group_handle_create_workspace,
    .destroy = _wlm_workspace_group_handle_destroy,
};

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
        _wlm_workspace_manager_destroy_workspace_dlnode,
        workspace_manager_ptr);

    for (bs_dllist_node_t *node_ptr = workspace_manager_ptr->clients.head_ptr;
         node_ptr != NULL;
         node_ptr = bs_dllist_node_iterator_forward(node_ptr)) {
        struct wlm_workspace_manager_client *client_ptr =
            _wlm_workspace_manager_client_from_dlnode(node_ptr);
        wl_resource_destroy(client_ptr->manager_wl_resource_ptr);
    }

    if (NULL != workspace_manager_ptr->wl_global_ptr) {
        wl_global_destroy(workspace_manager_ptr->wl_global_ptr);
        workspace_manager_ptr->wl_global_ptr = NULL;
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
        bspl_dict_t *dict_ptr = bspl_dict_from_object(
            bspl_array_at(array_ptr, i));
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "Element %zu in \"Workspaces\" not a dict", i);
            return false;
        }

        struct wlm_workspace_state_config c = {};
        if (!bspl_decode_dict(dict_ptr, wlm_workspace_state_config_desc, &c)) {
            bs_log(BS_ERROR,
                   "Failed decoding element %zu in \"Workspaces\"", i);
            return false;
        }

        if (!wlm_workspace_manager_add_workspace(
                workspace_manager_ptr, c.name)) {
            wlm_workspace_manager_destroy(workspace_manager_ptr);
        }
    }
    return true;
}


/* ------------------------------------------------------------------------- */
bool wlm_workspace_manager_add_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr,
    const char *name_ptr)
{
    struct wlm_workspace_manager_workspace *workspace_ptr = logged_calloc(
        1, sizeof(*workspace_ptr));
    if (NULL == workspace_ptr) return false;

    workspace_ptr->wlmtk_workspace_ptr = wlmtk_workspace_create(
        workspace_manager_ptr->wlr_output_layout_ptr,
        name_ptr,
        &workspace_manager_ptr->tile_style);
    if (NULL == workspace_ptr->wlmtk_workspace_ptr) {
        free(workspace_ptr);
        return false;
    }

    // FIXME: Step through all clients and add the client workspace handle.
    // and: commit for each client.

    wlmtk_desktop_add_workspace(
        workspace_manager_ptr->desktop_ptr,
        workspace_ptr->wlmtk_workspace_ptr);
    bs_dllist_push_back(
        &workspace_manager_ptr->workspaces,
        &workspace_ptr->dlnode);
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlm_workspace_manager_destroy_last_workspace(
    struct wlm_workspace_manager *workspace_manager_ptr)
{
    struct wlm_workspace_manager_workspace *workspace_ptr = BS_CONTAINER_OF(
        workspace_manager_ptr->workspaces.tail_ptr,
        struct wlm_workspace_manager_workspace,
        dlnode);
    wlmtk_workspace_t *curr_ws_ptr = wlmtk_desktop_get_current_workspace(
        workspace_manager_ptr->desktop_ptr);
    if (NULL == workspace_ptr ||
        workspace_ptr->wlmtk_workspace_ptr == curr_ws_ptr) return false;

    bs_dllist_node_t *dlnode_ptr = bs_dllist_pop_back(
        &workspace_manager_ptr->workspaces);
    BS_ASSERT(dlnode_ptr == &workspace_ptr->dlnode);
    _wlm_workspace_manager_destroy_workspace_dlnode(
        dlnode_ptr, workspace_manager_ptr);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Returns the @ref wlm_workspace_manager_client holding `dlnode_ptr`. */
struct wlm_workspace_manager_client *_wlm_workspace_manager_client_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(
        dlnode_ptr,
        struct wlm_workspace_manager_client,
        dlnode);
}

/* ------------------------------------------------------------------------- */
/** Destroys @ref wlm_workspace_manager_workspace, from it's dlnode. */
void _wlm_workspace_manager_destroy_workspace_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlm_workspace_manager_workspace *workspace_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlm_workspace_manager_workspace, dlnode);
    struct wlm_workspace_manager *workspace_manager_ptr = ud_ptr;

    if (NULL != workspace_ptr->wlmtk_workspace_ptr) {
        wlmtk_desktop_remove_workspace(
            workspace_manager_ptr->desktop_ptr,
            workspace_ptr->wlmtk_workspace_ptr);

        wlmtk_workspace_destroy(workspace_ptr->wlmtk_workspace_ptr);
        workspace_ptr->wlmtk_workspace_ptr = NULL;
    }

    // FIXME: delete all associated client workspaces.

    free(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Binds a client implementing the workspace interface. */
void _wlm_workspace_manager_bind(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wlm_workspace_manager *workspace_manager_ptr = data_ptr;

    struct wlm_workspace_manager_client *client_ptr = logged_calloc(
        1, sizeof(*client_ptr));
    if (NULL == client_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
    client_ptr->workspace_manager_ptr = workspace_manager_ptr;

    client_ptr->manager_wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &ext_workspace_manager_v1_interface,
        version, id);
    if (NULL == client_ptr->manager_wl_resource_ptr) {
        free(client_ptr);
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }

    wl_resource_set_implementation(
        client_ptr->manager_wl_resource_ptr,
        &_wlm_workspace_manager_interface,
        client_ptr,
        _wlm_workspace_handle_resource_destroy);
    bs_dllist_push_back(
        &workspace_manager_ptr->clients,
        &client_ptr->dlnode);

    client_ptr->workspace_group_wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &ext_workspace_group_handle_v1_interface,
        version, 0 /* create new ID */ );
    if (NULL == client_ptr->workspace_group_wl_resource_ptr) {
        _wlm_workspace_handle_resource_destroy(
            client_ptr->manager_wl_resource_ptr);
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
    wl_resource_set_implementation(
        client_ptr->workspace_group_wl_resource_ptr,
        &_wlm_workspace_group_interface,
        client_ptr,
        _wlm_workspace_group_handle_resource_destroy);
    ext_workspace_manager_v1_send_workspace_group(
        client_ptr->manager_wl_resource_ptr,
        client_ptr->workspace_group_wl_resource_ptr);

    // Send capabilities. Initially: None.
    ext_workspace_group_handle_v1_send_capabilities(
        client_ptr->workspace_group_wl_resource_ptr, 0);

    // FIXME: apply all outputs.
    // FIXME: apply all workspaces.

    bs_log(BS_INFO, "Workspace manager %p bound client %p on resource %p",
           workspace_manager_ptr,
           client_ptr,
           client_ptr->manager_wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroys the client's resources. */
void _wlm_workspace_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr)
{
    struct wlm_workspace_manager_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);

    bs_log(BS_INFO, "Workspace manager %p destroy client %p on resource %p",
           client_ptr->workspace_manager_ptr,
           client_ptr,
           client_ptr->manager_wl_resource_ptr);

    if (NULL != client_ptr->workspace_group_wl_resource_ptr) {
        ext_workspace_group_handle_v1_send_removed(
            client_ptr->workspace_group_wl_resource_ptr);
        wl_resource_destroy(client_ptr->workspace_group_wl_resource_ptr);
        client_ptr->workspace_group_wl_resource_ptr = NULL;
    }

    bs_dllist_remove(
        &client_ptr->workspace_manager_ptr->clients,
        &client_ptr->dlnode);

    if (NULL != client_ptr->manager_wl_resource_ptr) {
        ext_workspace_manager_v1_send_finished(
            client_ptr->manager_wl_resource_ptr);
        client_ptr->manager_wl_resource_ptr = NULL;
    }

    free(client_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlm_workspace_handle_commit(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_ERROR, "FIXME: commit %p - %p", wl_client_ptr, wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlm_workspace_handle_stop(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    bs_log(BS_ERROR, "FIXME: stop %p - %p", wl_client_ptr, wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** The workspace group resource is destroyed. Remove the reference. */
void _wlm_workspace_group_handle_resource_destroy(
    struct wl_resource *wl_resource_ptr)
{
    struct wlm_workspace_manager_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    client_ptr->workspace_group_wl_resource_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Requets to create a workspace. Currently not supported for `wlmaker`.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param workspace_name_ptr
 */
void _wlm_workspace_group_handle_create_workspace(
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
void _wlm_workspace_group_handle_destroy(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    struct wlm_workspace_manager_client *client_ptr =
        wl_resource_get_user_data(wl_resource_ptr);
    client_ptr->workspace_group_wl_resource_ptr = NULL;
}

/* == Unit Tests =========================================================== */

static void _wlm_workspace_manager_test_setup(bs_test_t *test_ptr);

/** Unit test cases for workspace manager. */
static const bs_test_case_t _wlm_workspace_manager_test_cases[] = {
    { true, "setup", _wlm_workspace_manager_test_setup },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlm_workspace_manager_test_set = BS_TEST_SET(
    true, "workspace_manager", _wlm_workspace_manager_test_cases);

/* ------------------------------------------------------------------------- */
void _wlm_workspace_manager_test_setup(bs_test_t *test_ptr)
{
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(wl_display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wlmtk_desktop_style ds = {};
    wlmtk_desktop_t *desktop_ptr = wlmtk_desktop_create(
        wlr_scene_ptr, &ds, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, desktop_ptr);

    struct wlm_workspace_manager *wsm_ptr = wlm_workspace_manager_create(
        wl_display_ptr, wlr_output_layout_ptr, desktop_ptr);
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

    // Will (implicitly) destroy the first workspace.
    wlm_workspace_manager_destroy(wsm_ptr);

    wlmtk_desktop_destroy(desktop_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(wl_display_ptr);
}

/* == End of workspace_manager.c =========================================== */
