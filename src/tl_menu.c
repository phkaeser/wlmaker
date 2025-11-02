/* ========================================================================= */
/**
 * @file tl_menu.c
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

#include "tl_menu.h"

#include <libbase/libbase.h>
#include <stddef.h>
#include <stdlib.h>
#include <wayland-server-core.h>

#include "action.h"
#include "action_item.h"
#include "config.h"

/* == Declarations ========================================================= */

/** State of a toplevel's window menu. */
struct _wlmaker_tl_menu_t {
    /** Pointer to the window's @ref wlmtk_menu_t. */
    wlmtk_menu_t              *menu_ptr;
    /** Pointer to the submenu of `move_to_ws_ai_ptr`. */
    wlmtk_menu_t              *workspaces_submenu_ptr;

    /** Holds @ref wlmaker_tl_menu_ws_item_t::dlnode items. */
    bs_dllist_t               submenu_items;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
    /** Back-link to the window. */
    wlmtk_window_t           *window_ptr;

    /** Listener for @ref wlmtk_window_events_t::state_changed. */
    struct wl_listener        window_state_changed_listener;

    /** Action item for 'Maximize'. */
    wlmaker_action_item_t     *maximize_ai_ptr;
    /** Action item for 'Unmaximize'. */
    wlmaker_action_item_t     *unmaximize_ai_ptr;
    /** Action item for 'Fullscreen'. */
    wlmaker_action_item_t     *fullscreen_ai_ptr;
    /** Action item for 'Shade'. */
    wlmaker_action_item_t     *shade_ai_ptr;
    /** Action item for 'Unshade'. */
    wlmaker_action_item_t     *unshade_ai_ptr;
    /** Menu item for attaching the workspaces submenu. */
    wlmaker_action_item_t     *move_to_ws_ai_ptr;
    /** Action item for 'close'. */
    wlmaker_action_item_t     *close_ai_ptr;
};

/** Item holder. */
typedef struct {
    /** Element of @ref wlmaker_tl_menu_t::submenu_items. */
    bs_dllist_node_t          dlnode;

    /** Composed from a menu item. */
    wlmtk_menu_item_t         *menu_item_ptr;

    /** Window to move. */
    wlmtk_window_t           *window_ptr;
    /** Workspace to move it to. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Listener for @ref wlmtk_menu_item_events_t::triggered. */
    struct wl_listener        triggered_listener;
    /** Listener for @ref wlmtk_menu_item_events_t::destroy. */
    struct wl_listener        destroy_listener;

} wlmaker_tl_menu_ws_item_t;

