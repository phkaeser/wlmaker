/* ========================================================================= */
/**
 * @file layer.c
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

#include "layer.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>  // IWYU pragma: keep
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "container.h"
#include "output_tracker.h"
#include "panel.h"
#include "test.h"  // IWYU pragma: keep
#include "tile.h"
#include "workspace.h"

/* == Declarations ========================================================= */

/** State of a layer, covering multiple outputs. */
struct _wlmtk_layer_t {
    /** Super class of the layer. */
    wlmtk_container_t         super_container;
    /** Workspace that the layer belongs to. */
    wlmtk_workspace_t         *workspace_ptr;
    /** Tracks the outputs. */
    wlmtk_output_tracker_t    *output_tracker_ptr;
};

/** State of the layer on the given output. */
struct _wlmtk_layer_output_t {
    /** Extents and position of the output in the layout. */
    struct wlr_box            extents;
    /** Panels. Holds nodes at @ref wlmtk_panel_t::dlnode. */
    bs_dllist_t               panels;
};

static void _wlmtk_layer_output_remove_dlnode_panel(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_layer_output_add_panel(
    wlmtk_layer_output_t *layer_output_ptr,
    wlmtk_panel_t *panel_ptr);
static void _wlmtk_layer_output_remove_panel(
    wlmtk_layer_output_t *layer_output_ptr,
    wlmtk_panel_t *panel_ptr);

static void *_wlmtk_layer_output_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);
static void _wlmtk_layer_output_update(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);
static void _wlmtk_layer_output_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_layer_t *wlmtk_layer_create(
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    wlmtk_layer_t *layer_ptr = logged_calloc(1, sizeof(wlmtk_layer_t));
    if (NULL == layer_ptr) return NULL;

    if (!wlmtk_container_init(&layer_ptr->super_container)) {
        wlmtk_layer_destroy(layer_ptr);
        return NULL;
    }

    layer_ptr->output_tracker_ptr = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        wlr_output_layout_ptr,
        _wlmtk_layer_output_create,
        _wlmtk_layer_output_update,
        _wlmtk_layer_output_destroy);

    return layer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_destroy(wlmtk_layer_t *layer_ptr)
{
    if (NULL != layer_ptr->output_tracker_ptr) {
        wlmtk_output_tracker_destroy(layer_ptr->output_tracker_ptr);
        layer_ptr->output_tracker_ptr = NULL;
    }

    BS_ASSERT(bs_dllist_empty(&layer_ptr->super_container.elements));
    wlmtk_container_fini(&layer_ptr->super_container);
    free(layer_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_layer_element(wlmtk_layer_t *layer_ptr)
{
    return &layer_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_layer_add_panel(
    wlmtk_layer_t *layer_ptr,
    wlmtk_panel_t *panel_ptr,
    struct wlr_output *wlr_output_ptr)
{
    BS_ASSERT(NULL == wlmtk_panel_get_layer(panel_ptr));

    wlmtk_layer_output_t *layer_output_ptr = wlmtk_output_tracker_get_output(
        layer_ptr->output_tracker_ptr,
        wlr_output_ptr);
    if (NULL == layer_output_ptr) {
        bs_log(BS_WARNING, "Layer %p does not contain output %p",
               layer_ptr, wlr_output_ptr);
        return false;
    }

    wlmtk_container_add_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_panel_set_layer(panel_ptr, layer_ptr);
    _wlmtk_layer_output_add_panel(layer_output_ptr, panel_ptr);
    wlmtk_layer_output_reconfigure(layer_output_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_remove_panel(wlmtk_layer_t *layer_ptr,
                              wlmtk_panel_t *panel_ptr)
{
    BS_ASSERT(layer_ptr == wlmtk_panel_get_layer(panel_ptr));

    wlmtk_layer_output_t *layer_output_ptr = BS_ASSERT_NOTNULL(
        wlmtk_panel_get_layer_output(panel_ptr));
    _wlmtk_layer_output_remove_panel(layer_output_ptr, panel_ptr);

    wlmtk_panel_set_layer(panel_ptr, NULL);
    wlmtk_container_remove_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_layer_output_reconfigure(layer_output_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_output_reconfigure(
    wlmtk_layer_output_t *layer_output_ptr)
{
    struct wlr_box usable_area = layer_output_ptr->extents;

    for (bs_dllist_node_t *dlnode_ptr = layer_output_ptr->panels.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_panel_t *panel_ptr = wlmtk_panel_from_dlnode(dlnode_ptr);

        struct wlr_box new_usable_area = usable_area;
        struct wlr_box panel_dimensions = wlmtk_panel_compute_dimensions(
            panel_ptr, &layer_output_ptr->extents, &new_usable_area);

        if (wlmtk_panel_element(panel_ptr)->visible) {
            usable_area = new_usable_area;
        }

        wlmtk_panel_request_size(
            panel_ptr,
            panel_dimensions.width,
            panel_dimensions.height);
        wlmtk_element_set_position(
            wlmtk_panel_element(panel_ptr),
            panel_dimensions.x,
            panel_dimensions.y);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_set_workspace(wlmtk_layer_t *layer_ptr,
                               wlmtk_workspace_t *workspace_ptr)
{
    layer_ptr->workspace_ptr = workspace_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Removes `dlnode_ptr`'s panel from the layer output and destroys it. */
void _wlmtk_layer_output_remove_dlnode_panel(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    wlmtk_panel_t *panel_ptr = wlmtk_panel_from_dlnode(dlnode_ptr);

    wlmtk_layer_remove_panel(
        BS_ASSERT_NOTNULL(wlmtk_panel_get_layer(panel_ptr)),
        panel_ptr);

    wlmtk_element_destroy(wlmtk_panel_element(panel_ptr));
}

/* ------------------------------------------------------------------------- */
/** Adds the panel to the output. */
void _wlmtk_layer_output_add_panel(
    wlmtk_layer_output_t *layer_output_ptr,
    wlmtk_panel_t *panel_ptr)
{
    wlmtk_panel_set_layer_output(panel_ptr, layer_output_ptr);
    bs_dllist_push_back(
        &layer_output_ptr->panels,
        wlmtk_dlnode_from_panel(panel_ptr));
}

/* ------------------------------------------------------------------------- */
/** Removes the panel from the output. */
void _wlmtk_layer_output_remove_panel(
    wlmtk_layer_output_t *layer_output_ptr,
    wlmtk_panel_t *panel_ptr)
{
    BS_ASSERT(layer_output_ptr == wlmtk_panel_get_layer_output(panel_ptr));
    bs_dllist_remove(
        &layer_output_ptr->panels,
        wlmtk_dlnode_from_panel(panel_ptr));
    wlmtk_panel_set_layer_output(panel_ptr, NULL);
}

/* ------------------------------------------------------------------------- */
/** Creates a layer output for `wlr_output_ptr`. */
void *_wlmtk_layer_output_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout *wlr_output_layout_ptr = ud_ptr;
    wlmtk_layer_output_t *layer_output_ptr = logged_calloc(
        1, sizeof(wlmtk_layer_output_t));
    if (NULL == layer_output_ptr) return NULL;

    wlr_output_layout_get_box(
        wlr_output_layout_ptr,
        wlr_output_ptr,
        &layer_output_ptr->extents);

    return layer_output_ptr;
}

/* ------------------------------------------------------------------------- */
/** Reconfigures the layer, if the extents of this output changed. */
void _wlmtk_layer_output_update(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    struct wlr_output_layout *wlr_output_layout_ptr = ud_ptr;
    wlmtk_layer_output_t *layer_output_ptr = output_ptr;

    struct wlr_box new_extents;
    wlr_output_layout_get_box(
        wlr_output_layout_ptr,
        wlr_output_ptr,
        &new_extents);
    if (!wlr_box_equal(&new_extents, &layer_output_ptr->extents)) {
        layer_output_ptr->extents = new_extents;
        wlmtk_layer_output_reconfigure(layer_output_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Output is removed. Unlink all panels, and destroy the layer output. */
void _wlmtk_layer_output_destroy(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    __UNUSED__ void *ud_ptr,
    void *output_ptr)
{
    wlmtk_layer_output_t *layer_output_ptr = output_ptr;

    bs_dllist_for_each(
        &layer_output_ptr->panels,
        _wlmtk_layer_output_remove_dlnode_panel,
        NULL);
    free(layer_output_ptr);
}

/* == Unit tests =========================================================== */

static void test_multi_output(bs_test_t *test_ptr);
static void test_layout(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_layer_test_cases[] = {
    { 1, "multi_output", test_multi_output },
    { 1, "layout", test_layout },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests adding + removing outputs, and updates to panel positions. */
void test_multi_output(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);

    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layer_ptr);

    // First output. Add to the layout + update the layer right away.
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlr_output_layout_add(wlr_output_layout_ptr, &o1, 10, 20);

    // Add panel to the first output, and verify global positioning.
    wlmtk_panel_positioning_t p1 = {
        .desired_width = 100, .desired_height = 50 };
    wlmtk_fake_panel_t *fp1_ptr = wlmtk_fake_panel_create(&p1);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp1_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel(layer_ptr, &fp1_ptr->panel, &o1));

    // Position: 10 (output) + 1024/2 (half output width) - 50 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 472, wlmtk_panel_element(&fp1_ptr->panel)->x);
    // Position: 20 (output) + 768/2 (half output height) - 25 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 379, wlmtk_panel_element(&fp1_ptr->panel)->y);

    // Explicitly remove first panel.
    wlmtk_layer_remove_panel(layer_ptr, &fp1_ptr->panel);
    wlmtk_fake_panel_destroy(fp1_ptr);

    // Second output. Do not add to layout yet.
    struct wlr_output o2 = { .width = 640, .height = 480, .scale = 2.0 };
    wlmtk_test_wlr_output_init(&o2);

    // Attempt to add panel to the second output. Must fail.
    wlmtk_panel_positioning_t p2 = {
        .desired_width = 80, .desired_height = 36 };
    wlmtk_fake_panel_t *fp2_ptr = wlmtk_fake_panel_create(&p2);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp2_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_layer_add_panel(layer_ptr, &fp2_ptr->panel, &o2));

    // Now: Add second output. Adding the panel must now work.
    wlr_output_layout_add(wlr_output_layout_ptr, &o2, 400, 200);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel(layer_ptr, &fp2_ptr->panel, &o2));

    // Position: 400 (output) + 640/4 (half scaled output) - 40 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 520, wlmtk_panel_element(&fp2_ptr->panel)->x );
    // Position: 200 (output) + 480/4 (half scaled output) - 18 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 302, wlmtk_panel_element(&fp2_ptr->panel)->y);

    // Reposition the second output. Must reconfigure that panel's position.
    wlr_output_layout_add(wlr_output_layout_ptr, &o2, 500, 300);

    // Position: 500 (output) + 640/4 (half scaled output) - 40 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 620, wlmtk_panel_element(&fp2_ptr->panel)->x );
    // Position: 300 (output) + 480/4 (half scaled output) - 18 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 402, wlmtk_panel_element(&fp2_ptr->panel)->y);

    wlr_output_layout_remove(wlr_output_layout_ptr, &o1);

    // We leave the second output + panel in. Must be destroyed in layer dtor.
    wlmtk_layer_destroy(layer_ptr);

    wlr_output_layout_remove(wlr_output_layout_ptr, &o2);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests panel layout with multiple panels. */
void test_layout(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    static const wlmtk_tile_style_t ts = { .size = 64 };
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);

    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layer_ptr);
    wlmtk_layer_set_workspace(layer_ptr, ws_ptr);

    // Adds a left-bounded panel with an exclusive zone.
    wlmtk_panel_positioning_t pos = {
        .desired_width = 100,
        .desired_height = 50,
        .exclusive_zone = 40,
        .anchor = WLR_EDGE_LEFT
    };
    wlmtk_fake_panel_t *fp1_ptr = wlmtk_fake_panel_create(&pos);
    BS_ASSERT_NOTNULL(fp1_ptr);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp1_ptr->panel), true);

    wlmtk_layer_add_panel(layer_ptr, &fp1_ptr->panel, &output);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_panel_element(&fp1_ptr->panel)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 359, wlmtk_panel_element(&fp1_ptr->panel)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, fp1_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, fp1_ptr->requested_height);

    // Next panel is to respect the exclusive zone. It is invisible => later
    // panels won't shift, but it still will be positioned.
    pos.anchor = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_BOTTOM;
    pos.desired_height = 0;
    wlmtk_fake_panel_t *fp2_ptr = wlmtk_fake_panel_create(&pos);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp2_ptr->panel), false);
    BS_ASSERT_NOTNULL(fp2_ptr);
    wlmtk_layer_add_panel(layer_ptr, &fp2_ptr->panel, &output);
    BS_TEST_VERIFY_EQ(test_ptr, 40, wlmtk_panel_element(&fp2_ptr->panel)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_panel_element(&fp2_ptr->panel)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, fp2_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, fp2_ptr->requested_height);

    // Next panel: Same size and position, since the former is invisible.
    wlmtk_fake_panel_t *fp3_ptr = wlmtk_fake_panel_create(&pos);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp3_ptr->panel), true);
    BS_ASSERT_NOTNULL(fp3_ptr);
    wlmtk_layer_add_panel(layer_ptr, &fp3_ptr->panel, &output);
    BS_TEST_VERIFY_EQ(test_ptr, 40, wlmtk_panel_element(&fp3_ptr->panel)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_panel_element(&fp3_ptr->panel)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, fp3_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, fp3_ptr->requested_height);

    wlmtk_layer_remove_panel(layer_ptr, &fp3_ptr->panel);
    wlmtk_fake_panel_destroy(fp3_ptr);

    wlmtk_layer_remove_panel(layer_ptr, &fp2_ptr->panel);
    wlmtk_fake_panel_destroy(fp2_ptr);

    wlmtk_layer_remove_panel(layer_ptr, &fp1_ptr->panel);
    wlmtk_fake_panel_destroy(fp1_ptr);

    wlmtk_layer_set_workspace(layer_ptr, NULL);
    wlmtk_layer_destroy(layer_ptr);
    wlmtk_workspace_destroy(ws_ptr);

    wlr_output_layout_remove(wlr_output_layout_ptr, &output);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(display_ptr);
}

/* == End of layer.c ======================================================= */
