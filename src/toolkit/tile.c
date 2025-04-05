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

#include <toolkit/gfxbuf.h>
#include <toolkit/primitives.h>
#include <toolkit/tile.h>

/* == Declarations ========================================================= */

static void _wlmtk_tile_update_layout(wlmtk_container_t *container_ptr);

static struct wlr_buffer *_wlmtk_tile_create_buffer(
    const wlmtk_tile_style_t *style_ptr);

/* == Data ================================================================= */

/** Virtual methods implemented by @ref wlmtk_tile_t. */
static const wlmtk_container_vmt_t _wlmtk_tile_container_vmt = {
    .update_layout = _wlmtk_tile_update_layout
};

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

    if (!wlmtk_buffer_init(&tile_ptr->buffer, NULL)) {
        wlmtk_tile_fini(tile_ptr);
        return false;
    }
    wlmtk_element_set_visible(wlmtk_buffer_element(&tile_ptr->buffer), true);
    wlmtk_container_add_element(
        &tile_ptr->super_container,
        wlmtk_buffer_element(&tile_ptr->buffer));

    struct wlr_buffer *wlr_buffer_ptr = _wlmtk_tile_create_buffer(
        &tile_ptr->style);
    if (NULL == wlr_buffer_ptr) {
        wlmtk_tile_fini(tile_ptr);
        return false;
    }
    wlmtk_tile_set_background_buffer(tile_ptr, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_tile_fini(wlmtk_tile_t *tile_ptr)
{
    if (NULL != tile_ptr->background_wlr_buffer_ptr) {
        wlr_buffer_unlock(tile_ptr->background_wlr_buffer_ptr);
        tile_ptr->background_wlr_buffer_ptr = NULL;
    }

    if (wlmtk_buffer_element(&tile_ptr->buffer)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &tile_ptr->super_container,
            wlmtk_buffer_element(&tile_ptr->buffer));
        wlmtk_buffer_fini(&tile_ptr->buffer);
    }

    wlmtk_container_fini(&tile_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_tile_set_background_buffer(
    wlmtk_tile_t *tile_ptr,
    struct wlr_buffer *wlr_buffer_ptr)
{
    if (tile_ptr->style.size != (uint64_t)wlr_buffer_ptr->width ||
        tile_ptr->style.size != (uint64_t)wlr_buffer_ptr->height) return false;

    if (NULL != tile_ptr->background_wlr_buffer_ptr) {
        wlr_buffer_unlock(tile_ptr->background_wlr_buffer_ptr);
    }
    tile_ptr->background_wlr_buffer_ptr = wlr_buffer_lock(wlr_buffer_ptr);
    wlmtk_buffer_set(&tile_ptr->buffer, tile_ptr->background_wlr_buffer_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_tile_set_content(
    wlmtk_tile_t *tile_ptr,
    wlmtk_element_t *element_ptr)
{
    if (element_ptr == tile_ptr->content_element_ptr) return;

    if (NULL != tile_ptr->content_element_ptr) {
        wlmtk_container_remove_element(
            &tile_ptr->super_container,
            tile_ptr->content_element_ptr);
        tile_ptr->content_element_ptr = NULL;
    }

    if (NULL != element_ptr) {
        wlmtk_container_add_element_atop(
            &tile_ptr->super_container,
            wlmtk_buffer_element(&tile_ptr->buffer),
            element_ptr);
        tile_ptr->content_element_ptr = element_ptr;

        struct wlr_box box = wlmtk_element_get_dimensions_box(element_ptr);
        if ((unsigned)box.width > tile_ptr->style.size ||
            (unsigned)box.height > tile_ptr->style.size) {
            bs_log(BS_WARNING, "Content size %d x %d > tile size %"PRIu64,
                   box.width, box.height, tile_ptr->style.size);
        }
        wlmtk_element_set_position(
            element_ptr,
            ((int)tile_ptr->style.size - box.width) / 2,
            ((int)tile_ptr->style.size - box.height) / 2);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_tile_set_overlay(
    wlmtk_tile_t *tile_ptr,
    wlmtk_element_t *element_ptr)
{
    if (element_ptr == tile_ptr->overlay_element_ptr) return;

    if (NULL != tile_ptr->overlay_element_ptr) {
        wlmtk_container_remove_element(
            &tile_ptr->super_container,
            tile_ptr->overlay_element_ptr);
        tile_ptr->overlay_element_ptr = NULL;
    }

    if (NULL != element_ptr) {
        wlmtk_container_add_element(&tile_ptr->super_container, element_ptr);
        tile_ptr->overlay_element_ptr = element_ptr;

        struct wlr_box box = wlmtk_element_get_dimensions_box(element_ptr);
        if ((unsigned)box.width > tile_ptr->style.size ||
            (unsigned)box.height > tile_ptr->style.size) {
            bs_log(BS_WARNING, "Overlay size %d x %d > tile size %"PRIu64,
                   box.width, box.height, tile_ptr->style.size);
        }
        wlmtk_element_set_position(
            element_ptr,
            ((int)tile_ptr->style.size - box.width) / 2,
            ((int)tile_ptr->style.size - box.height) / 2);
    }
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

/* ------------------------------------------------------------------------- */
/** Crates a wlr_buffer with background, as described in `style_ptr`. */
struct wlr_buffer *_wlmtk_tile_create_buffer(
    const wlmtk_tile_style_t *style_ptr)
{
    struct wlr_buffer* wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        style_ptr->size, style_ptr->size);
    if (NULL == wlr_buffer_ptr) return NULL;

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }

    wlmaker_primitives_cairo_fill(cairo_ptr, &style_ptr->fill);
    wlmaker_primitives_draw_bezel(cairo_ptr, style_ptr->bezel_width, true);

    cairo_destroy(cairo_ptr);
    return wlr_buffer_ptr;
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

    // Adds content and verifies it's centered.
    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    fe_ptr->dimensions.width = 48;
    fe_ptr->dimensions.height = 36;
    wlmtk_tile_set_content(&tile, &fe_ptr->element);
    int x, y;
    wlmtk_element_get_position(&fe_ptr->element, &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 8, x);
    BS_TEST_VERIFY_EQ(test_ptr, 14, y);

    wlmtk_tile_fini(&tile);
}

/* == End of tile.c ======================================================== */
