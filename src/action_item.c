/* ========================================================================= */
/**
 * @file action_item.c
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

#include "action_item.h"

/* == Declarations ========================================================= */

/** State of an action item that triggers a @ref wlmaker_action_t. */
struct _wlmaker_action_item_t {
    /** Composed from a menu item. */
    wlmtk_menu_item_t         *menu_item_ptr;

    /** Action to execute when triggered. */
    wlmaker_action_t          action;
    /** Back-link to @ref wlmaker_server_t, for executing the action. */
    wlmaker_server_t          *server_ptr;

    /** Listener for @ref wlmtk_menu_item_events_t::triggered. */
    struct wl_listener        triggered_listener;
    /** Listener for @ref wlmtk_menu_item_events_t::destroy. */
    struct wl_listener        destroy_listener;
};

static void _wlmaker_action_item_handle_triggered(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_action_item_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_action_item_t *wlmaker_action_item_create(
    const char *text_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmaker_action_t action,
    wlmaker_server_t *server_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_action_item_t *action_item_ptr = logged_calloc(
        1, sizeof(wlmaker_action_item_t));
    if (NULL == action_item_ptr) return NULL;
    action_item_ptr->action = action;
    action_item_ptr->server_ptr = server_ptr;

    action_item_ptr->menu_item_ptr = wlmtk_menu_item_create(style_ptr, env_ptr);
    if (NULL == action_item_ptr->menu_item_ptr) {
        wlmaker_action_item_destroy(action_item_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(action_item_ptr->menu_item_ptr)->triggered,
        &action_item_ptr->triggered_listener,
        _wlmaker_action_item_handle_triggered);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_item_events(action_item_ptr->menu_item_ptr)->destroy,
        &action_item_ptr->destroy_listener,
        _wlmaker_action_item_handle_destroy);

    if (!wlmtk_menu_item_set_text(action_item_ptr->menu_item_ptr, text_ptr)) {
        wlmaker_action_item_destroy(action_item_ptr);
        return NULL;
    }

    return action_item_ptr;
}

/* ------------------------------------------------------------------------- */
wlmaker_action_item_t *wlmaker_action_item_create_from_desc(
    const wlmaker_action_item_desc_t *desc_ptr,
    void *dest_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmaker_server_t *server_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_action_item_t *action_item_ptr = wlmaker_action_item_create(
        desc_ptr->text_ptr,
        style_ptr,
        desc_ptr->action,
        server_ptr,
        env_ptr);
    if (NULL == action_item_ptr) return NULL;

    *(wlmaker_action_item_t**)(
        (uint8_t*)dest_ptr + desc_ptr->destination_ofs) = action_item_ptr;
    return action_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_action_item_destroy(wlmaker_action_item_t *action_item_ptr)
{
    if (NULL != action_item_ptr->menu_item_ptr) {
        wlmtk_util_disconnect_listener(&action_item_ptr->destroy_listener);
        wlmtk_util_disconnect_listener(&action_item_ptr->triggered_listener);

        wlmtk_menu_item_destroy(action_item_ptr->menu_item_ptr);
        action_item_ptr->menu_item_ptr = NULL;
    }
    free(action_item_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmaker_action_item_menu_item(
    wlmaker_action_item_t *action_item_ptr)
{
    return action_item_ptr->menu_item_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_item_events_t::triggered. Triggers the action */
void _wlmaker_action_item_handle_triggered(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_action_item_t *action_item_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_action_item_t, triggered_listener);

    wlmaker_action_execute(
        action_item_ptr->server_ptr,
        action_item_ptr->action);

    if (NULL != action_item_ptr->server_ptr->root_menu_ptr) {
        wlmaker_root_menu_destroy(action_item_ptr->server_ptr->root_menu_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_item_events_t::destroy. Destroy the action item. */
void _wlmaker_action_item_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_action_item_t *action_item_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_action_item_t, destroy_listener);

    // Clear the reference to the menu item. It is already being destroyed.
    wlmtk_util_disconnect_listener(&action_item_ptr->destroy_listener);
    wlmtk_util_disconnect_listener(&action_item_ptr->triggered_listener);
    action_item_ptr->menu_item_ptr = NULL;
    wlmaker_action_item_destroy(action_item_ptr);
}

/* == Unit tests =========================================================== */

static void _wlmaker_action_item_test_create(bs_test_t *test_ptr);
static void _wlmaker_action_item_test_menu_dtor(bs_test_t *test_ptr);

/** Test cases for action items. */
const bs_test_case_t          wlmaker_action_item_test_cases[] = {
    { 1, "create", _wlmaker_action_item_test_create },
    { 1, "menu_dtor", _wlmaker_action_item_test_menu_dtor },
    { 0, NULL, NULL },
};

/** Test data: style for the menu item. */
static const wlmtk_menu_style_t _wlmaker_action_item_menu_style = {};
/** Test data: Descriptor for the action item used in tests. */
static const wlmaker_action_item_desc_t _wlmaker_action_item_desc = {
    "text", 42, 0
};

/* ------------------------------------------------------------------------- */
/** Tests creation the menu item. */
void _wlmaker_action_item_test_create(bs_test_t *test_ptr)
{
    wlmaker_action_item_t *ai_ptr = NULL;
    wlmaker_server_t server = {};

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmaker_action_item_create_from_desc(
            &_wlmaker_action_item_desc,
            &ai_ptr,
            &_wlmaker_action_item_menu_style.item, &server,
            NULL));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, ai_ptr);
    wlmaker_action_item_destroy(ai_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that dtors are called as deisred from the menu. */
void _wlmaker_action_item_test_menu_dtor(bs_test_t *test_ptr)
{
    wlmtk_menu_t *menu_ptr;
    wlmaker_action_item_t *ai_ptr;
    wlmaker_server_t server = {};

    menu_ptr = wlmtk_menu_create(&_wlmaker_action_item_menu_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);

    ai_ptr = wlmaker_action_item_create_from_desc(
        &_wlmaker_action_item_desc,
        &ai_ptr,
        &_wlmaker_action_item_menu_style.item, &server,
        NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ai_ptr);
    wlmtk_menu_add_item(menu_ptr, wlmaker_action_item_menu_item(ai_ptr));

    wlmtk_menu_destroy(menu_ptr);
}

/* == End of action_item.c ================================================= */
