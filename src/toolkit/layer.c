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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of a layer. */
struct _wlmtk_layer_t {
    /** Super class of the layer. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Workspace that the layer belongs to. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Panels, holds nodes at @ref wlmtk_panel_t::dlnode. */
    bs_dllist_t               panels;
};

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

    return layer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_destroy(wlmtk_layer_t *layer_ptr)
{
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

        // FIXME: Add tests & don't update usable_area if not visible.

        struct wlr_box panel_dimensions = wlmtk_panel_compute_dimensions(
            panel_ptr, &extents, &usable_area);

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

/* == Unit tests =========================================================== */

static void test_add_remove(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_layer_test_cases[] = {
    { 1, "add_remove", test_add_remove },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises the panel add & remove methods. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_layer_t *layer_ptr = BS_ASSERT_NOTNULL(wlmtk_layer_create(NULL));
    wlmtk_fake_workspace_t *fake_workspace_ptr = BS_ASSERT_NOTNULL(
        wlmtk_fake_workspace_create(1024, 768));
    wlmtk_layer_set_workspace(layer_ptr, fake_workspace_ptr->workspace_ptr);

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
    wlmtk_fake_workspace_destroy(fake_workspace_ptr);
    wlmtk_layer_destroy(layer_ptr);
}

/* == End of layer.c ======================================================= */