static void _wlmaker_tl_menu_workspace_iterator_create_item(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmaker_tl_menu_ws_items_iterator_enable_workspace(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmaker_tl_menu_handle_window_state_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _destroy(wlmaker_tl_menu_ws_item_t *ws_item_ptr);
static void _item_handle_triggered(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _item_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Menu items for the XDG toplevel's window menu. */
static const wlmaker_action_item_desc_t _tl_menu_items[] = {
    {
        "Maximize",
        WLMAKER_ACTION_WINDOW_MAXIMIZE,
        NULL,
        offsetof(wlmaker_tl_menu_t, maximize_ai_ptr)
    },
    {
        "Unmaximize",
        WLMAKER_ACTION_WINDOW_UNMAXIMIZE,
        NULL,
        offsetof(wlmaker_tl_menu_t, unmaximize_ai_ptr)
    },
    {
        "Fullscreen",
        WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN,
        NULL,
        offsetof(wlmaker_tl_menu_t, fullscreen_ai_ptr)
    },
    {
        "Shade",
        WLMAKER_ACTION_WINDOW_SHADE,
        NULL,
        offsetof(wlmaker_tl_menu_t, shade_ai_ptr)

    },
    {
        "Unshade",
        WLMAKER_ACTION_WINDOW_UNSHADE,
        NULL,
        offsetof(wlmaker_tl_menu_t, unshade_ai_ptr)

    },
    {
        "Move to workspace ...",
        WLMAKER_ACTION_NONE,
        NULL,
        offsetof(wlmaker_tl_menu_t, move_to_ws_ai_ptr)
    },
    {
        "Close",
        WLMAKER_ACTION_WINDOW_CLOSE,
        NULL,
        offsetof(wlmaker_tl_menu_t, close_ai_ptr)

    },
    { NULL, 0, NULL, 0 }  // Sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_tl_menu_t *wlmaker_tl_menu_create(
    wlmtk_window_t *window_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_tl_menu_t *tl_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_tl_menu_t));
    if (NULL == tl_menu_ptr) return NULL;
    tl_menu_ptr->server_ptr = server_ptr;
    tl_menu_ptr->menu_ptr = wlmtk_window_menu(window_ptr);
    tl_menu_ptr->window_ptr = window_ptr;

    for (const wlmaker_action_item_desc_t *desc_ptr = &_tl_menu_items[0];
         NULL != desc_ptr->text_ptr;
         ++desc_ptr) {

        wlmaker_action_item_t *ai_ptr = wlmaker_action_item_create_from_desc(
            desc_ptr,
            tl_menu_ptr,
            &server_ptr->style.menu.item,
            server_ptr);
        if (NULL == ai_ptr) {
            bs_log(BS_ERROR, "Failed wlmaker_action_item_create_from_desc()");
            wlmaker_tl_menu_destroy(tl_menu_ptr);
            return NULL;
        }

        wlmtk_menu_add_item(
            tl_menu_ptr->menu_ptr,
            wlmaker_action_item_menu_item(ai_ptr));
    }

    tl_menu_ptr->workspaces_submenu_ptr = wlmtk_menu_create(
        &server_ptr->style.menu);
    if (NULL == tl_menu_ptr->workspaces_submenu_ptr) {
        wlmaker_tl_menu_destroy(tl_menu_ptr);
        return NULL;
    }
    wlmtk_menu_item_set_submenu(
        wlmaker_action_item_menu_item(tl_menu_ptr->move_to_ws_ai_ptr),
        tl_menu_ptr->workspaces_submenu_ptr);
    wlmtk_root_for_each_workspace(
        server_ptr->root_ptr,
        _wlmaker_tl_menu_workspace_iterator_create_item,
        tl_menu_ptr);

    bs_dllist_for_each(
        &tl_menu_ptr->submenu_items,
        _wlmaker_tl_menu_ws_items_iterator_enable_workspace,
        NULL);

    // Connect state listener and initialize state.
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(window_ptr)->state_changed,
        &tl_menu_ptr->window_state_changed_listener,
        _wlmaker_tl_menu_handle_window_state_changed);
    _wlmaker_tl_menu_handle_window_state_changed(
        &tl_menu_ptr->window_state_changed_listener,
        window_ptr);

    return tl_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_tl_menu_destroy(wlmaker_tl_menu_t *tl_menu_ptr)
{
    wlmtk_util_disconnect_listener(
        &tl_menu_ptr->window_state_changed_listener);

    free(tl_menu_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles state changes: Updates the menu items accordingly. */
void _wlmaker_tl_menu_handle_window_state_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_tl_menu_t *tl_menu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_tl_menu_t, window_state_changed_listener);
    wlmtk_window_t *window_ptr = data_ptr;

    wlmtk_menu_item_set_enabled(
        wlmaker_action_item_menu_item(tl_menu_ptr->shade_ai_ptr),
        !wlmtk_window_is_shaded(window_ptr));
    wlmtk_menu_item_set_enabled(
        wlmaker_action_item_menu_item(tl_menu_ptr->unshade_ai_ptr),
        wlmtk_window_is_shaded(window_ptr));

    wlmtk_menu_item_set_enabled(
        wlmaker_action_item_menu_item(tl_menu_ptr->fullscreen_ai_ptr),
        !wlmtk_window_is_fullscreen(window_ptr));

    wlmtk_menu_item_set_enabled(
        wlmaker_action_item_menu_item(tl_menu_ptr->maximize_ai_ptr),
        !wlmtk_window_is_maximized(window_ptr));
    wlmtk_menu_item_set_enabled(
        wlmaker_action_item_menu_item(tl_menu_ptr->unmaximize_ai_ptr),
        wlmtk_window_is_maximized(window_ptr));

    // Refresh the list of workspaces.
    bs_dllist_for_each(
        &tl_menu_ptr->submenu_items,
        _wlmaker_tl_menu_ws_items_iterator_enable_workspace,
        NULL);

}

/* ------------------------------------------------------------------------- */
/** Destroys the item holder. */
void _destroy(wlmaker_tl_menu_ws_item_t *ws_item_ptr)
{
    if (NULL != ws_item_ptr->menu_item_ptr) {
        wlmtk_menu_item_destroy(ws_item_ptr->menu_item_ptr);
        ws_item_ptr->menu_item_ptr = NULL;
    }
    free(ws_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates a menu item for each workspace. */
void _wlmaker_tl_menu_workspace_iterator_create_item(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);
    wlmaker_tl_menu_t *tl_menu_ptr = ud_ptr;

    const char *name_ptr;
    int index;
    wlmtk_workspace_get_details(workspace_ptr, &name_ptr, &index);

    wlmaker_tl_menu_ws_item_t *ws_item_ptr = logged_calloc(
        1, sizeof(wlmaker_tl_menu_ws_item_t));
    if (NULL == ws_item_ptr) return;
    ws_item_ptr->workspace_ptr = workspace_ptr;
    ws_item_ptr->window_ptr = tl_menu_ptr->window_ptr;

    ws_item_ptr->menu_item_ptr = wlmtk_menu_item_create(
        &tl_menu_ptr->server_ptr->style.menu.item);
    if (NULL == ws_item_ptr->menu_item_ptr) {
        _destroy(ws_item_ptr);
        return;
    }

    wlmtk_menu_item_set_text(ws_item_ptr->menu_item_ptr, name_ptr);

    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(ws_item_ptr->menu_item_ptr)->triggered,
        &ws_item_ptr->triggered_listener,
        _item_handle_triggered);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(ws_item_ptr->menu_item_ptr)->destroy,
        &ws_item_ptr->destroy_listener,
        _item_handle_destroy);

    wlmtk_menu_add_item(
        tl_menu_ptr->workspaces_submenu_ptr,
        ws_item_ptr->menu_item_ptr);
    bs_dllist_push_back(&tl_menu_ptr->submenu_items, &ws_item_ptr->dlnode);
}

