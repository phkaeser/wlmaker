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

/** Forward declaration: State of the menu item. */
typedef struct _wlmtk_menu_item_t wlmtk_menu_item_t;
/** Forward declaration: Virtual method table of the menu item. */
typedef struct _wlmtk_menu_item_vmt_t wlmtk_menu_item_vmt_t;
/** Forward declaration: Style of a menu item. */
typedef struct _wlmtk_menu_item_style_t wlmtk_menu_item_style_t;

#include "buffer.h"
#include "element.h"
#include "env.h"
#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** States a menu item can be in. */
typedef enum {
    WLMTK_MENU_ITEM_ENABLED,
    WLMTK_MENU_ITEM_HIGHLIGHTED,
    WLMTK_MENU_ITEM_DISABLED
} wlmtk_menu_item_state_t;

/** Modes of the menu. */
typedef enum {
    /** Normal (window) mode of a menu: Left button click triggers items. */
    WLMTK_MENU_MODE_NORMAL,
    /**
     * Right-click mode of menu: Menu is invoked while right button is pressed.
     * Releasing the right button triggers items.
     */
    WLMTK_MENU_MODE_RIGHTCLICK
} wlmtk_menu_mode_t;

/** Events of the menu item. */
typedef struct {
    /** The menu item was triggered, by a click or key action. */
    struct wl_signal          triggered;
} wlmtk_menu_item_events_t;

/** Menu item style. */
struct _wlmtk_menu_item_style_t {
    /** Fill style. */
    wlmtk_style_fill_t        fill;
    /** Fill style when disabled. */
    wlmtk_style_fill_t        highlighted_fill;
    /** Style of the font used in the menu item. */
    wlmtk_style_font_t        font;
    /** Height of the menu item, in pixels. */
    uint64_t                  height;
    /** Width of the bezel, in pixels. */
    uint64_t                  bezel_width;
    /** Text color. */
    uint32_t                  enabled_text_color;
    /** Text color when highlighted. */
    uint32_t                  highlighted_text_color;
    /** Text color when disabled. */
    uint32_t                  disabled_text_color;
    /** Width of the item. */
    uint64_t                  width;
};

/**
 * Creates a menu item.
 *
 * Note: Menu items are created as 'visible' elements.
 *
 * @param style_ptr
 * @param env_ptr
 *
 * @return Pointer to the menu item state, or NULL on failure.
 */
wlmtk_menu_item_t *wlmtk_menu_item_create(
    const wlmtk_menu_item_style_t *style_ptr,
    wlmtk_env_t *env_ptr);

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
 * Sets the menu's mode for this item.
 *
 * @param menu_item_ptr
 * @param mode
 */
void wlmtk_menu_item_set_mode(
    wlmtk_menu_item_t *menu_item_ptr,
    wlmtk_menu_mode_t mode);

/** @return the mode of this item. */
wlmtk_menu_mode_t wlmtk_menu_item_get_mode(
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
