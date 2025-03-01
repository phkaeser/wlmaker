/* ========================================================================= */
/**
 * @file menu.c
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

#include "menu.h"

#include "style.h"

/* == Declarations ========================================================= */

/** State of the menu. */
struct _wlmtk_menu_t {
    /** Instantiates a @ref wlmtk_pane_t. */
    wlmtk_pane_t              super_pane;

    /** Composed of a box, holding menu items. */
    wlmtk_box_t               box;
    /** Style of the menu. */
    wlmtk_menu_style_t        style;

    /** Signals that can be raised by the menu. */
    wlmtk_menu_events_t       events;
    /** Virtual method table of the parent, before extending. */
    wlmtk_element_vmt_t       orig_element_vmt;

    /** List of menu items, via @ref wlmtk_menu_item_t::dlnode. */
    bs_dllist_t               items;
    /** The currently-highlighted menu item, or NULL if none. */
    wlmtk_menu_item_t         *highlighted_menu_item_ptr;
    /** Current mode of the menu. */
    wlmtk_menu_mode_t         mode;
};

static void _wlmtk_menu_eliminate_item(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_menu_set_item_mode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static void _wlmtk_menu_element_destroy(
    wlmtk_element_t *element_ptr);
static bool _wlmtk_menu_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

/* == Data ================================================================= */

/** The superclass' element virtual method table. */
static const wlmtk_element_vmt_t _wlmtk_menu_element_vmt = {
    .destroy = _wlmtk_menu_element_destroy,
    .pointer_button = _wlmtk_menu_element_pointer_button
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_menu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_menu_t *menu_ptr = logged_calloc(1, sizeof(wlmtk_menu_t));
    if (NULL == menu_ptr) return NULL;
    menu_ptr->style = *style_ptr;

    if (!wlmtk_box_init(
            &menu_ptr->box,
            env_ptr,
            WLMTK_BOX_VERTICAL,
            &menu_ptr->style.margin)) {
        wlmtk_menu_destroy(menu_ptr);
        return NULL;
    }

    if (!wlmtk_pane_init(
            &menu_ptr->super_pane,
            wlmtk_box_element(&menu_ptr->box),
            env_ptr)) {
        wlmtk_menu_destroy(menu_ptr);
        return NULL;
    }
    menu_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_menu_element(menu_ptr), &_wlmtk_menu_element_vmt);

    wl_signal_init(&menu_ptr->events.open_changed);
    wl_signal_init(&menu_ptr->events.request_close);
    return menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_destroy(wlmtk_menu_t *menu_ptr)
{
    // Must destroy the items before the pane and box.
    bs_dllist_for_each(
        &menu_ptr->items,
        _wlmtk_menu_eliminate_item,
        menu_ptr);

    wlmtk_pane_fini(&menu_ptr->super_pane);
    wlmtk_box_fini(&menu_ptr->box);
    free(menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_menu_element(wlmtk_menu_t *menu_ptr)
{
    return wlmtk_pane_element(&menu_ptr->super_pane);
}

/* ------------------------------------------------------------------------- */
wlmtk_pane_t *wlmtk_menu_pane(wlmtk_menu_t *menu_ptr)
{
    return &menu_ptr->super_pane;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_events_t *wlmtk_menu_events(wlmtk_menu_t *menu_ptr)
{
    return &menu_ptr->events;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_set_open(wlmtk_menu_t *menu_ptr, bool opened)
{
    if (wlmtk_menu_element(menu_ptr)->visible == opened) return;

    wlmtk_element_set_visible(wlmtk_menu_element(menu_ptr), opened);

    if (NULL != menu_ptr->highlighted_menu_item_ptr) {
        wlmtk_menu_item_set_highlighted(
            menu_ptr->highlighted_menu_item_ptr, false);
        menu_ptr->highlighted_menu_item_ptr = NULL;
    }

    wl_signal_emit(&menu_ptr->events.open_changed, menu_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_menu_is_open(wlmtk_menu_t *menu_ptr)
{
    return wlmtk_menu_element(menu_ptr)->visible;
}


/* ------------------------------------------------------------------------- */
void wlmtk_menu_set_mode(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_mode_t mode)
{
    if (menu_ptr->mode == mode) return;
    menu_ptr->mode = mode;
    bs_dllist_for_each(
        &menu_ptr->items,
        _wlmtk_menu_set_item_mode,
        menu_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_add_item(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_item_t *menu_item_ptr)
{
    bs_dllist_push_back(
        &menu_ptr->items,
        wlmtk_dlnode_from_menu_item(menu_item_ptr));
    wlmtk_box_add_element_back(
        &menu_ptr->box,
        wlmtk_menu_item_element(menu_item_ptr));
    wlmtk_menu_item_set_mode(menu_item_ptr, menu_ptr->mode);
    wlmtk_menu_item_set_parent_menu(menu_item_ptr, menu_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_remove_item(wlmtk_menu_t *menu_ptr,
                            wlmtk_menu_item_t *menu_item_ptr)
{
    if (menu_ptr->highlighted_menu_item_ptr == menu_item_ptr) {
        menu_ptr->highlighted_menu_item_ptr = NULL;
    }
    wlmtk_menu_item_set_parent_menu(menu_item_ptr, NULL);
    wlmtk_box_remove_element(
        &menu_ptr->box,
        wlmtk_menu_item_element(menu_item_ptr));
    bs_dllist_remove(
        &menu_ptr->items,
        wlmtk_dlnode_from_menu_item(menu_item_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_request_item_highlight(
    wlmtk_menu_t *menu_ptr,
    wlmtk_menu_item_t *menu_item_ptr)
{
    if (menu_ptr->highlighted_menu_item_ptr == menu_item_ptr) return;

    if (NULL != menu_ptr->highlighted_menu_item_ptr) {
        wlmtk_menu_item_set_highlighted(
            menu_ptr->highlighted_menu_item_ptr, false);
        menu_ptr->highlighted_menu_item_ptr = NULL;
    }

    if (NULL != menu_item_ptr &&
        wlmtk_menu_item_set_highlighted(menu_item_ptr, true)) {
        menu_ptr->highlighted_menu_item_ptr = menu_item_ptr;
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Removes item from items, destroys it. */
void _wlmtk_menu_eliminate_item(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_menu_item_t *item_ptr = wlmtk_menu_item_from_dlnode(dlnode_ptr);
    wlmtk_menu_t *menu_ptr = ud_ptr;

    wlmtk_menu_remove_item(menu_ptr, item_ptr);
    wlmtk_element_destroy(wlmtk_menu_item_element(item_ptr));
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for bs_dllist_for_each: Sets the menu mode for the item.
 *
 * @param dlnode_ptr
 * @param ud_ptr
 */
void _wlmtk_menu_set_item_mode(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_menu_item_set_mode(
        wlmtk_menu_item_from_dlnode(dlnode_ptr),
        ((wlmtk_menu_t*)ud_ptr)->mode);
}

/* ------------------------------------------------------------------------- */
/** Wraps to dtor. Implements @ref wlmtk_element_vmt_t::destroy. */
void _wlmtk_menu_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmtk_menu_t *menu_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_t, super_pane.super_container.super_element);

    wlmtk_menu_destroy(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * If the menu is in right-click mode, acts on right-button events and signals
 * the menu to close.
 *
 * Implementation of @ref wlmtk_element_vmt_t::pointer_button.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return whether the button event was claimed.
 */
bool _wlmtk_menu_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_menu_t *menu_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_t, super_pane.super_container.super_element);

    bool rv = menu_ptr->orig_element_vmt.pointer_button(
        element_ptr, button_event_ptr);

    if (WLMTK_MENU_MODE_RIGHTCLICK == menu_ptr->mode &&
        BTN_RIGHT == button_event_ptr->button) {
        wl_signal_emit(&menu_ptr->events.request_close, NULL);
        rv = true;
    }

    return rv;
}

/* == Unit tests =========================================================== */

static void test_pointer_highlight(bs_test_t *test_ptr);
static void test_set_mode(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_menu_test_cases[] = {
    { 1, "pointer_highlight", test_pointer_highlight },
    { 1, "set_mode", test_set_mode },
    { 0, NULL, NULL }
};

/** For tests: Meu style to apply. */
static const wlmtk_menu_style_t _test_style = {
    .margin = { .width = 2 },
    .border = { .width = 2 },
    .item = { .height = 10, .bezel_width=1, .width = 100 }
};

/* ------------------------------------------------------------------------- */
/** Tests that pointer moves highlight the items. */
void test_pointer_highlight(bs_test_t *test_ptr)
{
    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(&_test_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);
    wlmtk_element_t *me = wlmtk_menu_element(menu_ptr);

    wlmtk_menu_item_t *i1 = wlmtk_menu_item_create(&_test_style.item, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i1);
    wlmtk_menu_add_item(menu_ptr, i1);
    wlmtk_menu_item_t *i2 = wlmtk_menu_item_create(&_test_style.item, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i2);
    wlmtk_menu_add_item(menu_ptr, i2);

    // Motion into first element: highlight it.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, 9, 5, 1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_HIGHLIGHTED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(test_ptr, i1, menu_ptr->highlighted_menu_item_ptr);

    // Motion into second element: highlight that, un-highlight the other.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, 9, 15, 1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_HIGHLIGHTED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, i2, menu_ptr->highlighted_menu_item_ptr);

    // Move into the margin area: Both are un-highlighted.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, 9, 10, 1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, menu_ptr->highlighted_menu_item_ptr);

    // Move entirely outside: Both remain just enabled.
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_pointer_motion(me, 9, 55, 1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, menu_ptr->highlighted_menu_item_ptr);

    // Back into second element: highlight that.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, 9, 15, 1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_HIGHLIGHTED, wlmtk_menu_item_get_state(i2));

    // Remove one item explicitly, the other one to destroy during cleanup.
    wlmtk_menu_remove_item(menu_ptr, i1);
    wlmtk_menu_item_destroy(i1);
    wlmtk_menu_destroy(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests setting the menu's mode. */
void test_set_mode(bs_test_t *test_ptr)
{
    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(&_test_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);

    wlmtk_menu_item_t *item1_ptr = wlmtk_menu_item_create(
        &_test_style.item, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, item1_ptr);
    wlmtk_menu_add_item(menu_ptr, item1_ptr);

    // Setting the mode must propagate.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_NORMAL,
        wlmtk_menu_item_get_mode(item1_ptr));
    wlmtk_menu_set_mode(menu_ptr, WLMTK_MENU_MODE_RIGHTCLICK);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_RIGHTCLICK,
        wlmtk_menu_item_get_mode(item1_ptr));

    // A new item must get the mode applied.
    wlmtk_menu_item_t *item2_ptr = wlmtk_menu_item_create(
        &_test_style.item, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, item2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_NORMAL,
        wlmtk_menu_item_get_mode(item2_ptr));
    wlmtk_menu_add_item(menu_ptr, item2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_RIGHTCLICK,
        wlmtk_menu_item_get_mode(item2_ptr));

    // Setting the mode must propagate to all.
    wlmtk_menu_set_mode(menu_ptr, WLMTK_MENU_MODE_NORMAL);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_NORMAL,
        wlmtk_menu_item_get_mode(item1_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_MODE_NORMAL,
        wlmtk_menu_item_get_mode(item2_ptr));

    wlmtk_menu_destroy(menu_ptr);
}

/* == End of menu.c ======================================================== */
