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

/* == Declarations ========================================================= */

/** State of a layer. */
struct _wlmtk_layer_t {
    /** Super class of the layer. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;
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
    wlmtk_container_add_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
    wlmtk_panel_set_layer(panel_ptr, layer_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_layer_remove_panel(wlmtk_layer_t *layer_ptr,
                              wlmtk_panel_t *panel_ptr)
{
    wlmtk_panel_set_layer(panel_ptr, NULL);
    wlmtk_container_remove_element(
        &layer_ptr->super_container,
        wlmtk_panel_element(panel_ptr));
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
    wlmtk_layer_t *layer_ptr = wlmtk_layer_create(NULL);
    BS_ASSERT(NULL != layer_ptr);

    wlmtk_panel_t panel;
    BS_ASSERT(wlmtk_panel_init(&panel, NULL));

    wlmtk_layer_add_panel(layer_ptr, &panel);
    BS_TEST_VERIFY_EQ(test_ptr, layer_ptr, panel.layer_ptr);

    wlmtk_layer_remove_panel(layer_ptr, &panel);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, panel.layer_ptr);

    wlmtk_panel_fini(&panel);
    wlmtk_layer_destroy(layer_ptr);
}

/* == End of layer.c ======================================================= */