/* ------------------------------------------------------------------------- */
/** Enables workspace items, except the one the window is currently on. */
void _wlmaker_tl_menu_ws_items_iterator_enable_workspace(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    wlmaker_tl_menu_ws_item_t *ws_item_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_tl_menu_ws_item_t, dlnode);

    wlmtk_menu_item_set_enabled(
        ws_item_ptr->menu_item_ptr,
        (wlmtk_window_get_workspace(ws_item_ptr->window_ptr) !=
         ws_item_ptr->workspace_ptr));
}

/* ------------------------------------------------------------------------- */
/** Handler for @ref wlmtk_menu_item_events_t::triggered. Moves the window. */
void _item_handle_triggered(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_tl_menu_ws_item_t *ws_item_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_tl_menu_ws_item_t, triggered_listener);

    wlmtk_workspace_unmap_window(
        wlmtk_window_get_workspace(ws_item_ptr->window_ptr),
        ws_item_ptr->window_ptr);
    wlmtk_workspace_map_window(
        ws_item_ptr->workspace_ptr,
        ws_item_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for @ref wlmtk_menu_item_events_t::destroy. Destroy. */
void _item_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_tl_menu_ws_item_t *ws_item_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_tl_menu_ws_item_t, destroy_listener);

    ws_item_ptr->menu_item_ptr = NULL;
    _destroy(ws_item_ptr);
}

/* == End of tl_menu.c ===================================================== */
