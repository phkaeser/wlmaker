/* ========================================================================= */
/**
 * @file menu_item.h
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
#ifndef __WLMTK_MENU_ITEM_H__
#define __WLMTK_MENU_ITEM_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <wayland-server-core.h>

#include "element.h"
#include "style.h"

/** Forward declaration: State of the menu item. */
typedef struct _wlmtk_menu_item_t wlmtk_menu_item_t;

#include "menu.h"  // IWYU pragma: keep

enum wlmtk_menu_mode;

/** Forward declaration: Virtual method table of the menu item. */
typedef struct _wlmtk_menu_item_vmt_t wlmtk_menu_item_vmt_t;

/** States a menu item can be in. */
typedef enum {
    WLMTK_MENU_ITEM_ENABLED,
    WLMTK_MENU_ITEM_HIGHLIGHTED,
    WLMTK_MENU_ITEM_DISABLED
} wlmtk_menu_item_state_t;

/** Events of the menu item. */
typedef struct {
    /** The menu item was triggered, by a click or key action. */
    struct wl_signal          triggered;
    /** The menu item is being destroyed. */
    struct wl_signal          destroy;
} wlmtk_menu_item_events_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a menu item.
 *
 * Note: Menu items are created as 'visible' elements.
 *
 * @param style_ptr
 *
 * @return Pointer to the menu item state, or NULL on failure.
 */
wlmtk_menu_item_t *wlmtk_menu_item_create(
    const wlmtk_menu_item_style_t *style_ptr);

/**
 * Destroys the menu item.
 *
 * @param menu_item_ptr
 */
void wlmtk_menu_item_destroy(wlmtk_menu_item_t *menu_item_ptr);

/** Returns pointer to the menu item's @ref wlmtk_menu_item_t::events. */
wlmtk_menu_item_events_t *wlmtk_menu_item_events(
    wlmtk_menu_item_t *menu_item_ptr);

/**
 * Sets the menu this item belongs to.
 *
 * Private: Should only be called by @ref wlmtk_menu_add_item.
 *
 * @param menu_item_ptr
 * @param menu_ptr            May be NULL, to detach from menu.
 */
void wlmtk_menu_item_set_parent_menu(
    wlmtk_menu_item_t *menu_item_ptr,
    wlmtk_menu_t *menu_ptr);

/**
 * Sets the submenu for this menu item.
 *
 * @param menu_item_ptr
 * @param submenu_ptr         The submenu to set for the item. The item will
 *                            take ownership of `submenu_ptr`, and destroy it
 *                            when the item ist destroyed. Unless the submenu
 *                            is detached again, by calling with a NULL
 *                            argument.
 */
void wlmtk_menu_item_set_submenu(
    wlmtk_menu_item_t *menu_item_ptr,
    wlmtk_menu_t *submenu_ptr);

/**
 * Sets the menu's mode for this item.
 *
 * @param menu_item_ptr
 * @param mode
 */
void wlmtk_menu_item_set_mode(
    wlmtk_menu_item_t *menu_item_ptr,
    enum wlmtk_menu_mode mode);

/** @return the mode of this item. */
enum wlmtk_menu_mode wlmtk_menu_item_get_mode(
    wlmtk_menu_item_t *menu_item_ptr);

/** @return The state of the item, @ref wlmtk_menu_item_t::state. */
wlmtk_menu_item_state_t wlmtk_menu_item_get_state(
    wlmtk_menu_item_t *menu_item_ptr);

/**
 * Sets or updates the text for the menu item.
 *
 * @param menu_item_ptr
 * @param text_ptr
 */
bool wlmtk_menu_item_set_text(
    wlmtk_menu_item_t *menu_item_ptr,
    const char *text_ptr);

/**
 * Sets whether the menu item is enabled or disabled.
 *
 * @param menu_item_ptr
 * @param enabled
 */
void wlmtk_menu_item_set_enabled(
    wlmtk_menu_item_t *menu_item_ptr,
    bool enabled);

/**
 * Sets the menu item as highlighted.
 *
 * @param menu_item_ptr
 * @param highlighted
 *
 * @return false if the menu was not enabled, ie. could not be highlighted.
 */
bool wlmtk_menu_item_set_highlighted(
    wlmtk_menu_item_t *menu_item_ptr,
    bool highlighted);

/** Returns pointer to @ref wlmtk_menu_item_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_menu_item(
    wlmtk_menu_item_t *menu_item_ptr);

/** Returns the base @ref wlmtk_menu_item_t from `dlnode_ptr`. */
wlmtk_menu_item_t *wlmtk_menu_item_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/** Returns a pointer to the superclass @ref wlmtk_element_t. */
wlmtk_element_t *wlmtk_menu_item_element(wlmtk_menu_item_t *menu_item_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_menu_item_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_ITEM_H__ */
/* == End of menu_item.h =================================================== */
