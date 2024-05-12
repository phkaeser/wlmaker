/* ========================================================================= */
/**
 * @file pubase.c
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

#include "pubase.h"

/* == Declarations ========================================================= */

/* ------------------------------------------------------------------------- */
bool wlmtk_pubase_init(wlmtk_pubase_t *pubase_ptr, wlmtk_env_t *env_ptr)
{
    memset(pubase_ptr, 0, sizeof(wlmtk_pubase_t));
    if (!wlmtk_container_init(&pubase_ptr->super_container, env_ptr)) {
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_pubase_fini(wlmtk_pubase_t *pubase_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = pubase_ptr->popups.head_ptr)) {
        wlmtk_popup_t *popup_ptr = wlmtk_popup_from_dlnode(dlnode_ptr);
        wlmtk_pubase_remove_popup(pubase_ptr, popup_ptr);
    }

    wlmtk_container_fini(&pubase_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
void wlmtk_pubase_add_popup(wlmtk_pubase_t *pubase_ptr,
                            wlmtk_popup_t *popup_ptr)
{
    wlmtk_container_add_element(
        &pubase_ptr->super_container,
        wlmtk_popup_element(popup_ptr));
    wlmtk_popup_set_pubase(popup_ptr, pubase_ptr);

    bs_dllist_push_back(
        &pubase_ptr->popups,
        wlmtk_dlnode_from_popup(popup_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_pubase_remove_popup(wlmtk_pubase_t *pubase_ptr,
                               wlmtk_popup_t *popup_ptr)
{
    bs_dllist_remove(
        &pubase_ptr->popups,
        wlmtk_dlnode_from_popup(popup_ptr));
    wlmtk_container_remove_element(
        &pubase_ptr->super_container,
        wlmtk_popup_element(popup_ptr));
    wlmtk_popup_set_pubase(popup_ptr, NULL);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_pubase_element(wlmtk_pubase_t *pubase_ptr)
{
    return &pubase_ptr->super_container.super_element;
}

/* == Exported methods ===================================================== */

/* == Local (static) methods =============================================== */

/* == End of pubase.c ====================================================== */
