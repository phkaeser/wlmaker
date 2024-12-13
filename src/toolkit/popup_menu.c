/* ========================================================================= */
/**
 * @file popup_menu.c
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

#include "popup_menu.h"

#include "menu.h"

/* == Declarations ========================================================= */

/** State of the popup menu. */
struct _wlmtk_popup_menu_t {
    /** Wrapped as a popup. */
    wlmtk_popup_t             super_popup;
    /** The contained menu. */
    wlmtk_menu_t              menu;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_popup_menu_t *wlmtk_popup_menu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_popup_menu_t *popup_menu_ptr = logged_calloc(
        1, sizeof(wlmtk_popup_menu_t));
    if (NULL == popup_menu_ptr) return NULL;

    if (!wlmtk_menu_init(&popup_menu_ptr->menu, style_ptr, env_ptr)) {
        wlmtk_popup_menu_destroy(popup_menu_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_menu_element(&popup_menu_ptr->menu), true);

    if (!wlmtk_popup_init(&popup_menu_ptr->super_popup,
                          env_ptr,
                          wlmtk_menu_element(&popup_menu_ptr->menu))) {
        wlmtk_popup_menu_destroy(popup_menu_ptr);
        return NULL;
    }

    return popup_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_menu_destroy(wlmtk_popup_menu_t *popup_menu_ptr)
{
    wlmtk_popup_fini(&popup_menu_ptr->super_popup);
    wlmtk_menu_fini(&popup_menu_ptr->menu);
    free(popup_menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_popup_t *wlmtk_popup_menu_popup(wlmtk_popup_menu_t *popup_menu_ptr)
{
    return &popup_menu_ptr->super_popup;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_popup_menu_menu(wlmtk_popup_menu_t *popup_menu_ptr)
{
    return &popup_menu_ptr->menu;
}

/* == Local (static) methods =============================================== */

/* == End of popup_menu.c ================================================== */
