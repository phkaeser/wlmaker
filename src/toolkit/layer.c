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

#include "container.h"
#include "test.h"


#define WLR_USE_UNSTABLE
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of a layer. */
struct _wlmtk_layer_t {
    /** Super class of the layer. */
    wlmtk_container_t         super_container;

    /** Workspace that the layer belongs to. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Panels, holds nodes at @ref wlmtk_panel_t::dlnode. */
    bs_dllist_t               panels;

    /** Holds outputs and panels. */
    bs_avltree_t              *output_tree_ptr;

    /** Used only in @ref wlmtk_layer_update_layout. The former tree. */
    bs_avltree_t              *former_output_tree_ptr;
    /** Used only in @ref wlmtk_layer_update_layout. Output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
};

/** An output node. */
typedef struct {
    /** Tree node, referenced from @ref wlmtk_layer_t::output_tree_ptr. */
    bs_avltree_node_t         avlnode;

    /** The WLR output that the panels in this node belong to. */
    struct wlr_output         *wlr_output_ptr;
    /** Extents and position of the output in the layout. */
    struct wlr_box            extents;
    /** Panels. Holds nodes at @ref wlmtk_panel_t::dlnode. */
    bs_dllist_t               panels;
} wlmtk_layer_output_tree_node_t;

static wlmtk_layer_output_tree_node_t *_wlmtk_layer_output_tree_node_create(
    struct wlr_output *wlr_output_ptr);
static void _wlmtk_layer_output_tree_node_destroy(
    bs_avltree_node_t *avlnode_ptr);
static int _wlmtk_layer_output_tree_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr);
static bool _wlmtk_layer_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr);
static void _wlmtk_layer_reconfigure_output(
    wlmtk_layer_output_tree_node_t *node_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_layer_t *wlmtk_layer_create(wlmtk_env_t *env_ptr)
{
    wlmtk_layer_t *layer_ptr = logged_calloc(1, sizeof(wlmtk_layer_t));
    if (NULL == layer_ptr) return NULL;

    if (!wlmtk_container_init(&layer_ptr->super_container, env_ptr)) {
        wlmtk_layer_destroy(layer_ptr);
        return NULL;
    }

    layer_ptr->output_tree_ptr = bs_avltree_create(
        _wlmtk_layer_output_tree_node_cmp,
        _wlmtk_layer_output_tree_node_destroy);
    if (NULL == layer_ptr->output_tree_ptr) {
        bs_log(BS_ERROR, "Failed bs_avltree_create(%p, NULL)",
               _wlmtk_layer_output_tree_node_cmp);
        wlmtk_layer_destroy(layer_ptr);
        return NULL;
    }

    return layer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_destroy(wlmtk_layer_t *layer_ptr)
{
    if (NULL != layer_ptr->output_tree_ptr) {
        bs_avltree_destroy(layer_ptr->output_tree_ptr);
        layer_ptr->output_tree_ptr = NULL;
    }
    wlmtk_container_fini(&layer_ptr->super_container);
    free(layer_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_layer_element(wlmtk_layer_t *layer_ptr)
{
    return &layer_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_add_panel(wlmtk_layer_t *layer_ptr,
                           wlmtk_panel_t *panel_ptr)
{
    BS_ASSERT(NULL == wlmtk_panel_get_layer(panel_ptr));
    wlmtk_container_add_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_panel_set_layer(panel_ptr, layer_ptr);
    bs_dllist_push_back(
        &layer_ptr->panels,
        wlmtk_dlnode_from_panel(panel_ptr));
    wlmtk_layer_reconfigure(layer_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_layer_add_panel_output(
    wlmtk_layer_t *layer_ptr,
    wlmtk_panel_t *panel_ptr,
    struct wlr_output *wlr_output_ptr)
{
    BS_ASSERT(NULL == wlmtk_panel_get_layer(panel_ptr));

    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        layer_ptr->output_tree_ptr,
        wlr_output_ptr);
    if (NULL == avlnode_ptr) {
        bs_log(BS_WARNING, "Layer %p does not contain output %p",
               layer_ptr, wlr_output_ptr);
        return false;
    }
    wlmtk_layer_output_tree_node_t *node_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_layer_output_tree_node_t, avlnode);

    wlmtk_container_add_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_panel_set_layer(panel_ptr, layer_ptr);
    bs_dllist_push_back(
        &node_ptr->panels,
        wlmtk_dlnode_from_panel(panel_ptr));

    _wlmtk_layer_reconfigure_output(node_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_remove_panel(wlmtk_layer_t *layer_ptr,
                              wlmtk_panel_t *panel_ptr)
{
    BS_ASSERT(layer_ptr == wlmtk_panel_get_layer(panel_ptr));
    bs_dllist_remove(
        &layer_ptr->panels,
        wlmtk_dlnode_from_panel(panel_ptr));
    wlmtk_panel_set_layer(panel_ptr, NULL);
    wlmtk_container_remove_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_layer_reconfigure(layer_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_reconfigure(wlmtk_layer_t *layer_ptr)
{
    struct wlr_box extents = wlmtk_workspace_get_fullscreen_extents(
        layer_ptr->workspace_ptr);
    struct wlr_box usable_area = extents;

    for (bs_dllist_node_t *dlnode_ptr = layer_ptr->panels.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_panel_t *panel_ptr = wlmtk_panel_from_dlnode(dlnode_ptr);

        struct wlr_box new_usable_area = usable_area;
        struct wlr_box panel_dimensions = wlmtk_panel_compute_dimensions(
            panel_ptr, &extents, &new_usable_area);

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
void wlmtk_layer_update_layout(
    wlmtk_layer_t *layer_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    BS_ASSERT(NULL == layer_ptr->former_output_tree_ptr);
    BS_ASSERT(NULL != layer_ptr->output_tree_ptr);
    layer_ptr->former_output_tree_ptr = layer_ptr->output_tree_ptr;
    layer_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    layer_ptr->output_tree_ptr = bs_avltree_create(
        _wlmtk_layer_output_tree_node_cmp,
        _wlmtk_layer_output_tree_node_destroy);
    BS_ASSERT(wlmtk_util_wl_list_for_each(
                  &wlr_output_layout_ptr->outputs,
                  _wlmtk_layer_update_output,
                  layer_ptr));

    bs_avltree_destroy(layer_ptr->former_output_tree_ptr);
    layer_ptr->former_output_tree_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_set_workspace(wlmtk_layer_t *layer_ptr,
                               wlmtk_workspace_t *workspace_ptr)
{
    layer_ptr->workspace_ptr = workspace_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Creates an output node for `wlr_output_ptr`. */
wlmtk_layer_output_tree_node_t *_wlmtk_layer_output_tree_node_create(
    struct wlr_output *wlr_output_ptr)
{
    wlmtk_layer_output_tree_node_t *node_ptr = logged_calloc(
        1, sizeof(wlmtk_layer_output_tree_node_t));
    if (NULL == node_ptr) return NULL;
    node_ptr->wlr_output_ptr = wlr_output_ptr;
    return node_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the output node. */
void _wlmtk_layer_output_tree_node_destroy(
    bs_avltree_node_t *avlnode_ptr)
{
    wlmtk_layer_output_tree_node_t *node_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_layer_output_tree_node_t, avlnode);

    free(node_ptr);
}

/* ------------------------------------------------------------------------- */
/** Compares @ref wlmtk_layer_output_tree_node_t::wlr_output_ptr. */
int _wlmtk_layer_output_tree_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr)
{
    wlmtk_layer_output_tree_node_t *node_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_layer_output_tree_node_t, avlnode);
    return bs_avltree_cmp_ptr(node_ptr->wlr_output_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the given output in @ref wlmtk_layer_t::output_tree_ptr.
 *
 * If the output already exists in @ref wlmtk_layer_t::former_output_tree_ptr,
 * the corresponding node will be moved from there. Otherwise, a new node is
 * created.
 * The panel layout will be recomputed, if the output's extents changed.
 *
 * @param link_ptr            struct wlr_output_layout_output::link.
 * @param ud_ptr              The @ref wlmtk_layer_t.
 *
 * @return true on success, or false on error.
 */
bool _wlmtk_layer_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_ptr->output;
    wlmtk_layer_t *layer_ptr = ud_ptr;

    wlmtk_layer_output_tree_node_t *node_ptr = NULL;
    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        layer_ptr->former_output_tree_ptr, wlr_output_ptr);
    if (NULL != avlnode_ptr) {
        node_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmtk_layer_output_tree_node_t, avlnode);
    } else {
        node_ptr = _wlmtk_layer_output_tree_node_create(wlr_output_ptr);
        if (NULL == node_ptr) return false;
        avlnode_ptr = &node_ptr->avlnode;
    }

    struct wlr_box new_extents;
    wlr_output_layout_get_box(
        layer_ptr->wlr_output_layout_ptr,
        wlr_output_ptr,
        &new_extents);
    if (!wlr_box_equal(&new_extents, &node_ptr->extents)) {
        node_ptr->extents = new_extents;
        _wlmtk_layer_reconfigure_output(node_ptr);
    }

    return bs_avltree_insert(
        layer_ptr->output_tree_ptr, wlr_output_ptr, avlnode_ptr, false);
}

/* ------------------------------------------------------------------------- */
/** Reconfigures the panel positions within the output of the layer. */
void _wlmtk_layer_reconfigure_output(
    wlmtk_layer_output_tree_node_t *node_ptr)
{
    struct wlr_box usable_area = node_ptr->extents;

    for (bs_dllist_node_t *dlnode_ptr = node_ptr->panels.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_panel_t *panel_ptr = wlmtk_panel_from_dlnode(dlnode_ptr);

        struct wlr_box new_usable_area = usable_area;
        struct wlr_box panel_dimensions = wlmtk_panel_compute_dimensions(
            panel_ptr, &node_ptr->extents, &new_usable_area);

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

/* == Unit tests =========================================================== */

static void test_add_remove(bs_test_t *test_ptr);
static void test_update_layout(bs_test_t *test_ptr);
static void test_layout(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_layer_test_cases[] = {
    { 1, "add_remove", test_add_remove },
    { 1, "update_layout", test_update_layout },
    { 1, "layout", test_layout },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises the panel add & remove methods. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create_for_test(1024, 768, 0);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layer_ptr);
    wlmtk_layer_set_workspace(layer_ptr, ws_ptr);

    wlmtk_panel_positioning_t pos = {
        .desired_width = 100,
        .desired_height = 50
    };
    wlmtk_fake_panel_t *fake_panel_ptr = BS_ASSERT_NOTNULL(
        wlmtk_fake_panel_create(&pos));
    BS_TEST_VERIFY_EQ(test_ptr, 0, fake_panel_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 0, fake_panel_ptr->requested_height);

    // Adds the panel. Triggers a 'compute_dimensions' call and then calls
    // into wlmtk_panel_request_size.
    wlmtk_layer_add_panel(layer_ptr, &fake_panel_ptr->panel);
    BS_TEST_VERIFY_EQ(
        test_ptr, layer_ptr,
        wlmtk_panel_get_layer(&fake_panel_ptr->panel));
    BS_TEST_VERIFY_EQ(test_ptr, 100, fake_panel_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 50, fake_panel_ptr->requested_height);

    wlmtk_layer_remove_panel(layer_ptr, &fake_panel_ptr->panel);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_panel_get_layer(&fake_panel_ptr->panel));

    wlmtk_fake_panel_destroy(fake_panel_ptr);

    wlmtk_layer_set_workspace(layer_ptr, NULL);
    wlmtk_layer_destroy(layer_ptr);
    wlmtk_workspace_destroy(ws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests adding + removing outputs, and updates to panel positions. */
void test_update_layout(bs_test_t *test_ptr)
{
    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layer_ptr);

    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *layout_ptr = wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layout_ptr);

    // First output. Add to the layout + update the layer right away.
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlr_output_layout_add(layout_ptr, &o1, 10, 20);
    wlmtk_layer_update_layout(layer_ptr, layout_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, 1, bs_avltree_size(layer_ptr->output_tree_ptr));

    // Add panel to the first output, and verify global positioning.
    wlmtk_panel_positioning_t p1 = {
        .desired_width = 100, .desired_height = 50 };
    wlmtk_fake_panel_t *fp1_ptr = wlmtk_fake_panel_create(&p1);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp1_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel_output(layer_ptr, &fp1_ptr->panel, &o1));

    // Position: 10 (output) + 1024/2 (half output width) - 50 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 472, wlmtk_panel_element(&fp1_ptr->panel)->x );
    // Position: 20 (output) + 768/2 (half output height) - 25 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 379, wlmtk_panel_element(&fp1_ptr->panel)->y);

    // Second output. Do not add to layout yet.
    struct wlr_output o2 = { .width = 640, .height = 480, .scale = 2.0 };
    wlmtk_test_wlr_output_init(&o2);
    wlr_output_layout_add(layout_ptr, &o2, 400, 200);

    // Attempt to add panel to the second output. Must fail.
    wlmtk_panel_positioning_t p2 = {
        .desired_width = 80, .desired_height = 36 };
    wlmtk_fake_panel_t *fp2_ptr = wlmtk_fake_panel_create(&p2);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp2_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_layer_add_panel_output(layer_ptr, &fp2_ptr->panel, &o2));

    // Now: Add second output. Adding the panel must now work.
    wlmtk_layer_update_layout(layer_ptr, layout_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, 2, bs_avltree_size(layer_ptr->output_tree_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel_output(layer_ptr, &fp2_ptr->panel, &o2));

    // Position: 400 (output) + 640/4 (half scaled output) - 40 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 520, wlmtk_panel_element(&fp2_ptr->panel)->x );
    // Position: 200 (output) + 480/4 (half scaled output) - 18 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 302, wlmtk_panel_element(&fp2_ptr->panel)->y);

    // Reposition the second output. Must reconfigure that panel's position.
    wlr_output_layout_add(layout_ptr, &o2, 500, 300);
    wlmtk_layer_update_layout(layer_ptr, layout_ptr);

    // Position: 500 (output) + 640/4 (half scaled output) - 40 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 620, wlmtk_panel_element(&fp2_ptr->panel)->x );
    // Position: 300 (output) + 480/4 (half scaled output) - 18 (half panel).
    BS_TEST_VERIFY_EQ(test_ptr, 402, wlmtk_panel_element(&fp2_ptr->panel)->y);

    wlr_output_layout_remove(layout_ptr, &o1);
    wlmtk_layer_update_layout(layer_ptr, layout_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, 1, bs_avltree_size(layer_ptr->output_tree_ptr));

    wlmtk_layer_destroy(layer_ptr);

    wlr_output_layout_remove(layout_ptr, &o2);
    wlr_output_layout_destroy(layout_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests panel layout with multiple panels. */
void test_layout(bs_test_t *test_ptr)
{
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create_for_test(1024, 768, 0);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(NULL);
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

    wlmtk_layer_add_panel(layer_ptr, &fp1_ptr->panel);
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
    wlmtk_layer_add_panel(layer_ptr, &fp2_ptr->panel);
    BS_TEST_VERIFY_EQ(test_ptr, 40, wlmtk_panel_element(&fp2_ptr->panel)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_panel_element(&fp2_ptr->panel)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, fp2_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, fp2_ptr->requested_height);

    // Next panel: Same size and position, since the former is invisible.
    wlmtk_fake_panel_t *fp3_ptr = wlmtk_fake_panel_create(&pos);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp3_ptr->panel), true);
    BS_ASSERT_NOTNULL(fp3_ptr);
    wlmtk_layer_add_panel(layer_ptr, &fp3_ptr->panel);
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
}

/* == End of layer.c ======================================================= */
