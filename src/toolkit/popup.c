/* ========================================================================= */
/**
 * @file popup.c
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

#include "popup.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_popup_init(
    wlmtk_popup_t *popup_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_surface_t *surface_ptr)
{
    memset(popup_ptr, 0, sizeof(wlmtk_popup_t));
    if (!wlmtk_container_init(&popup_ptr->super_container, env_ptr)) {
        return false;
    }

    if (!wlmtk_pubase_init(&popup_ptr->pubase, env_ptr)) {
        wlmtk_popup_fini(popup_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &popup_ptr->super_container,
        wlmtk_pubase_element(&popup_ptr->pubase));
    wlmtk_element_set_visible(wlmtk_pubase_element(&popup_ptr->pubase), true);

    wlmtk_container_add_element(
        &popup_ptr->super_container,
        wlmtk_surface_element(surface_ptr));
    popup_ptr->surface_ptr = surface_ptr;
    wlmtk_element_set_visible(wlmtk_surface_element(surface_ptr), true);


    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_fini(wlmtk_popup_t *popup_ptr)
{
    if (NULL != popup_ptr->parent_pubase_ptr) {
        wlmtk_pubase_remove_popup(
            popup_ptr->parent_pubase_ptr,
            popup_ptr);
    }

    if (NULL != popup_ptr->surface_ptr) {
        wlmtk_container_remove_element(
            &popup_ptr->super_container,
            wlmtk_surface_element(popup_ptr->surface_ptr));
        popup_ptr->surface_ptr = NULL;
    }

    if (wlmtk_pubase_element(&popup_ptr->pubase)->parent_container_ptr) {
    wlmtk_container_remove_element(
        &popup_ptr->super_container,
        wlmtk_pubase_element(&popup_ptr->pubase));
    }
    wlmtk_pubase_fini(&popup_ptr->pubase);

    wlmtk_container_fini(&popup_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_set_pubase(wlmtk_popup_t *popup_ptr,
                            wlmtk_pubase_t *pubase_ptr)
{
    BS_ASSERT((NULL == popup_ptr->parent_pubase_ptr) ||
              (NULL == pubase_ptr));
    popup_ptr->parent_pubase_ptr = pubase_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_popup_element(wlmtk_popup_t *popup_ptr)
{
    return &popup_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* == End of popup.c ======================================================= */
