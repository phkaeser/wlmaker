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

#include "popup_menu.h"
#include "util.h"

/* == Declarations ========================================================= */

struct _wlmtk_submenu_t {
    wlmtk_menu_item_t         *menu_item_ptr;

    wlmtk_popup_menu_t        *popup_menu_ptr;


    wlmtk_menu_item_t         *item1_ptr;
    wlmtk_menu_item_t         *item2_ptr;

    /** Listener for @ref wlmtk_menu_item_events_t::state_changed. */
    struct wl_listener        state_changed_listener;
    /** Listener for @ref wlmtk_menu_item_events_t::triggered. */
    struct wl_listener        triggered_listener;
    /** Listener for @ref wlmtk_menu_item_events_t::destroy. */
    struct wl_listener        destroy_listener;
};

static void _wlmtk_submenu_handle_state_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_submenu_handle_triggered(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_submenu_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_submenu_t *wlmtk_submenu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_popup_menu_t *parent_pum_ptr)
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
        &wlmtk_menu_item_events(submenu_ptr->menu_item_ptr)->state_changed,
        &submenu_ptr->state_changed_listener,
        _wlmtk_submenu_handle_state_changed);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(submenu_ptr->menu_item_ptr)->triggered,
        &submenu_ptr->triggered_listener,
        _wlmtk_submenu_handle_triggered);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(submenu_ptr->menu_item_ptr)->destroy,
        &submenu_ptr->destroy_listener,
        _wlmtk_submenu_handle_destroy);

    submenu_ptr->popup_menu_ptr = wlmtk_popup_menu_create(style_ptr, env_ptr);
    if (NULL == submenu_ptr->popup_menu_ptr) {
        wlmtk_submenu_destroy(submenu_ptr);
        return NULL;
    }


    wlmtk_menu_item_set_text(submenu_ptr->menu_item_ptr, "FIXME");
    submenu_ptr->item1_ptr = wlmtk_menu_item_create(&style_ptr->item, env_ptr);
    submenu_ptr->item2_ptr = wlmtk_menu_item_create(&style_ptr->item, env_ptr);
    wlmtk_menu_item_set_text(submenu_ptr->item1_ptr, "FIXME: Item 1");
    wlmtk_menu_item_set_text(submenu_ptr->item2_ptr, "FIXME: Item 2");

    wlmtk_menu_add_item(
        wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
        submenu_ptr->item1_ptr);
    wlmtk_menu_add_item(
        wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
        submenu_ptr->item2_ptr);

    wlmtk_popup_add_popup(
        wlmtk_popup_menu_popup(parent_pum_ptr),
        wlmtk_popup_menu_popup(submenu_ptr->popup_menu_ptr));

    wlmtk_element_set_visible(
        wlmtk_popup_element(wlmtk_popup_menu_popup(submenu_ptr->popup_menu_ptr)),
        true);
    wlmtk_element_set_position(
        wlmtk_popup_element(wlmtk_popup_menu_popup(submenu_ptr->popup_menu_ptr)),
        150, 0);

    return submenu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_submenu_destroy(wlmtk_submenu_t *submenu_ptr)
{

    if (NULL != submenu_ptr->popup_menu_ptr) {

        wlmtk_menu_remove_item(
            wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
            submenu_ptr->item1_ptr);
        wlmtk_menu_remove_item(
            wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
            submenu_ptr->item2_ptr);

        wlmtk_popup_menu_destroy(submenu_ptr->popup_menu_ptr);
        submenu_ptr->popup_menu_ptr = NULL;
    }

    wlmtk_menu_item_destroy(submenu_ptr->item2_ptr);
    wlmtk_menu_item_destroy(submenu_ptr->item1_ptr);

    if (NULL == submenu_ptr->menu_item_ptr) {
        wlmtk_util_disconnect_listener(&submenu_ptr->destroy_listener);
        wlmtk_util_disconnect_listener(&submenu_ptr->triggered_listener);
        wlmtk_util_disconnect_listener(&submenu_ptr->state_changed_listener);

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
/** Handle @ref wlmtk_menu_item_events_t::state_changed. Show/hide submenu. */
void _wlmtk_submenu_handle_state_changed(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    int x, y, t, r;
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_submenu_t, state_changed_listener);
    wlmtk_element_t *item_element_ptr = wlmtk_menu_item_element(
        submenu_ptr->menu_item_ptr);
    wlmtk_element_t *popup_element_ptr = wlmtk_popup_element(
        wlmtk_popup_menu_popup(submenu_ptr->popup_menu_ptr));

    switch (wlmtk_menu_item_get_state(submenu_ptr->menu_item_ptr)) {
    case WLMTK_MENU_ITEM_HIGHLIGHTED:
        wlmtk_element_get_position(item_element_ptr, &x, &y);
        wlmtk_element_get_dimensions(item_element_ptr, NULL, &t, &r, NULL);
        x += r;
        y += t;
        wlmtk_element_set_position(popup_element_ptr, x, y);
        wlmtk_container_raise_element_to_top(
            popup_element_ptr->parent_container_ptr, popup_element_ptr);
        wlmtk_element_set_visible(popup_element_ptr, true);
        break;

    case WLMTK_MENU_ITEM_ENABLED:
    case WLMTK_MENU_ITEM_DISABLED:
    default:
        wlmtk_element_set_visible(popup_element_ptr, false);
        break;
    }
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_item_events_t::triggered. Triggers the action */
void _wlmtk_submenu_handle_triggered(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_submenu_t, triggered_listener);

    bs_log(BS_ERROR, "FIXME: Clicked: %p", submenu_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_item_events_t::destroy. Destroy the action item. */
void _wlmtk_submenu_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_submenu_t, destroy_listener);

    submenu_ptr->menu_item_ptr = NULL;
    wlmtk_submenu_destroy(submenu_ptr);
}

/* == End of submenu.c ===================================================== */
