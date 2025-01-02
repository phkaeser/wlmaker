/* ========================================================================= */
/**
 * @file tl_menu.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "tl_menu.h"

#include "action_item.h"

/* == Declarations ========================================================= */

/** State of a toplevel's window menu. */
struct _wlmaker_tl_menu_t {
    /** Pointer to the window's @ref wlmtk_menu_t. */
    wlmtk_menu_t              *menu_ptr;
};

/** Temporary: Struct for defining an item for the window menu. */
typedef struct {
    /** Text to use for the menu item. */
    const char                *text_ptr;
    /** Action to be executed for that menu item. */
    wlmaker_action_t          action;
} wlmaker_window_menu_item_t;

/* == Data ================================================================= */
/** Menu items for the XDG toplevel's window menu. */
static const wlmaker_window_menu_item_t _xdg_toplevel_menu_items[] = {
    { "Maximize", WLMAKER_ACTION_WINDOW_MAXIMIZE },
    { "Unmaximize", WLMAKER_ACTION_WINDOW_UNMAXIMIZE },
    { "Fullscreen", WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN },
    { "Shade", WLMAKER_ACTION_WINDOW_SHADE },
    { "Unshade", WLMAKER_ACTION_WINDOW_UNSHADE },
    { "To prev. workspace", WLMAKER_ACTION_WINDOW_TO_PREVIOUS_WORKSPACE },
    { "To next workspace", WLMAKER_ACTION_WINDOW_TO_NEXT_WORKSPACE },
    { "Close", WLMAKER_ACTION_WINDOW_CLOSE },
    { NULL, 0 }  // Sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_tl_menu_t *wlmaker_tl_menu_create(
    wlmtk_window_t *window_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_tl_menu_t *tl_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_tl_menu_t));
    if (NULL == tl_menu_ptr) return NULL;
    tl_menu_ptr->menu_ptr = wlmtk_window_menu(window_ptr);

    for (const wlmaker_window_menu_item_t *i_ptr = &_xdg_toplevel_menu_items[0];
         i_ptr->text_ptr != NULL;
         ++i_ptr) {

        wlmaker_action_item_t *action_item_ptr = wlmaker_action_item_create(
            i_ptr->text_ptr,
            &server_ptr->style.menu.item,
            i_ptr->action,
            server_ptr,
            server_ptr->env_ptr);
        if (NULL == action_item_ptr) {
            wlmaker_tl_menu_destroy(tl_menu_ptr);
            return NULL;
        }
        wlmtk_menu_add_item(
            tl_menu_ptr->menu_ptr,
            wlmaker_action_item_menu_item(action_item_ptr));
    }


    return tl_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_tl_menu_destroy(wlmaker_tl_menu_t *tl_menu_ptr)
{
    free(tl_menu_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of tl_menu.c ===================================================== */
