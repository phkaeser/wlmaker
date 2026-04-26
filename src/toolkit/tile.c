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

#include <cairo.h>
#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <string.h>

#include "gfxbuf.h"  // IWYU pragma: keep
#include "primitives.h"
#include "style.h"

/* == Declarations ========================================================= */

static struct wlr_buffer *_wlmtk_tile_create_buffer(
    const struct wlmtk_tile_style *style_ptr);
static void _wlmtk_tile_align_content(wlmtk_tile_t *tile_ptr);

/* == Data ================================================================= */

const bspl_desc_t wlmtk_tile_style_desc[] = {
    BSPL_DESC_UINT64(
        "Size", true, struct wlmtk_tile_style, size, size, 64),
    BSPL_DESC_UINT64(
        "ContentSize", true, struct wlmtk_tile_style,
        content_size, content_size, 48),
    BSPL_DESC_UINT64(
        "BezelWidth", true, struct wlmtk_tile_style,
        bezel_width, bezel_width, 2),
    BSPL_DESC_CUSTOM(
        "Fill", true, struct wlmtk_tile_style, fill, fill,
        wlmtk_style_decode_fill, NULL, NULL, NULL),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_tile_init(
    wlmtk_tile_t *tile_ptr,
    const struct wlmtk_tile_style *style_ptr)
{
    *tile_ptr = (wlmtk_tile_t){ .style = *style_ptr };

    if (!wlmtk_container_init(&tile_ptr->super_container)) {
        wlmtk_tile_fini(tile_ptr);
        return false;
    }

    if (!wlmtk_buffer_init(&tile_ptr->buffer)) {
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
wlmtk_tile_vmt_t wlmtk_tile_extend(
    wlmtk_tile_t *tile_ptr,
    const wlmtk_tile_vmt_t *tile_vmt_ptr)
{
    wlmtk_tile_vmt_t orig_vmt = tile_ptr->vmt;

    if (NULL != tile_vmt_ptr->set_content_size) {
        tile_ptr->vmt.set_content_size = tile_vmt_ptr->set_content_size;
    }
    return orig_vmt;
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
bool wlmtk_tile_set_style(wlmtk_tile_t *tile_ptr,
                          const struct wlmtk_tile_style *style_ptr)
{
    // Update buffer.
    struct wlr_buffer *wlr_buffer_ptr = _wlmtk_tile_create_buffer(style_ptr);
    if (NULL == wlr_buffer_ptr) return false;

    tile_ptr->style = *style_ptr;
    bool rv = wlmtk_tile_set_background_buffer(tile_ptr, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);
    if (!rv) return false;

    if (NULL != tile_ptr->vmt.set_content_size) {
        tile_ptr->vmt.set_content_size(tile_ptr, style_ptr->content_size);
    }

    _wlmtk_tile_align_content(tile_ptr);
    wlmtk_element_invalidate_parent_layout(wlmtk_tile_element(tile_ptr));
    return true;
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
    if (element_ptr != tile_ptr->content_element_ptr) {

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
        }
    }

    _wlmtk_tile_align_content(tile_ptr);
    wlmtk_element_invalidate_parent_layout(wlmtk_tile_element(tile_ptr));
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

/* ------------------------------------------------------------------------- */
wlmtk_tile_t *wlmtk_tile_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_tile_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_tile(wlmtk_tile_t *tile_ptr)
{
    return &tile_ptr->dlnode;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Crates a wlr_buffer with background, as described in `style_ptr`. */
struct wlr_buffer *_wlmtk_tile_create_buffer(
    const struct wlmtk_tile_style *style_ptr)
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

/* ------------------------------------------------------------------------- */
/** (re)centers the content element. */
void _wlmtk_tile_align_content(wlmtk_tile_t *tile_ptr)
{
    wlmtk_element_t *element_ptr = tile_ptr->content_element_ptr;
    if (NULL == element_ptr) return;

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

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);

/** Test cases */
static const bs_test_case_t _wlmtk_tile_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmtk_tile_test_set = BS_TEST_SET(
    true, "tile", _wlmtk_tile_test_cases);

/* ------------------------------------------------------------------------- */
/** Exercises setup and teardown. */
static void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_tile_t tile;
    struct wlmtk_tile_style style = { .size = 64 };

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_tile_init(&tile, &style));
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
