/* ========================================================================= */
/**
 * @file submenu.c
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

#include "submenu.h"

#include "util.h"

/* == Declarations ========================================================= */

/** State of submenu. */
struct _wlmtk_submenu_t {
    /** The menu item the submenu is anchored to. */
    wlmtk_menu_item_t         *menu_item_ptr;
    /** The submenu. */
    wlmtk_menu_t              *sub_menu_ptr;
    /** Links to the parent pane. */
    wlmtk_pane_t              *parent_pane_ptr;

    /** Temporary: Submenu item 1. */
    wlmtk_menu_item_t         *item1_ptr;
    /** Temporary: Submenu item 2. */
    wlmtk_menu_item_t         *item2_ptr;

    /** Listener for @ref wlmtk_menu_item_events_t::destroy. */
    struct wl_listener        item_destroy_listener;
};

static void _wlmtk_submenu_handle_item_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_submenu_t *wlmtk_submenu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_submenu_t *submenu_ptr = logged_calloc(1, sizeof(wlmtk_submenu_t));
    if (NULL == submenu_ptr) return NULL;

    submenu_ptr->menu_item_ptr = wlmtk_menu_item_create(
        &style_ptr->item, env_ptr);
    if (NULL == submenu_ptr->menu_item_ptr) {
        wlmtk_submenu_destroy(submenu_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(submenu_ptr->menu_item_ptr)->destroy,
        &submenu_ptr->item_destroy_listener,
        _wlmtk_submenu_handle_item_destroy);

    submenu_ptr->sub_menu_ptr = wlmtk_menu_create(style_ptr, env_ptr);
    if (NULL == submenu_ptr->sub_menu_ptr) {
        wlmtk_submenu_destroy(submenu_ptr);
        return NULL;
    }
    wlmtk_menu_item_set_submenu(
        submenu_ptr->menu_item_ptr,
        submenu_ptr->sub_menu_ptr);

    // TODO(kaeser@gubbe.ch): Well, the contents should be configurable.
    wlmtk_menu_item_set_text(submenu_ptr->menu_item_ptr, "Submenu test 1");
    submenu_ptr->item1_ptr = wlmtk_menu_item_create(&style_ptr->item, env_ptr);
    submenu_ptr->item2_ptr = wlmtk_menu_item_create(&style_ptr->item, env_ptr);
    wlmtk_menu_item_set_text(submenu_ptr->item1_ptr, "submenu sub 1");
    wlmtk_menu_item_set_text(submenu_ptr->item2_ptr, "submenu sub 2");

    wlmtk_menu_add_item(submenu_ptr->sub_menu_ptr, submenu_ptr->item1_ptr);
    wlmtk_menu_add_item(submenu_ptr->sub_menu_ptr, submenu_ptr->item2_ptr);

    wlmtk_element_set_position(
        wlmtk_menu_element(submenu_ptr->sub_menu_ptr),
        150, 0);

    return submenu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_submenu_destroy(wlmtk_submenu_t *submenu_ptr)
{
    if (NULL != submenu_ptr->sub_menu_ptr) {

        wlmtk_menu_remove_item(submenu_ptr->sub_menu_ptr, submenu_ptr->item1_ptr);
        wlmtk_menu_remove_item(submenu_ptr->sub_menu_ptr, submenu_ptr->item2_ptr);
        wlmtk_menu_item_destroy(submenu_ptr->item2_ptr);
        wlmtk_menu_item_destroy(submenu_ptr->item1_ptr);

        wlmtk_menu_destroy(submenu_ptr->sub_menu_ptr);
        submenu_ptr->sub_menu_ptr = NULL;
    }

    if (NULL != submenu_ptr->menu_item_ptr) {
        wlmtk_util_disconnect_listener(&submenu_ptr->item_destroy_listener);

        wlmtk_menu_item_destroy(submenu_ptr->menu_item_ptr);
        submenu_ptr->menu_item_ptr = NULL;
    }
    free(submenu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmtk_submenu_menu_item(wlmtk_submenu_t *submenu_ptr)
{
    return submenu_ptr->menu_item_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_item_events_t::destroy. Destroy the action item. */
void _wlmtk_submenu_handle_item_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_submenu_t, item_destroy_listener);

    wlmtk_menu_item_set_submenu(submenu_ptr->menu_item_ptr, NULL);
    submenu_ptr->menu_item_ptr = NULL;
    wlmtk_submenu_destroy(submenu_ptr);
}

/* == End of submenu.c ===================================================== */
