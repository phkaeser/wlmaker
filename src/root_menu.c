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
    wlmtk_menu_t              *menu_ptr;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
};

static void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr);
static wlmaker_action_item_t *_wlmaker_root_menu_create_action_item_from_array(
    wlmcfg_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static wlmtk_menu_t *_wlmaker_root_menu_create_menu_from_array(
    wlmcfg_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);

/* == Data ================================================================= */

/** Virtual method of the root menu's window content. */
static const wlmtk_content_vmt_t _wlmaker_root_menu_content_vmt = {
    .request_close = _wlmaker_root_menu_content_request_close
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
    if (wlmcfg_array_size(server_ptr->root_menu_array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return NULL;
    }
    if (WLMCFG_STRING != wlmcfg_object_type(
            wlmcfg_array_at(server_ptr->root_menu_array_ptr, 0))) {
        bs_log(BS_ERROR, "Array element [0] must be a string.");
        return NULL;
    }

    wlmaker_root_menu_t *root_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_t));
    if (NULL == root_menu_ptr) return NULL;
    root_menu_ptr->server_ptr = server_ptr;
    root_menu_ptr->server_ptr->root_menu_ptr = root_menu_ptr;

    root_menu_ptr->menu_ptr = _wlmaker_root_menu_create_menu_from_array(
        server_ptr->root_menu_array_ptr,
        menu_style_ptr,
        server_ptr);
    if (NULL == root_menu_ptr->menu_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    if (right_click_mode) {
        wlmtk_menu_set_mode(
            wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
            WLMTK_MENU_MODE_RIGHTCLICK);
    }

    if (!wlmtk_content_init(
            &root_menu_ptr->content,
            wlmtk_menu_element(root_menu_ptr->menu_ptr),
            env_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_content_extend(
        &root_menu_ptr->content,
        &_wlmaker_root_menu_content_vmt);
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_menu_element(root_menu_ptr->menu_ptr));
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
    wlmtk_window_set_title(
        root_menu_ptr->window_ptr,
        wlmcfg_array_string_value_at(server_ptr->root_menu_array_ptr, 0));
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
    if (NULL != root_menu_ptr->menu_ptr) {
        wlmtk_menu_destroy(root_menu_ptr->menu_ptr);
        root_menu_ptr->menu_ptr = NULL;
    }
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
    return root_menu_ptr->menu_ptr;
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

/* ------------------------------------------------------------------------- */
/**
 * Creates an action menu item from the plist array.
 *
 * @param array_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return Pointer to the created @ref wlmaker_action_item_t or NULL on error.
 */
wlmaker_action_item_t *_wlmaker_root_menu_create_action_item_from_array(
    wlmcfg_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    if (wlmcfg_array_size(server_ptr->root_menu_array_ptr) <= 2) {
        bs_log(BS_ERROR, "Needs >= 2 array elements for item definition.");
        return NULL;
    }
    const char *name_ptr = wlmcfg_array_string_value_at(array_ptr, 0);
    if (NULL == name_ptr) {
        bs_log(BS_ERROR, "Array element [0] for item must be a string.");
        return NULL;
    }

    wlmtk_menu_t *submenu_ptr = NULL;
    int action = WLMAKER_ACTION_NONE;
    wlmcfg_object_t *obj_ptr = wlmcfg_array_at(array_ptr, 1);
    if (WLMCFG_ARRAY == wlmcfg_object_type(obj_ptr)) {

        submenu_ptr = _wlmaker_root_menu_create_menu_from_array(
            array_ptr,
            menu_style_ptr,
            server_ptr);
        if (NULL == submenu_ptr) {
            bs_log(BS_ERROR, "Failed to create submenu for item '%s'",
                   name_ptr);
            return NULL;
        }

    } else {
        const char *action_name_ptr = wlmcfg_string_value(
            wlmcfg_string_from_object(obj_ptr));
        if (NULL == action_name_ptr) {
            bs_log(BS_ERROR, "Array element [1] for item '%s' must be a "
                   "string.", name_ptr);
            return NULL;
        }

        if (!wlmcfg_enum_name_to_value(
                wlmaker_action_desc,
                action_name_ptr,
                &action)) {
            bs_log(BS_ERROR, "Failed decoding '%s' of item '%s' into action.",
                   action_name_ptr, name_ptr);
            return NULL;
        }
    }

    wlmaker_action_item_t *action_item_ptr = wlmaker_action_item_create(
        name_ptr,
        &menu_style_ptr->item,
        action,
        server_ptr,
        server_ptr->env_ptr);
    if (NULL == action_item_ptr) {
        if (NULL == submenu_ptr) wlmtk_menu_destroy(submenu_ptr);
        return NULL;
    }

    wlmtk_menu_item_set_submenu(
        wlmaker_action_item_menu_item(action_item_ptr),
        submenu_ptr);
    return action_item_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a @ref wlmtk_menu_t from the plist array.
 *
 * @param array_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return A pointer to the created @ref wlmtk_menu_t or NULL on error.
 */
wlmtk_menu_t *_wlmaker_root_menu_create_menu_from_array(
    wlmcfg_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    if (wlmcfg_array_size(server_ptr->root_menu_array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return NULL;
    }
    const char *name_ptr = wlmcfg_array_string_value_at(array_ptr, 0);
    if (NULL == name_ptr) {
        bs_log(BS_ERROR, "Array element [0] must be a string.");
        return NULL;
    }

    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(
        menu_style_ptr,
        server_ptr->env_ptr);
    if (NULL == menu_ptr) return NULL;

    for (size_t i = 1; i < wlmcfg_array_size(array_ptr); ++i) {
        wlmcfg_array_t *item_array_ptr = wlmcfg_array_from_object(
            wlmcfg_array_at(array_ptr, i));
        if (NULL == item_array_ptr) {
            bs_log(BS_ERROR, "Array element [1] in '%s' must be an array.",
                   name_ptr);
            wlmtk_menu_destroy(menu_ptr);
            return NULL;
        }

        wlmaker_action_item_t *action_item_ptr =
            _wlmaker_root_menu_create_action_item_from_array(
                item_array_ptr,
                menu_style_ptr,
                server_ptr);
        if (NULL == action_item_ptr) {
            bs_log(BS_ERROR, "Failed to create action item from element [%zu] "
                   "in '%s'", i, name_ptr);
            return NULL;
        }

        wlmtk_menu_add_item(
            menu_ptr,
            wlmaker_action_item_menu_item(action_item_ptr));
    }

    return menu_ptr;
}

/* == End of root_menu.c =================================================== */
