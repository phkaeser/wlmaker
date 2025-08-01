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

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdlib.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "input.h"
#include "util.h"

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
    /** If this is a submenu, this points to the parent menu item. */
    wlmtk_menu_item_t         *parent_menu_item_ptr;
    /** Current mode of the menu. */
    enum wlmtk_menu_mode      mode;
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
static bool _wlmtk_menu_element_keyboard_sym(
    wlmtk_element_t *element_ptr,
    xkb_keysym_t keysym,
    enum xkb_key_direction direction,
    uint32_t modifiers);
static bs_dllist_node_t *_wlmtk_menu_this_or_next_non_disabled_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    bs_dllist_node_iterator_t iterator);

/* == Data ================================================================= */

/** The superclass' element virtual method table. */
static const wlmtk_element_vmt_t _wlmtk_menu_element_vmt = {
    .destroy = _wlmtk_menu_element_destroy,
    .pointer_button = _wlmtk_menu_element_pointer_button,
};

/** Extra override for the contained element. */
// TODO(kaeser@gube.ch): Migrate into @ref _wlmtk_menu_element_vmt. */
static const wlmtk_element_vmt_t _wlmtk_menu_box_element_vmt = {
    .keyboard_sym = _wlmtk_menu_element_keyboard_sym,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_menu_create(const wlmtk_menu_style_t *style_ptr)
{
    wlmtk_menu_t *menu_ptr = logged_calloc(1, sizeof(wlmtk_menu_t));
    if (NULL == menu_ptr) return NULL;
    menu_ptr->style = *style_ptr;

    if (!wlmtk_box_init(
            &menu_ptr->box,
            WLMTK_BOX_VERTICAL,
            &menu_ptr->style.margin)) {
        wlmtk_menu_destroy(menu_ptr);
        return NULL;
    }

    if (!wlmtk_pane_init(
            &menu_ptr->super_pane,
            wlmtk_box_element(&menu_ptr->box))) {
        wlmtk_menu_destroy(menu_ptr);
        return NULL;
    }
    menu_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_menu_element(menu_ptr), &_wlmtk_menu_element_vmt);
    // TODO(kaeser@gubbe.ch): That should work directly on the pane. Update
    // this, once having eliminated wlmtk_content_t and the ugly hack in
    // @ref wlmaker_root_menu_create.
    wlmtk_element_extend(
        wlmtk_box_element(&menu_ptr->box),
        &_wlmtk_menu_box_element_vmt);

    wl_signal_init(&menu_ptr->events.open_changed);
    wl_signal_init(&menu_ptr->events.request_close);
    wl_signal_init(&menu_ptr->events.destroy);
    return menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_destroy(wlmtk_menu_t *menu_ptr)
{
    wl_signal_emit(&menu_ptr->events.destroy, NULL);

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
                         enum wlmtk_menu_mode mode)
{
    if (menu_ptr->mode == mode) return;
    menu_ptr->mode = mode;
    bs_dllist_for_each(
        &menu_ptr->items,
        _wlmtk_menu_set_item_mode,
        menu_ptr);
}

/* ------------------------------------------------------------------------- */
enum wlmtk_menu_mode wlmtk_menu_get_mode(wlmtk_menu_t *menu_ptr)
{
    return menu_ptr->mode;
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
void wlmtk_menu_set_parent_item(wlmtk_menu_t *menu_ptr,
                                wlmtk_menu_item_t *menu_item_ptr)
{
    menu_ptr->parent_menu_item_ptr = menu_item_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmtk_menu_get_parent_item(wlmtk_menu_t *menu_ptr)
{
    return menu_ptr->parent_menu_item_ptr;
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

/* ------------------------------------------------------------------------- */
size_t wlmtk_menu_items_size(wlmtk_menu_t *menu_ptr)
{
    return bs_dllist_size(&menu_ptr->items);
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

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::keyboard_sym. Translated keys. */
bool _wlmtk_menu_element_keyboard_sym(
    wlmtk_element_t *element_ptr,
    xkb_keysym_t keysym,
    enum xkb_key_direction direction,
    uint32_t modifiers)
{
    wlmtk_menu_t *menu_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_t, box.super_container.super_element);
    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_menu_item(
        menu_ptr->highlighted_menu_item_ptr);
    bs_dllist_node_iterator_t node_iterator;

    // If there is a submenu with an already highlighted item: Forward key.
    wlmtk_menu_t *submenu_ptr = NULL;
    if (NULL != menu_ptr->highlighted_menu_item_ptr) {
        submenu_ptr = wlmtk_menu_item_get_submenu(
            menu_ptr->highlighted_menu_item_ptr);
        if (NULL != submenu_ptr &&
            NULL != submenu_ptr->highlighted_menu_item_ptr) {
            return _wlmtk_menu_element_keyboard_sym(
                &submenu_ptr->box.super_container.super_element,
                keysym,
                direction,
                modifiers);
        }
    }

    // Otherwise: Only interested in key press events, not release.
    if (direction != XKB_KEY_DOWN) return false;

    switch (keysym) {
    case XKB_KEY_Escape:
        if (NULL != wlmtk_menu_get_parent_item(menu_ptr)) {
            wlmtk_menu_request_item_highlight(menu_ptr, NULL);
        } else {
            wl_signal_emit(&menu_ptr->events.request_close, NULL);
        }
        return true;

    case XKB_KEY_Return:
        if (NULL != menu_ptr->highlighted_menu_item_ptr) {
            wlmtk_menu_item_trigger(menu_ptr->highlighted_menu_item_ptr);
        }
        return true;

    case XKB_KEY_Left:
        if (NULL == wlmtk_menu_get_parent_item(menu_ptr)) return false;
        wlmtk_menu_request_item_highlight(menu_ptr, NULL);
        return true;

    case XKB_KEY_Right:
        if (NULL != submenu_ptr &&
            NULL == submenu_ptr->highlighted_menu_item_ptr) {
            wlmtk_menu_item_t *item_ptr = wlmtk_menu_item_from_dlnode(
                _wlmtk_menu_this_or_next_non_disabled_dlnode(
                    submenu_ptr->items.head_ptr,
                    bs_dllist_node_iterator_forward));
            if (NULL != item_ptr) {
                wlmtk_menu_request_item_highlight(submenu_ptr, item_ptr);
                return true;
            }
        }
        return false;

    case XKB_KEY_Home:
        node_iterator = bs_dllist_node_iterator_forward;
        dlnode_ptr = menu_ptr->items.head_ptr;
        break;

    case XKB_KEY_End:
        node_iterator = bs_dllist_node_iterator_backward;
        dlnode_ptr = menu_ptr->items.tail_ptr;
        break;

    case XKB_KEY_Down:
        node_iterator = bs_dllist_node_iterator_forward;
        dlnode_ptr = _wlmtk_menu_this_or_next_non_disabled_dlnode(
            node_iterator(dlnode_ptr), node_iterator);
        if (NULL == dlnode_ptr) dlnode_ptr = menu_ptr->items.head_ptr;
        break;

    case XKB_KEY_Up:
        node_iterator = bs_dllist_node_iterator_backward;
        dlnode_ptr = _wlmtk_menu_this_or_next_non_disabled_dlnode(
            node_iterator(dlnode_ptr), node_iterator);
        if (NULL == dlnode_ptr) dlnode_ptr = menu_ptr->items.tail_ptr;
        break;

    default:
        return false;
    }

    wlmtk_menu_item_t *item_ptr = wlmtk_menu_item_from_dlnode(
        _wlmtk_menu_this_or_next_non_disabled_dlnode(
            dlnode_ptr, node_iterator));
    if (NULL != item_ptr) {
        wlmtk_menu_request_item_highlight(menu_ptr, item_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the given-or-next non-disabled menu item, from `dlnode_ptr`.
 *
 * @param dlnode_ptr
 * @param iterator            Iterator to reach the "next" item.
 *
 * @return A pointer to @ref wlmtk_menu_item_t::dlnode of `dlnode_ptr` (or the
 *     the next non-disabled item), or NULL.
 */
bs_dllist_node_t *_wlmtk_menu_this_or_next_non_disabled_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    bs_dllist_node_iterator_t iterator)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_from_dlnode(dlnode_ptr);
    if (NULL == menu_item_ptr) return NULL;

    switch (wlmtk_menu_item_get_state(menu_item_ptr)) {
    case WLMTK_MENU_ITEM_ENABLED:
    case WLMTK_MENU_ITEM_HIGHLIGHTED:
        return dlnode_ptr;

    case WLMTK_MENU_ITEM_DISABLED:
        break;
    }
    return _wlmtk_menu_this_or_next_non_disabled_dlnode(
        iterator(dlnode_ptr), iterator);
}

/* == Unit tests =========================================================== */

static void test_pointer_highlight(bs_test_t *test_ptr);
static void test_set_mode(bs_test_t *test_ptr);
static void test_keyboard_navigation(bs_test_t *test_ptr);
static void test_keyboard_navigation_nested(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_menu_test_cases[] = {
    { 1, "pointer_highlight", test_pointer_highlight },
    { 1, "set_mode", test_set_mode },
    { 1, "keyboard_navigation", test_keyboard_navigation },
    { 1, "keyboard_navigation_nested", test_keyboard_navigation_nested },
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
    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(&_test_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);
    wlmtk_element_t *me = wlmtk_menu_element(menu_ptr);
    wlmtk_element_set_visible(me, true);

    wlmtk_menu_item_t *i1 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i1);
    wlmtk_menu_add_item(menu_ptr, i1);
    wlmtk_menu_item_t *i2 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i2);
    wlmtk_menu_add_item(menu_ptr, i2);

    // Motion into first element: highlight it.
    wlmtk_pointer_motion_event_t e = { .x = 9, .y = 5 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_HIGHLIGHTED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(test_ptr, i1, menu_ptr->highlighted_menu_item_ptr);

    // Motion into second element: highlight that, un-highlight the other.
    e = (wlmtk_pointer_motion_event_t){ .x = 9, .y = 15 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_HIGHLIGHTED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, i2, menu_ptr->highlighted_menu_item_ptr);

    // Move into the margin area: Both are un-highlighted.
    e = (wlmtk_pointer_motion_event_t){ .x = 9, .y = 10 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, menu_ptr->highlighted_menu_item_ptr);

    // Move entirely outside: Both remain just enabled.
    e = (wlmtk_pointer_motion_event_t){ .x = 9, .y = 55 };
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i1));
    BS_TEST_VERIFY_EQ(
        test_ptr, WLMTK_MENU_ITEM_ENABLED, wlmtk_menu_item_get_state(i2));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, menu_ptr->highlighted_menu_item_ptr);

    // Back into second element: highlight that.
    e = (wlmtk_pointer_motion_event_t){ .x = 9, .y = 15 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
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
    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(&_test_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);

    wlmtk_util_test_listener_t destroy_test_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_menu_events(menu_ptr)->destroy,
        &destroy_test_listener);

    wlmtk_menu_item_t *item1_ptr = wlmtk_menu_item_create(
        &_test_style.item);
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
        &_test_style.item);
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

    BS_TEST_VERIFY_EQ(test_ptr, 0, destroy_test_listener.calls);
    wlmtk_menu_destroy(menu_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, destroy_test_listener.calls);
}

/* ------------------------------------------------------------------------- */
/** Tests keyboard navigation within the menu. */
void test_keyboard_navigation(bs_test_t *test_ptr)
{
    static const wlmtk_menu_item_state_t HL = WLMTK_MENU_ITEM_HIGHLIGHTED;

    wlmtk_menu_t *menu_ptr = wlmtk_menu_create(&_test_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);
    wlmtk_element_t *me = wlmtk_menu_element(menu_ptr);
    wlmtk_element_set_visible(me, true);
    // TODO(kaeser@gubbe.ch): Replace with me, once not relying on overriding
    // the wlmtk_box_t element.
    wlmtk_element_t *ke = wlmtk_box_element(&menu_ptr->box);

    wlmtk_util_test_listener_t request_close_test_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_menu_events(menu_ptr)->request_close,
        &request_close_test_listener);

    // Test items: disabled, enabled, enabled, disabled, enabled.
    struct {
        wlmtk_menu_item_t *item;
        bool enabled;
        wlmtk_util_test_listener_t triggered_test_listener;
    } items[6] = {
        [1] = { .enabled = true },
        [2] = { .enabled = true },
        [4] = { .enabled = true },
    };
    for (int i = 0; i < 5; ++i) {
        items[i].item = wlmtk_menu_item_create(&_test_style.item);
        BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, items[i].item);
        wlmtk_util_connect_test_listener(
            &wlmtk_menu_item_events(items[i].item)->triggered,
            &items[i].triggered_test_listener);
        wlmtk_menu_add_item(menu_ptr, items[i].item);
        wlmtk_menu_item_set_enabled(items[i].item, items[i].enabled);
    }

    // Move pointer over items[2].
    wlmtk_pointer_motion_event_t e = { .x = 9, .y = 25 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[2].item));

    // Down key: Moves down, items[3] is disabled => land at items[4].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Down, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[4].item));

    // Down key once more: Wrap around, land at items[1].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Down, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ( test_ptr, HL, wlmtk_menu_item_get_state(items[1].item));

    // Up key: Wraps around, land at items[4].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Up, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[4].item));

    // Up key: Moves up once more, at items[2].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Up, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[2].item));

    // End key: Jump to items[4].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_End, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[4].item));

    // A motion, within items[2]. Re-gain focus there.
    e = (wlmtk_pointer_motion_event_t){ .x = 8, .y = 25 };
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(me, &e));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[2].item));

    // Home key: Jump to items[1].
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Home, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(items[1].item));

    // Return key: Trigger items[1].
    wlmtk_util_clear_test_listener(&items[1].triggered_test_listener);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Return, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, 1, items[1].triggered_test_listener.calls);

    // Escape key: Request to close the menu.
    wlmtk_util_clear_test_listener(&request_close_test_listener);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Escape, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_close_test_listener.calls);

    // Keys that were released: No trigger.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Escape, XKB_KEY_UP, 0));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_close_test_listener.calls);

    wlmtk_menu_destroy(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests keyboard navigation with nested menus. */
void test_keyboard_navigation_nested(bs_test_t *test_ptr)
{
    static const wlmtk_menu_item_state_t HL = WLMTK_MENU_ITEM_HIGHLIGHTED;

    // Menu m0 with two items (i00, i01).
    wlmtk_menu_t *m0 = wlmtk_menu_create(&_test_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, m0);
    wlmtk_menu_item_t *i00 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i00);
    wlmtk_menu_add_item(m0, i00);
    wlmtk_menu_item_t *i01 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i01);
    wlmtk_menu_add_item(m0, i01);

    wlmtk_util_test_listener_t request_close_test_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_menu_events(m0)->request_close,
        &request_close_test_listener);

    // i01 has submenu m1 with two items (i10, i11).
    wlmtk_menu_t *m1 = wlmtk_menu_create(&_test_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, m1);
    wlmtk_menu_item_set_submenu(i01, m1);
    wlmtk_menu_item_t *i10 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i10);
    wlmtk_menu_add_item(m1, i10);
    wlmtk_menu_item_t *i11 = wlmtk_menu_item_create(&_test_style.item);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, i11);
    wlmtk_menu_add_item(m1, i11);
    wlmtk_element_t *me = wlmtk_menu_element(m0);
    wlmtk_element_set_visible(me, true);

    // TODO(kaeser@gubbe.ch): Replace with me, once not relying on overriding
    // the wlmtk_box_t element.
    wlmtk_element_t *ke = wlmtk_box_element(&m0->box);

    // Home key, highlights i00.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Home, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i00));

    // Right key. Does not do anything.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Right, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i00));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_menu_is_open(m1));

    // Down key. Highlights i01, opens m1, but no highlight there.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Down, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_menu_is_open(m1));
    BS_TEST_VERIFY_NEQ(test_ptr, HL, wlmtk_menu_item_get_state(i10));

    // Right key. Now highlights i10. i01 remains highlighted.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Right, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i10));

    // Down key. Now highlights i11. i01 remains highlighted.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Down, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i11));

    // Left key. Moves highlight back, but keeps submenu open.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Left, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_menu_is_open(m1));
    BS_TEST_VERIFY_NEQ(test_ptr, HL, wlmtk_menu_item_get_state(i10));
    BS_TEST_VERIFY_NEQ(test_ptr, HL, wlmtk_menu_item_get_state(i11));

    // Right key. Highlights the submenu with i10 again.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Right, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i10));

    // Esc key, while submenu has highlight. Same action as left key.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Escape, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, HL, wlmtk_menu_item_get_state(i01));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_menu_is_open(m1));
    BS_TEST_VERIFY_NEQ(test_ptr, HL, wlmtk_menu_item_get_state(i10));
    BS_TEST_VERIFY_NEQ(test_ptr, HL, wlmtk_menu_item_get_state(i11));
    BS_TEST_VERIFY_EQ(test_ptr, 0, request_close_test_listener.calls);

    // Esc key, once more. Must close.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_keyboard_sym(ke, XKB_KEY_Escape, XKB_KEY_DOWN, 0));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_close_test_listener.calls);

    wlmtk_menu_destroy(m0);
}

/* == End of menu.c ======================================================== */
