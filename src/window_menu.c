/* ========================================================================= */
/**
 * @file window_menu.c
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

#include "window_menu.h"

/* == Declarations ========================================================= */

/** State of the window menu. */
struct _wlmaker_window_menu_t {
    /** The window this menu belongs to. */
    wlmtk_window_t            *window_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_window_menu_t *wlmaker_window_menu_create(
    wlmtk_window_t *window_ptr)
{
    wlmaker_window_menu_t *window_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_window_menu_t));
    if (NULL == window_menu_ptr) return NULL;
    window_menu_ptr->window_ptr = window_ptr;

    return window_menu_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Destroys the window menu. */
void _wlmaker_window_menu_destroy(wlmaker_window_menu_t *window_menu_ptr)
{
    free(window_menu_ptr);
}

/* == End of window_menu.c ================================================= */
