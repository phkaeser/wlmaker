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

#include <wlr/util/edges.h>

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
void wlmtk_panel_commit(
    __UNUSED__ wlmtk_panel_t *panel_ptr,
    __UNUSED__ uint32_t serial)
{
    // FIXME: Implement.
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_panel_compute_dimensions(
    const wlmtk_panel_t *panel_ptr,
    const struct wlr_box *full_area_ptr,
    struct wlr_box *usable_area_ptr)
{
    // Copied for readability.
    struct wlr_box max_dims = *full_area_ptr;
    uint32_t anchor = panel_ptr->anchor;
    int margin_left = panel_ptr->margin_left;
    int margin_right = panel_ptr->margin_right;
    int margin_top = panel_ptr->margin_top;
    int margin_bottom = panel_ptr->margin_bottom;

    struct wlr_box dims = {
        .width = panel_ptr->width,
        .height = panel_ptr->height
    };

    // Set horizontal position and width.
    if (0 == dims.width) {
        // Width not given. Protocol requires the anchor to be set on left &
        // right edges, and translates to full width (minus margins).
        BS_ASSERT(anchor & WLR_EDGE_LEFT && anchor & WLR_EDGE_RIGHT);
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
        BS_ASSERT(anchor & WLR_EDGE_TOP && anchor & WLR_EDGE_BOTTOM);
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

    *usable_area_ptr = *full_area_ptr;
    return dims;
}

/* == Local (static) methods =============================================== */

static uint32_t _wlmtk_fake_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

/** Virtual methods of the fake panel. */
static const wlmtk_panel_vmt_t _wlmtk_fake_panel_vmt = {
    .request_size = _wlmtk_fake_panel_request_size
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_panel_t *wlmtk_fake_panel_create(
    uint32_t anchor, int width, int height)
{
    wlmtk_fake_panel_t *fake_panel_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_panel_t));
    if (NULL == fake_panel_ptr) return NULL;

    if (!wlmtk_panel_init(&fake_panel_ptr->panel, NULL)) {
        wlmtk_fake_panel_destroy(fake_panel_ptr);
        return NULL;
    }
    wlmtk_panel_extend(&fake_panel_ptr->panel, &_wlmtk_fake_panel_vmt);

    fake_panel_ptr->panel.anchor = anchor;
    fake_panel_ptr->panel.width = width;
    fake_panel_ptr->panel.height = height;

    return fake_panel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_panel_destroy(wlmtk_fake_panel_t *fake_panel_ptr)
{
    wlmtk_panel_fini(&fake_panel_ptr->panel);
    free(fake_panel_ptr);
}

/* ------------------------------------------------------------------------- */
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

const bs_test_case_t          wlmtk_panel_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "compute_dimensions", test_compute_dimensions },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup, teardown and some accessors. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_panel_t p;

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_panel_init(&p, NULL));

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_panel(&p);
    BS_TEST_VERIFY_EQ(test_ptr, &p.dlnode, dlnode_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, &p, wlmtk_panel_from_dlnode(dlnode_ptr));

    wlmtk_panel_fini(&p);
}

/* ------------------------------------------------------------------------- */
/** Verifies wlmtk_panel_compute_dimensions. */
void test_compute_dimensions(bs_test_t *test_ptr)
{
    wlmtk_fake_panel_t *fake_panel_ptr = wlmtk_fake_panel_create(0, 100, 50);
    wlmtk_panel_t *p_ptr = &fake_panel_ptr->panel;

    struct wlr_box extents = { .x = 0, .y = 0, .width = 200, .height = 100};
    struct wlr_box usable = {};
    struct wlr_box dims;

    p_ptr->margin_left = 10;
    p_ptr->margin_right = 20;
    p_ptr->margin_top = 8;
    p_ptr->margin_bottom = 4;

    p_ptr->anchor = 0;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 25, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_LEFT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 10, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 25, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_RIGHT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 80, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 25, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_LEFT | WLR_EDGE_RIGHT;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 25, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_TOP;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 8, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 46, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    p_ptr->anchor = WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
    dims = wlmtk_panel_compute_dimensions(p_ptr, &extents, &usable);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.x);
    BS_TEST_VERIFY_EQ(test_ptr, 25, dims.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, dims.width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, dims.height);

    wlmtk_fake_panel_destroy(fake_panel_ptr);
}

/* == End of panel.c ======================================================= */
