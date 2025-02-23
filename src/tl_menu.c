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

#include "action_item.h"

/* == Declarations ========================================================= */

/** State of a toplevel's window menu. */
struct _wlmaker_tl_menu_t {
    /** Pointer to the window's @ref wlmtk_menu_t. */
    wlmtk_menu_t              *menu_ptr;

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
    /** Action item for 'to previous workspace'. */
    wlmaker_action_item_t     *prev_ws_ai_ptr;
    /** Action item for 'to next workspace'. */
    wlmaker_action_item_t     *next_ws_ai_ptr;
    /** Action item for 'close'. */
    wlmaker_action_item_t     *close_ai_ptr;
};

static void _wlmaker_tl_menu_handle_window_state_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Menu items for the XDG toplevel's window menu. */
static const wlmaker_action_item_desc_t _tl_menu_items[] = {
    {
        "Maximize",
        WLMAKER_ACTION_WINDOW_MAXIMIZE,
        offsetof(wlmaker_tl_menu_t, maximize_ai_ptr)
    },
    {
        "Unmaximize",
        WLMAKER_ACTION_WINDOW_UNMAXIMIZE,
        offsetof(wlmaker_tl_menu_t, unmaximize_ai_ptr)
    },
    {
        "Fullscreen",
        WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN,
        offsetof(wlmaker_tl_menu_t, fullscreen_ai_ptr)
    },
    {
        "Shade",
        WLMAKER_ACTION_WINDOW_SHADE,
        offsetof(wlmaker_tl_menu_t, shade_ai_ptr)

    },
    {
        "Unshade",
        WLMAKER_ACTION_WINDOW_UNSHADE,
        offsetof(wlmaker_tl_menu_t, unshade_ai_ptr)

    },
    {
        "To prev. workspace",
        WLMAKER_ACTION_WINDOW_TO_PREVIOUS_WORKSPACE,
        offsetof(wlmaker_tl_menu_t, prev_ws_ai_ptr)

    },
    {
        "To next workspace",
        WLMAKER_ACTION_WINDOW_TO_NEXT_WORKSPACE,
        offsetof(wlmaker_tl_menu_t, next_ws_ai_ptr)

    },
    {
        "Close",
        WLMAKER_ACTION_WINDOW_CLOSE,
        offsetof(wlmaker_tl_menu_t, close_ai_ptr)

    },
    { NULL, 0, 0 }  // Sentinel.
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
    tl_menu_ptr->menu_ptr = wlmtk_window_menu(window_ptr);

    for (const wlmaker_action_item_desc_t *desc_ptr = &_tl_menu_items[0];
         NULL != desc_ptr->text_ptr;
         ++desc_ptr) {

        wlmaker_action_item_t *ai_ptr = wlmaker_action_item_create_from_desc(
            desc_ptr,
            tl_menu_ptr,
            &server_ptr->style.menu.item,
            server_ptr,
            server_ptr->env_ptr);
        if (NULL == ai_ptr) {
            bs_log(BS_ERROR, "Failed wlmaker_action_item_create_from_desc()");
            wlmaker_tl_menu_destroy(tl_menu_ptr);
            return NULL;
        }

        wlmtk_menu_add_item(
            tl_menu_ptr->menu_ptr,
            wlmaker_action_item_menu_item(ai_ptr));
    }

    // Connect state listener and initialize state.
    wlmtk_util_connect_listener_signal(
        &wlmtk_window_events(window_ptr)->state_changed,
        &tl_menu_ptr->window_state_changed_listener,
        _wlmaker_tl_menu_handle_window_state_changed);
    _wlmaker_tl_menu_handle_window_state_changed(
        &tl_menu_ptr->window_state_changed_listener,
        window_ptr);

    if (false) {
        // TODO(kaeser@gubbe.ch): Move to appropriate place.
        wlmtk_submenu_t *submenu_ptr = wlmtk_submenu_create(
            &server_ptr->style.menu,
            server_ptr->env_ptr,
            wlmtk_window_menu_popup(window_ptr));
        if (NULL != submenu_ptr) {
            wlmtk_menu_add_item(
                tl_menu_ptr->menu_ptr,
                wlmtk_submenu_menu_item(submenu_ptr));
        }
    }

    return tl_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_tl_menu_destroy(wlmaker_tl_menu_t *tl_menu_ptr)
{
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

    if (NULL != wlmtk_window_get_workspace(window_ptr)) {
        bs_dllist_node_t *ws_dlnode_ptr = wlmtk_dlnode_from_workspace(
            wlmtk_window_get_workspace(window_ptr));
        wlmtk_menu_item_set_enabled(
            wlmaker_action_item_menu_item(tl_menu_ptr->prev_ws_ai_ptr),
            NULL != ws_dlnode_ptr->prev_ptr);
        wlmtk_menu_item_set_enabled(
            wlmaker_action_item_menu_item(tl_menu_ptr->next_ws_ai_ptr),
            NULL != ws_dlnode_ptr->next_ptr);
    }
}

/* == End of tl_menu.c ===================================================== */
