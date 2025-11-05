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

#include <libbase/libbase.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/edges.h>

#include "test.h"  // IWYU pragma: keep

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_panel_init(
    wlmtk_panel_t *panel_ptr,
    const wlmtk_panel_positioning_t *positioning_ptr)
{
    *panel_ptr = (wlmtk_panel_t){};
    if (!wlmtk_container_init(&panel_ptr->super_container)) {
        wlmtk_panel_fini(panel_ptr);
        return false;
    }
    panel_ptr->positioning = *positioning_ptr;

    if (!wlmtk_container_init(&panel_ptr->popup_container)) {
        wlmtk_panel_fini(panel_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &panel_ptr->super_container,
        &panel_ptr->popup_container.super_element);
    wlmtk_element_set_visible(&panel_ptr->popup_container.super_element, true);

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_panel_fini(wlmtk_panel_t *panel_ptr)
{
    if (panel_ptr->popup_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &panel_ptr->super_container,
            &panel_ptr->popup_container.super_element);
    }
    wlmtk_container_fini(&panel_ptr->popup_container);
    wlmtk_container_fini(&panel_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
wlmtk_panel_vmt_t wlmtk_panel_extend(
    wlmtk_panel_t *panel_ptr,
    const wlmtk_panel_vmt_t *panel_vmt_ptr)
{
    wlmtk_panel_vmt_t orig_panel_vmt = panel_ptr->vmt;

    if (NULL != panel_vmt_ptr->request_size) {
        panel_ptr->vmt.request_size = panel_vmt_ptr->request_size;
    }

    return orig_panel_vmt;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_panel_element(wlmtk_panel_t *panel_ptr)
{
    return &panel_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_panel(wlmtk_panel_t *panel_ptr)
{
    if (NULL == panel_ptr) return NULL;
    return &panel_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_panel_t *wlmtk_panel_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_panel_t, dlnode);
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

/* ------------------------------------------------------------------------- */
void wlmtk_panel_set_layer_output(
    wlmtk_panel_t *panel_ptr,
    wlmtk_layer_output_t *layer_output_ptr)
{
    panel_ptr->layer_output_ptr = layer_output_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_layer_output_t *wlmtk_panel_get_layer_output(wlmtk_panel_t *panel_ptr)
{
    return panel_ptr->layer_output_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_panel_commit(
    wlmtk_panel_t *panel_ptr,
    __UNUSED__ uint32_t serial,
    const wlmtk_panel_positioning_t *positioning_ptr)
{
    // TODO(kaeser@gubbe.ch): Make use of `serial` and only update the
    // element's position once this matches the corresponding call to
    // @ref wlmtk_panel_request_size.

    // Guard clause: No updates, nothing more to do.
    if (0 == memcmp(
            &panel_ptr->positioning,
            positioning_ptr,
            sizeof(wlmtk_panel_positioning_t))) return;

    panel_ptr->positioning = *positioning_ptr;

    if (NULL != panel_ptr->layer_output_ptr) {
        wlmtk_layer_output_reconfigure(panel_ptr->layer_output_ptr);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_panel_compute_dimensions(
    const wlmtk_panel_t *panel_ptr,
    const struct wlr_box *full_area_ptr,
    struct wlr_box *usable_area_ptr)
{
    // Copied for readability.
    uint32_t anchor = panel_ptr->positioning.anchor;
    int margin_left = panel_ptr->positioning.margin_left;
    int margin_right = panel_ptr->positioning.margin_right;
    int margin_top = panel_ptr->positioning.margin_top;
    int margin_bottom = panel_ptr->positioning.margin_bottom;

    // Negative 'exclusive_zone' values mean to ignore other panels.
    struct wlr_box max_dims = *usable_area_ptr;
    if (0 > panel_ptr->positioning.exclusive_zone) {
        max_dims = *full_area_ptr;
    }

    struct wlr_box dims = {
        .width = panel_ptr->positioning.desired_width,
        .height = panel_ptr->positioning.desired_height
    };

    // Set horizontal position and width.
    if (0 == dims.width) {
        // Width not given. Protocol requires the anchor to be set on left &
        // right edges, and translates to full width (minus margins).
        dims.x = max_dims.x + margin_left;
        dims.width = max_dims.width - margin_left - margin_right;
    } else if (anchor & WLR_EDGE_LEFT && !(anchor & WLR_EDGE_RIGHT)) {
        // Width given, anchored only on the left: At margin.
        dims.x = max_dims.x + margin_left;
    } else if (anchor & WLR_EDGE_RIGHT && !(anchor & WLR_EDGE_LEFT)) {
        // Width given, anchored only on the right: At margin minus width.
        dims.x = max_dims.x + max_dims.width - margin_right - dims.width;
    } else {
        // There was a width, and no one-sided anchoring: Center it.
        dims.x = max_dims.x + max_dims.width / 2 - dims.width / 2;
    }

    // Set vertical position and height.
    if (0 == dims.height) {
        // Height not given. Protocol requires the anchor to be set on top &
        // bottom edges, and translates to full height (minus margins).
        dims.y = max_dims.y + margin_top;
        dims.height = max_dims.height - margin_top - margin_bottom;
    } else if (anchor & WLR_EDGE_TOP && !(anchor & WLR_EDGE_BOTTOM)) {
        // Height given, anchored only on the top: At margin.
        dims.y = max_dims.y + margin_top;
    } else if (anchor & WLR_EDGE_BOTTOM && !(anchor & WLR_EDGE_TOP)) {
        // Height given, anchored only on the bottom: At margin minus height.
        dims.y = max_dims.y + max_dims.height - margin_bottom - dims.height;
    } else {
        // There was a height, and no vertical anchoring: Center it.
        dims.y = max_dims.y + max_dims.height / 2 - dims.height / 2;
    }

    // Update the usable area, if there is an exclusive zone.
    int exclusive_zone = panel_ptr->positioning.exclusive_zone;
    if (0 < exclusive_zone) {
        if (anchor == WLR_EDGE_LEFT ||
            anchor == (WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) {
            usable_area_ptr->x += exclusive_zone + margin_left;
            usable_area_ptr->width -= exclusive_zone + margin_left;
        }
        if (anchor == WLR_EDGE_RIGHT ||
            anchor == (WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) {
            usable_area_ptr->width -= exclusive_zone + margin_right;
        }
        if (anchor == WLR_EDGE_TOP ||
            anchor == (WLR_EDGE_TOP | WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
            usable_area_ptr->y += exclusive_zone + margin_top;
            usable_area_ptr->height -= exclusive_zone + margin_top;
        }
        if (anchor == WLR_EDGE_BOTTOM ||
            anchor == (WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
            usable_area_ptr->height -= exclusive_zone + margin_bottom;
        }
    }

    return dims;
}

/* == Local (static) methods =============================================== */

static void _wlmtk_fake_panel_element_destroy(
    wlmtk_element_t *element_ptr);
static uint32_t _wlmtk_fake_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

/** Virtual methods of the fake panel's element superclass. */
static const wlmtk_element_vmt_t _wlmtk_fake_panel_element_vmt = {
    .destroy = _wlmtk_fake_panel_element_destroy
};
/** Virtual methods of the fake panel. */
static const wlmtk_panel_vmt_t _wlmtk_fake_panel_vmt = {
    .request_size = _wlmtk_fake_panel_request_size
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_panel_t *wlmtk_fake_panel_create(
    const wlmtk_panel_positioning_t *positioning_ptr)
{
    wlmtk_fake_panel_t *fake_panel_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_panel_t));
    if (NULL == fake_panel_ptr) return NULL;

    if (!wlmtk_panel_init(&fake_panel_ptr->panel, positioning_ptr)) {
        wlmtk_fake_panel_destroy(fake_panel_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &fake_panel_ptr->panel.super_container.super_element,
        &_wlmtk_fake_panel_element_vmt);
    wlmtk_panel_extend(&fake_panel_ptr->panel, &_wlmtk_fake_panel_vmt);

    return fake_panel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_panel_destroy(wlmtk_fake_panel_t *fake_panel_ptr)
{
    wlmtk_panel_fini(&fake_panel_ptr->panel);
    free(fake_panel_ptr);
}

/** Implements @ref wlmtk_element_vmt_t::destroy for the fake panel. */
void _wlmtk_fake_panel_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmtk_fake_panel_t *fake_panel_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_fake_panel_t, panel.super_container.super_element);
    wlmtk_fake_panel_destroy(fake_panel_ptr);
}

/** Fake implementation of @ref wlmtk_panel_vmt_t::request_size. */
uint32_t _wlmtk_fake_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height)
{
    wlmtk_fake_panel_t *fake_panel_ptr = BS_CONTAINER_OF(
        panel_ptr, wlmtk_fake_panel_t, panel);
    fake_panel_ptr->requested_width = width;
    fake_panel_ptr->requested_height = height;
    return fake_panel_ptr->serial;
}


/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_compute_dimensions(bs_test_t *test_ptr);
static void test_compute_dimensions_exclusive(bs_test_t *test_ptr);

const bs_test_case_t          wlmtk_panel_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "compute_dimensions", test_compute_dimensions },
    { 1, "compute_dimensions_exclusive", test_compute_dimensions_exclusive },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup, teardown and some accessors. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_panel_t p;
    wlmtk_panel_positioning_t pos = {};

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_panel_init(&p, &pos));

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_panel(&p);
    BS_TEST_VERIFY_EQ(test_ptr, &p.dlnode, dlnode_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, &p, wlmtk_panel_from_dlnode(dlnode_ptr));

    wlmtk_panel_fini(&p);
}

/* ------------------------------------------------------------------------- */
/** Verifies wlmtk_panel_compute_dimensions. */
void test_compute_dimensions(bs_test_t *test_ptr)
{
    wlmtk_panel_positioning_t pos = {
        .desired_width = 100,
        .desired_height = 50
    };
    wlmtk_fake_panel_t *fake_panel_ptr = BS_ASSERT_NOTNULL(
        wlmtk_fake_panel_create(&pos));
    wlmtk_panel_t *p_ptr = &fake_panel_ptr->panel;

    struct wlr_box extents = { .x = 0, .y = 0, .width = 200, .height = 100 };
    struct wlr_box usable = extents;
    struct wlr_box dims;

    p_ptr->positioning.margin_left = 10;
    p_ptr->positioning.margin_right = 20;
    p_ptr->positioning.margin_top = 8;
    p_ptr->positioning.margin_bottom = 4;

    // Not anchored: Keep proposed dimensions.
    p_ptr->positioning.anchor = 0;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 50, 25, 100, 50, dims);

    // Anchored left or right: Respect margin.
    p_ptr->positioning.anchor = WLR_EDGE_LEFT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 10, 25, 100, 50, dims);

    p_ptr->positioning.anchor = WLR_EDGE_RIGHT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 80, 25, 100, 50, dims);

    // Anchored left & right: Centered, and keep proposed dimensions.
    p_ptr->positioning.anchor = WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 50, 25, 100, 50, dims);

    // Anchored top or bottom: Respect margin.
    p_ptr->positioning.anchor = WLR_EDGE_TOP;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 50, 8, 100, 50, dims);

    p_ptr->positioning.anchor = WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 50, 46, 100, 50, dims);

    // Anchored top and bottom: Centered.
    p_ptr->positioning.anchor = WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 50, 25, 100, 50, dims);

    // Anchored all around, and no size proposed: Use full extents,
    // while respecting margins.
    p_ptr->positioning.anchor =
        WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
    p_ptr->positioning.desired_height = 0;
    p_ptr->positioning.desired_width = 0;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 10, 8, 170, 88, dims);

    wlmtk_fake_panel_destroy(fake_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies dimension computation with an exclusive_zone. */
void test_compute_dimensions_exclusive(bs_test_t *test_ptr)
{
    wlmtk_panel_positioning_t pos = {
        .exclusive_zone = 16,
        .anchor = (WLR_EDGE_LEFT | WLR_EDGE_RIGHT |
                   WLR_EDGE_TOP | WLR_EDGE_BOTTOM),
        .margin_left = 40,
        .margin_right = 30,
        .margin_top = 20,
        .margin_bottom = 10
    };

    wlmtk_fake_panel_t *fake_panel_ptr = BS_ASSERT_NOTNULL(
        wlmtk_fake_panel_create(&pos));
    wlmtk_panel_t *p_ptr = &fake_panel_ptr->panel;

    struct wlr_box extents = { .x = 0, .y = 0, .width = 200, .height = 100 };
    struct wlr_box usable = { .x = 1, .y = 2, .width = 195, .height = 90 };
    struct wlr_box dims;

    // Use full extents on negative exclusive_zone value.
    p_ptr->positioning.exclusive_zone = -1;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 40, 20, 130, 70, dims);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 1, 2, 195, 90, usable);

    // Respect the usable area, for non-negative exclusive zone.
    p_ptr->positioning.exclusive_zone = 0;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 41, 22, 125, 60, dims);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 1, 2, 195, 90, usable);

    // Respect the usable area, for non-negative exclusive zone. Do not
    // update the usable zone, since anchored not appropriately.
    p_ptr->positioning.exclusive_zone = 7;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 41, 22, 125, 60, dims);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 1, 2, 195, 90, usable);

    // Respect usable zone, and update, since anchored left and full-height.
    p_ptr->positioning.desired_width = 20;
    p_ptr->positioning.exclusive_zone = 7;
    p_ptr->positioning.anchor = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 41, 22, 20, 60, dims);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 48, 2, 148, 90, usable);

    // Check for usable zone at the bottom.
    usable.x = 1; usable.y = 2; usable.width = 195; usable.height = 90;
    p_ptr->positioning.desired_width = 100;
    p_ptr->positioning.desired_height = 20;
    p_ptr->positioning.exclusive_zone = 7;
    p_ptr->positioning.anchor = WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 48, 62, 100, 20, dims);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 1, 2, 195, 73, usable);

    wlmtk_fake_panel_destroy(fake_panel_ptr);
}

/* == End of panel.c ======================================================= */
