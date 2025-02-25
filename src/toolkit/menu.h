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

#include "box.h"
#include "env.h"
#include "menu_item.h"
#include "pane.h"

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

/** Events of the popup menu. */
typedef struct {
    /** Popup menu requests to be closed. */
    struct wl_signal          request_close;
} wlmtk_menu_events_t;

/**
 * Creates a menu.
 *
 * @param style_ptr
 * @param env_ptr
 *
 * @return Pointer to the menu state or NULL on error.
 */
wlmtk_menu_t *wlmtk_menu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr);

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
 * Sets the mode of the menu.
 *
 * @param menu_ptr
 * @param mode
 */
void wlmtk_menu_set_mode(wlmtk_menu_t *menu_ptr,
                         wlmtk_menu_mode_t mode);

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
 * Requests that menu_item_ptr be highlighted.
 *
 * @param menu_ptr
 * @param menu_item_ptr
 */
void wlmtk_menu_request_item_highlight(
    wlmtk_menu_t *menu_ptr,
    wlmtk_menu_item_t *menu_item_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_menu_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_MENU_H__ */
/* == End of menu.h ======================================================== */
