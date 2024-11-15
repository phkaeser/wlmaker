/* ========================================================================= */
/**
 * @file root_menu.c
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

#include "root_menu.h"

#include <libbase/libbase.h>
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of the root menu. */
struct _wlmaker_root_menu_t {
    /** Window. */
    wlmtk_window_t            *window_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_menu_t *wlmaker_root_menu_create(void)
{
    wlmaker_root_menu_t *root_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_t));
    if (NULL == root_menu_ptr) return NULL;

    return root_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_menu_destroy(wlmaker_root_menu_t *root_menu_ptr)
{
    free(root_menu_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of root_menu.c =================================================== */
