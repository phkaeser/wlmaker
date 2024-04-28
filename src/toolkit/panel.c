/* ========================================================================= */
/**
 * @file panel.c
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

#include "panel.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_panel_init(wlmtk_panel_t *panel_ptr,
                      wlmtk_env_t *env_ptr)
{
    memset(panel_ptr, 0, sizeof(wlmtk_panel_t));
    if (!wlmtk_container_init(&panel_ptr->super_container, env_ptr)) {
        wlmtk_panel_fini(panel_ptr);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_panel_fini(wlmtk_panel_t *panel_ptr)
{
    panel_ptr = panel_ptr;  // Unused.
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_panel_element(wlmtk_panel_t *panel_ptr)
{
    return &panel_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_panel_set_layer(wlmtk_panel_t *panel_ptr,
                           wlmtk_layer_t *layer_ptr)
{
    // Guard condition: Permit setting layer only if none set. And clearing
    // only if one is set.
    BS_ASSERT((NULL == layer_ptr) != (NULL == panel_ptr->layer_ptr));
    panel_ptr->layer_ptr = layer_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_layer_t *wlmtk_panel_get_layer(wlmtk_panel_t *panel_ptr)
{
    return panel_ptr->layer_ptr;
}

/* == Local (static) methods =============================================== */

const bs_test_case_t          wlmtk_panel_test_cases[] = {
    { 0, NULL, NULL }
};

/* == End of panel.c ======================================================= */
