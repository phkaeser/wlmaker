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

// Background: A buffer element. (he tile texture and such).
// has: fill, and bezel width & details.
//
// This needs the desired tile size. This *may* be change-able, part of style.
// then bs_gfxbuf_create_wlr_buffer(...)
//
// So, we need:
// - size
// - fill style
// - bezel width

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_tile_init(
    wlmtk_tile_t *tile_ptr,
    const wlmtk_tile_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    memset(tile_ptr, 0, sizeof(wlmtk_tile_t));
    memcpy(&tile_ptr->style, style_ptr, sizeof(wlmtk_tile_style_t));

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
wlmtk_element_t *wlmtk_tile_element(wlmtk_tile_t *tile_ptr)
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

static void test_init_fini(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_tile_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
static void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_tile_t tile;
    wlmtk_tile_style_t style = { .size = 64 };

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_tile_init(&tile, &style, NULL));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &tile.super_container.super_element,
        wlmtk_tile_element(&tile));
    wlmtk_tile_fini(&tile);
}

/* == End of tile.c ======================================================== */
