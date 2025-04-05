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

#include <toolkit/popup.h>

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_popup_init(
    wlmtk_popup_t *popup_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_element_t *element_ptr)
{
    memset(popup_ptr, 0, sizeof(wlmtk_popup_t));
    if (!wlmtk_container_init(&popup_ptr->super_container, env_ptr)) {
        return false;
    }

    if (!wlmtk_container_init(&popup_ptr->popup_container, env_ptr)) {
        wlmtk_popup_fini(popup_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &popup_ptr->super_container,
        &popup_ptr->popup_container.super_element);
    wlmtk_element_set_visible(&popup_ptr->popup_container.super_element, true);

    if (NULL != element_ptr) {
        popup_ptr->element_ptr = element_ptr;
        wlmtk_container_add_element(
            &popup_ptr->super_container,
            popup_ptr->element_ptr);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_fini(wlmtk_popup_t *popup_ptr)
{
    if (NULL != wlmtk_popup_element(popup_ptr)->parent_container_ptr) {
        wlmtk_container_remove_element(
            wlmtk_popup_element(popup_ptr)->parent_container_ptr,
            wlmtk_popup_element(popup_ptr));
    }

    if (NULL != popup_ptr->element_ptr) {
        wlmtk_container_remove_element(
            &popup_ptr->super_container,
            popup_ptr->element_ptr);
        popup_ptr->element_ptr = NULL;
    }

    if (popup_ptr->popup_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &popup_ptr->super_container,
            &popup_ptr->popup_container.super_element);
    }
    wlmtk_container_fini(&popup_ptr->popup_container);

    wlmtk_container_fini(&popup_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_add_popup(wlmtk_popup_t *popup_ptr,
                           wlmtk_popup_t *further_popup_ptr)
{
    BS_ASSERT(!wlmtk_popup_element(further_popup_ptr)->parent_container_ptr);
    wlmtk_container_add_element(
        &popup_ptr->popup_container,
        wlmtk_popup_element(further_popup_ptr));
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_popup_element(wlmtk_popup_t *popup_ptr)
{
    return &popup_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* == End of popup.c ======================================================= */
