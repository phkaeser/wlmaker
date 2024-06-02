/* ========================================================================= */
/**
 * @file tile.c
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

#include "tile.h"

/* == Declarations ========================================================= */

static void _wlmtk_tile_update_layout(wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Virtual methods implemented by @ref wlmtk_tile_t. */
static const wlmtk_container_vmt_t _wlmtk_tile_container_vmt = {
    .update_layout = _wlmtk_tile_update_layout
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_tile_init(
    wlmtk_tile_t *tile_ptr,
    wlmtk_env_t *env_ptr)
{
    memset(tile_ptr, 0, sizeof(wlmtk_tile_t));
    if (!wlmtk_container_init(&tile_ptr->super_container, env_ptr)) {
        wlmtk_tile_fini(tile_ptr);
        return false;
    }
    tile_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &tile_ptr->super_container, &_wlmtk_tile_container_vmt);

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_tile_fini(wlmtk_tile_t *tile_ptr)
{
    wlmtk_container_fini(&tile_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_tile_elmeent(wlmtk_tile_t *tile_ptr)
{
    return &tile_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles requests to update layout. Called when elements are added. */
void _wlmtk_tile_update_layout(__UNUSED__ wlmtk_container_t *container_ptr)
{
}

/* == Unit tests =========================================================== */

const bs_test_case_t wlmtk_tile_test_cases[] = {
    { 0, NULL, NULL }
};

/* == End of tile.c ======================================================== */
