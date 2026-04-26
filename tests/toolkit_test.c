/* ========================================================================= */
/**
 * @file toolkit_test.c
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

#include <stddef.h>
#include <libbase/libbase.h>

#include "toolkit/toolkit.h"

/** Toolkit unit tests. */
const bs_test_set_t *toolkit_test_sets[] = {
    &wlmtk_base_test_set,
    &wlmtk_bordered_test_set,
    &wlmtk_box_test_set,
    &wlmtk_buffer_test_set,
    &wlmtk_button_test_set,
    &wlmtk_container_test_set,
    &wlmtk_dock_test_set,
    &wlmtk_element_test_set,
    &wlmtk_fsm_test_set,
    &wlmtk_image_test_set,
    &wlmtk_layer_test_set,
    &wlmtk_menu_test_set,
    &wlmtk_menu_item_test_set,
    &wlmtk_output_tracker_test_set,
    &wlmtk_panel_test_set,
    &wlmaker_primitives_test_set,
    &wlmtk_rectangle_test_set,
    &wlmtk_resizebar_test_set,
    &wlmtk_resizebar_area_test_set,
    &wlmtk_root_test_set,
    &wlmtk_style_test_set,
    &wlmtk_surface_test_set,
    &wlmtk_tile_test_set,
    &wlmtk_titlebar_test_set,
    &wlmtk_titlebar_button_test_set,
    &wlmtk_titlebar_title_test_set,
    &wlmtk_util_test_set,
    &wlmtk_window_test_set,
    &wlmtk_workspace_test_set,
    NULL
};

#if !defined(TEST_DATA_DIR)
/** Directory root for looking up test data. See `bs_test_resolve_path`. */
#define TEST_DATA_DIR "./"
#endif  // TEST_DATA_DIR

/** Main program, runs the unit tests. */
int main(int argc, const char **argv)
{
    const bs_test_param_t params = {
        .test_data_dir_ptr   = TEST_DATA_DIR
    };
    return bs_test_sets(toolkit_test_sets, argc, argv, &params);
}

/* == End of toolkit_test.c ================================================ */
