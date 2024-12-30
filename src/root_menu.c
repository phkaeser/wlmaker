/* ========================================================================= */
/**
 * @file root_menu.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "root_menu.h"

#include <libbase/libbase.h>

#include "action_item.h"

/* == Declarations ========================================================= */

/** State of the root menu. */
struct _wlmaker_root_menu_t {
    /** Window. */
    wlmtk_window_t            *window_ptr;

    /** The root menu's window content base instance. */
    wlmtk_content_t           content;
    /** The root menu base instance. */
    wlmtk_menu_t              menu;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
};

static void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr);

/** Temporary: Struct for defining a menu item for the root menu. */
typedef struct {
    /** Text to use in the root menu item. */
    const char                *text_ptr;
    /** Action to be executed for that menu item. */
    wlmaker_action_t          action;
} wlmaker_root_menu_item_t;

/* == Data ================================================================= */

/** Virtual method of the root menu's window content. */
static const wlmtk_content_vmt_t _wlmaker_root_menu_content_vmt = {
    .request_close = _wlmaker_root_menu_content_request_close
};

/** Menu items in the root menu. */
static const wlmaker_root_menu_item_t _wlmaker_root_menu_items[] = {
    { "Previous Workspace", WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS },
    { "Next Workspace", WLMAKER_ACTION_WORKSPACE_TO_NEXT },
    { "Lock", WLMAKER_ACTION_LOCK_SCREEN },
    { "Exit", WLMAKER_ACTION_QUIT },
    { NULL, 0 }  // Sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_menu_t *wlmaker_root_menu_create(
    wlmaker_server_t *server_ptr,
    const wlmtk_window_style_t *window_style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    bool right_click_mode,
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_t));
    if (NULL == root_menu_ptr) return NULL;
    root_menu_ptr->server_ptr = server_ptr;
    root_menu_ptr->server_ptr->root_menu_ptr = root_menu_ptr;

    if (!wlmtk_menu_init(&root_menu_ptr->menu,
                         menu_style_ptr,
                         env_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    if (right_click_mode) {
        wlmtk_menu_set_mode(
            wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
            WLMTK_MENU_MODE_RIGHTCLICK);
    }

    for (const wlmaker_root_menu_item_t *i_ptr = &_wlmaker_root_menu_items[0];
         i_ptr->text_ptr != NULL;
         ++i_ptr) {
        wlmaker_action_item_t *action_item_ptr = wlmaker_action_item_create(
            i_ptr->text_ptr,
            &menu_style_ptr->item,
            i_ptr->action,
            server_ptr,
            env_ptr);
        if (NULL == action_item_ptr) {
            wlmaker_root_menu_destroy(root_menu_ptr);
            return NULL;
        }
        wlmtk_menu_add_item(
            &root_menu_ptr->menu,
            wlmaker_action_item_menu_item(action_item_ptr));
    }

    if (!wlmtk_content_init(
            &root_menu_ptr->content,
            wlmtk_menu_element(&root_menu_ptr->menu),
            env_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_content_extend(
        &root_menu_ptr->content,
        &_wlmaker_root_menu_content_vmt);
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_menu_element(&root_menu_ptr->menu));
    // TODO(kaeser@gubbe.ch): Should not be required. Also, the sequence
    // of set_server_side_decorated and set_attributes is brittle.
    wlmtk_content_commit(
        &root_menu_ptr->content,
        box.width,
        box.height,
        0);

    root_menu_ptr->window_ptr = wlmtk_window_create(
        &root_menu_ptr->content,
        window_style_ptr,
        menu_style_ptr,
        env_ptr);
    if (NULL == root_menu_ptr->window_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_window_set_title(root_menu_ptr->window_ptr, "Root Menu");
    wlmtk_window_set_server_side_decorated(root_menu_ptr->window_ptr, true);
    uint32_t properties = 0;
    if (right_click_mode) {
        properties |= WLMTK_WINDOW_PROPERTY_RIGHTCLICK;
    } else {
        properties |= WLMTK_WINDOW_PROPERTY_CLOSABLE;
    }
    wlmtk_window_set_properties(root_menu_ptr->window_ptr, properties);

    wlmtk_workspace_map_window(workspace_ptr, root_menu_ptr->window_ptr);
    if (right_click_mode) {
        wlmtk_container_pointer_grab(
            wlmtk_window_element(root_menu_ptr->window_ptr)->parent_container_ptr,
            wlmtk_window_element(root_menu_ptr->window_ptr));
    }


    return root_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_menu_destroy(wlmaker_root_menu_t *root_menu_ptr)
{
    if (NULL != root_menu_ptr->server_ptr) {
        BS_ASSERT(root_menu_ptr->server_ptr->root_menu_ptr == root_menu_ptr);
        root_menu_ptr->server_ptr->root_menu_ptr = NULL;
        root_menu_ptr->server_ptr = NULL;;
    }

    if (NULL != root_menu_ptr->window_ptr) {
        // Unmap, in case it's not unmapped yet.
        wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(
            root_menu_ptr->window_ptr);
        if (NULL != workspace_ptr) {
            wlmtk_workspace_unmap_window(workspace_ptr,
                                         root_menu_ptr->window_ptr);
        }

        wlmtk_window_destroy(root_menu_ptr->window_ptr);
        root_menu_ptr->window_ptr = NULL;
    }

    wlmtk_content_fini(&root_menu_ptr->content);
    wlmtk_menu_fini(&root_menu_ptr->menu);
    free(root_menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmaker_root_menu_window(wlmaker_root_menu_t *root_menu_ptr)
{
    return root_menu_ptr->window_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmaker_root_menu_menu(wlmaker_root_menu_t *root_menu_ptr)
{
    return &root_menu_ptr->menu;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_close. Closes root menu. */
void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_root_menu_t, content);
    wlmaker_root_menu_destroy(root_menu_ptr);
}

/* == End of root_menu.c =================================================== */
