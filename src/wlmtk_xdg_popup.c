/* ========================================================================= */
/**
 * @file wlmtk_xdg_popup.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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

#include "wlmtk_xdg_popup.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_xdg_popup_t *wlmtk_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_xdg_popup_t *xdg_popup_ptr = logged_calloc(
        1, sizeof(wlmtk_xdg_popup_t));
    if (NULL == xdg_popup_ptr) return NULL;
    xdg_popup_ptr->wlr_xdg_popup_ptr = wlr_xdg_popup_ptr;

    if (!wlmtk_surface_init(
            &xdg_popup_ptr->surface,
            wlr_xdg_popup_ptr->base->surface,
            env_ptr)) {
        wlmtk_xdg_popup_destroy(xdg_popup_ptr);
        return NULL;
    }

    if (!wlmtk_content_init(
            &xdg_popup_ptr->super_content,
            &xdg_popup_ptr->surface,
            env_ptr)) {
        wlmtk_xdg_popup_destroy(xdg_popup_ptr);
        return NULL;
    }

    return xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_xdg_popup_destroy(wlmtk_xdg_popup_t *xdg_popup_ptr)
{
    wlmtk_content_fini(&xdg_popup_ptr->super_content);
    wlmtk_surface_fini(&xdg_popup_ptr->surface);
    free(xdg_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of wlmtk_xdg_popup.c ============================================= */
