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
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>

#include "action.h"
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
    /** Listener for @ref wlmtk_menu_events_t::open_changed. */
    struct wl_listener        menu_open_changed_listener;
    /** Listener for @ref wlmtk_menu_events_t::request_close. */
    struct wl_listener        menu_request_close_listener;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
};

static void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr);
static void _wlmaker_root_menu_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

static void _wlmaker_root_menu_handle_menu_open_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_root_menu_handle_request_close(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static wlmaker_action_item_t *_wlmaker_root_menu_create_action_item_from_array(
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static wlmtk_menu_t *_wlmaker_root_menu_create_menu_from_array(
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);

/* == Data ================================================================= */

/** Virtual method of the root menu's window content. */
static const wlmtk_content_vmt_t _wlmaker_root_menu_content_vmt = {
    .request_close = _wlmaker_root_menu_content_request_close,
    .set_activated = _wlmaker_root_menu_content_set_activated,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_menu_t *wlmaker_root_menu_create(
    wlmaker_server_t *server_ptr,
    const wlmtk_window_style_t *window_style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr)
{
    if (bspl_array_size(server_ptr->root_menu_array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return NULL;
    }
    if (BSPL_STRING != bspl_object_type(
            bspl_array_at(server_ptr->root_menu_array_ptr, 0))) {
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
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(root_menu_ptr->menu_ptr)->open_changed,
        &root_menu_ptr->menu_open_changed_listener,
        _wlmaker_root_menu_handle_menu_open_changed);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(root_menu_ptr->menu_ptr)->request_close,
        &root_menu_ptr->menu_request_close_listener,
        _wlmaker_root_menu_handle_request_close);

    // FIXME - really terrible hack.
    wlmtk_pane_t *pane_ptr = wlmtk_menu_pane(root_menu_ptr->menu_ptr);
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_menu_element(root_menu_ptr->menu_ptr));
    wlmtk_container_remove_element(
        &pane_ptr->super_container,
        pane_ptr->element_ptr);
    if (!wlmtk_content_init(
            &root_menu_ptr->content,
            pane_ptr->element_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_container_remove_element(
        &pane_ptr->super_container,
        &pane_ptr->popup_container.super_element);
    wlmtk_container_add_element(
        &root_menu_ptr->content.popup_container,
        &pane_ptr->popup_container.super_element);

    wlmtk_content_extend(
        &root_menu_ptr->content,
        &_wlmaker_root_menu_content_vmt);
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
        server_ptr->wlr_seat_ptr);
    if (NULL == root_menu_ptr->window_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_window_set_title(
        root_menu_ptr->window_ptr,
        bspl_array_string_value_at(server_ptr->root_menu_array_ptr, 0));
    wlmtk_window_set_server_side_decorated(root_menu_ptr->window_ptr, true);

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

    if (NULL != root_menu_ptr->menu_ptr) {
        wlmtk_content_set_element(&root_menu_ptr->content, NULL);
        wlmtk_pane_t *pane_ptr = wlmtk_menu_pane(root_menu_ptr->menu_ptr);
        wlmtk_container_add_element(
            &pane_ptr->super_container,
            pane_ptr->element_ptr);

        wlmtk_container_remove_element(
            &root_menu_ptr->content.popup_container,
            &pane_ptr->popup_container.super_element);
        wlmtk_container_add_element(
            &pane_ptr->super_container,
            &pane_ptr->popup_container.super_element);
    }

    wlmtk_content_fini(&root_menu_ptr->content);
    if (NULL != root_menu_ptr->menu_ptr) {
        wlmtk_util_disconnect_listener(
            &root_menu_ptr->menu_request_close_listener);
        wlmtk_util_disconnect_listener(
            &root_menu_ptr->menu_open_changed_listener);
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

    wlmtk_menu_set_open(root_menu_ptr->menu_ptr, false);
}

/* ------------------------------------------------------------------------- */
/** Imlements @ref wlmtk_content_vmt_t::set_activated. Gets keyboard focus. */
void _wlmaker_root_menu_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_root_menu_t, content);

    wlmtk_element_t *e = wlmtk_menu_pane(root_menu_ptr->menu_ptr)->element_ptr;
    if (NULL != e->parent_container_ptr) {
        wlmtk_container_set_keyboard_focus_element(
            e->parent_container_ptr, e, activated);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_events_t::open_changed. Unmaps window on close. */
void _wlmaker_root_menu_handle_menu_open_changed(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_root_menu_t, menu_open_changed_listener);
    if (!wlmtk_menu_is_open(root_menu_ptr->menu_ptr) &&
        NULL != wlmtk_window_get_workspace(root_menu_ptr->window_ptr)) {
        wlmtk_workspace_unmap_window(
            wlmtk_window_get_workspace(root_menu_ptr->window_ptr),
            root_menu_ptr->window_ptr);
    } else {

        uint32_t properties = 0;
        if (WLMTK_MENU_MODE_RIGHTCLICK ==
            wlmtk_menu_get_mode(root_menu_ptr->menu_ptr)) {
            properties |= WLMTK_WINDOW_PROPERTY_RIGHTCLICK;

            wlmtk_container_pointer_grab(
                wlmtk_window_element(
                    root_menu_ptr->window_ptr)->parent_container_ptr,
                wlmtk_window_element(root_menu_ptr->window_ptr));

        } else {
            properties |= WLMTK_WINDOW_PROPERTY_CLOSABLE;
        }
        wlmtk_window_set_properties(root_menu_ptr->window_ptr, properties);

    }
}

/* ------------------------------------------------------------------------- */
/** Listens to @ref wlmtk_menu_events_t::request_close. Closes the menu. */
void _wlmaker_root_menu_handle_request_close(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_root_menu_t, menu_request_close_listener);

    wlmtk_menu_set_open(root_menu_ptr->menu_ptr, false);
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
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    if (bspl_array_size(server_ptr->root_menu_array_ptr) <= 2) {
        bs_log(BS_ERROR, "Needs >= 2 array elements for item definition.");
        return NULL;
    }
    const char *name_ptr = bspl_array_string_value_at(array_ptr, 0);
    if (NULL == name_ptr) {
        bs_log(BS_ERROR, "Array element [0] for item must be a string.");
        return NULL;
    }

    wlmtk_menu_t *submenu_ptr = NULL;
    int action = WLMAKER_ACTION_NONE;
    bspl_object_t *obj_ptr = bspl_array_at(array_ptr, 1);
    if (BSPL_ARRAY == bspl_object_type(obj_ptr)) {

#if 1
        // TODO(kaeser@gubbe.ch): Re-enable, once submenu hierarchy fixed.
        submenu_ptr = _wlmaker_root_menu_create_menu_from_array(
            array_ptr,
            menu_style_ptr,
            server_ptr);
        if (NULL == submenu_ptr) {
            bs_log(BS_ERROR, "Failed to create submenu for item '%s'",
                   name_ptr);
            return NULL;
        }
#else
        bs_log(BS_ERROR, "Submenu definition from plist yet unsupported.");
        return NULL;
#endif

    } else {
        const char *action_name_ptr = bspl_string_value(
            bspl_string_from_object(obj_ptr));
        if (NULL == action_name_ptr) {
            bs_log(BS_ERROR, "Array element [1] for item '%s' must be a "
                   "string.", name_ptr);
            return NULL;
        }

        if (!bspl_enum_name_to_value(
                wlmaker_action_desc,
                action_name_ptr,
                &action)) {
            bs_log(BS_ERROR, "Failed decoding '%s' of item '%s' into action.",
                   action_name_ptr, name_ptr);
            return NULL;
        }
    }

    const char *action_arg_ptr = NULL;
    if (2 < bspl_array_size(array_ptr)) {
        action_arg_ptr = bspl_array_string_value_at(array_ptr, 2);
    }

    wlmaker_action_item_t *action_item_ptr = wlmaker_action_item_create(
        name_ptr,
        &menu_style_ptr->item,
        action,
        action_arg_ptr,
        server_ptr);
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
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    if (bspl_array_size(server_ptr->root_menu_array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return NULL;
    }
    const char *name_ptr = bspl_array_string_value_at(array_ptr, 0);
    if (NULL == name_ptr) {
        bs_log(BS_ERROR, "Array element [0] must be a string.");
        return NULL;
    }

    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(menu_style_ptr);
    if (NULL == menu_ptr) return NULL;

    for (size_t i = 1; i < bspl_array_size(array_ptr); ++i) {
        bspl_array_t *item_array_ptr = bspl_array_from_object(
            bspl_array_at(array_ptr, i));
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
