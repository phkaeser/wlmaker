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

#include "box.h"
#include "style.h"

/* == Declarations ========================================================= */

/** State of the menu. */
struct _wlmtk_menu_t {
    /** Derived from a box, holding menu items. */
    wlmtk_box_t               super_box;
};

/* == Data ================================================================= */

/** Style. TODO(kaeser@gubbe.ch): Make a parameter. */
static const wlmtk_margin_style_t style = {};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_menu_create(wlmtk_env_t *env_ptr)
{
    wlmtk_menu_t *menu_ptr = logged_calloc(1, sizeof(wlmtk_menu_t));
    if (NULL == menu_ptr) return NULL;

    if (!wlmtk_box_init(
            &menu_ptr->super_box,
            env_ptr,
            WLMTK_BOX_VERTICAL,
            &style)) {
        wlmtk_menu_destroy(menu_ptr);
        return NULL;
    }

    return menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_destroy(wlmtk_menu_t *menu_ptr)
{
    wlmtk_box_fini(&menu_ptr->super_box);
    free(menu_ptr);
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

/* == End of menu.c ======================================================== */
