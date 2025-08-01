/* ========================================================================= */
/**
 * @file menu.h
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
#ifndef __WLMTK_MENU_H__
#define __WLMTK_MENU_H__

/** Forward declaration: Menu handle. */
typedef struct _wlmtk_menu_t wlmtk_menu_t;

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stddef.h>
#include <wayland-server-core.h>

#include "box.h"  // IWYU pragma: keep
#include "element.h"
#include "menu_item.h"  // IWYU pragma: keep
#include "pane.h"
#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Style definition for the menu. */
typedef struct  {
    /** Margin. */
    wlmtk_margin_style_t      margin;
    /** Border. */
    wlmtk_margin_style_t      border;
    /** Item's style. */
    wlmtk_menu_item_style_t   item;
} wlmtk_menu_style_t;

/** Modes of the menu. */
enum wlmtk_menu_mode {
    /** Normal (window) mode of a menu: Left button click triggers items. */
    WLMTK_MENU_MODE_NORMAL,
    /**
     * Right-click mode of menu: Menu is invoked while right button is pressed.
     * Releasing the right button triggers items.
     */
    WLMTK_MENU_MODE_RIGHTCLICK
};

/** Events of the popup menu. */
typedef struct {
    /**
     * The menu was opened or closed. Check through @ref wlmtk_menu_is_open.
     * The signal's argument is a pointer to the menu that opened/closed.
     */
    struct wl_signal          open_changed;
    /** Popup menu requests to be closed. */
    struct wl_signal          request_close;
    /** The dtor has been called. */
    struct wl_signal          destroy;
} wlmtk_menu_events_t;

/**
 * Creates a menu.
 *
 * @param style_ptr
 *
 * @return Pointer to the menu state or NULL on error.
 */
wlmtk_menu_t *wlmtk_menu_create(
    const wlmtk_menu_style_t *style_ptr);

/**
 * Destroys the menu.
 *
 * @param menu_ptr
 */
void wlmtk_menu_destroy(wlmtk_menu_t *menu_ptr);

/** @return pointer to the menu's @ref wlmtk_element_t superclass. */
wlmtk_element_t *wlmtk_menu_element(wlmtk_menu_t *menu_ptr);

/** @return pointer to the menu's @ref wlmtk_pane_t superclass. */
wlmtk_pane_t *wlmtk_menu_pane(wlmtk_menu_t *menu_ptr);

/** @return a pointer to @ref wlmtk_menu_t::events. */
wlmtk_menu_events_t *wlmtk_menu_events(wlmtk_menu_t *menu_ptr);

/**
 * Opens the menu: Makes it visible or invisible, and resets state
 * if needed.
 *
 * @param menu_ptr
 * @param opened
 */
void wlmtk_menu_set_open(wlmtk_menu_t *menu_ptr, bool opened);

/** @return whether the menu is open, ie. visible. */
bool wlmtk_menu_is_open(wlmtk_menu_t *menu_ptr);

/**
 * Sets the mode of the menu.
 *
 * @param menu_ptr
 * @param mode
 */
void wlmtk_menu_set_mode(wlmtk_menu_t *menu_ptr,
                         enum wlmtk_menu_mode mode);

/** @return mode of the menu. @see wlmtk_menu_set_mode. */
enum wlmtk_menu_mode wlmtk_menu_get_mode(wlmtk_menu_t *menu_ptr);

/**
 * Adds a menu item to the menu.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_add_item(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_item_t *menu_item_ptr);

/**
 * Removes a menu item from the menu.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_remove_item(wlmtk_menu_t *menu_ptr,
                            wlmtk_menu_item_t *menu_item_ptr);

/**
 * Registers `menu_item_ptr` as this submenu's parent item.
 *
 * @protected Must be called only from @ref wlmtk_menu_item_set_submenu.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_set_parent_item(wlmtk_menu_t *menu_ptr,
                                wlmtk_menu_item_t *menu_item_ptr);

/** @return the parent menu item, if this is a submenu. NULL otherwise. */
wlmtk_menu_item_t *wlmtk_menu_get_parent_item(wlmtk_menu_t *menu_ptr);

/**
 * Requests that menu_item_ptr be highlighted.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_request_item_highlight(
    wlmtk_menu_t *menu_ptr,
    wlmtk_menu_item_t *menu_item_ptr);

/**
 * @param menu_ptr
 *
 * @return the size of @ref wlmtk_menu_t::items.
 */
size_t wlmtk_menu_items_size(wlmtk_menu_t *menu_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_menu_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_H__ */
/* == End of menu.h ======================================================== */
