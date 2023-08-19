/* ========================================================================= */
/**
 * @file element.c
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

#include "toolkit.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_element_init(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_impl_t *element_impl_ptr)
{
    BS_ASSERT(NULL != element_ptr);
    BS_ASSERT(NULL != element_impl_ptr);
    memset(element_ptr, 0, sizeof(wlmtk_element_t));

    element_ptr->impl_ptr = element_impl_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr)
{
    // Verify we're no longer mapped, nor part of a container.
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
    BS_ASSERT(NULL == element_ptr->container_ptr);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_element(
    wlmtk_element_t *element_ptr)
{
    return &element_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_element_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_element_t, dlnode);
}

/* ------------------------------------------------------------------------- */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr)
{
    element_ptr->container_ptr = parent_container_ptr;
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

const bs_test_case_t wlmtk_element_test_cases[] = {
    { 0, NULL, NULL }
};

/* == End of toolkit.c ===================================================== */
