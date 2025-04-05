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

#include <toolkit/toolkit.h>

/** Toolkit unit tests. */
const bs_test_set_t toolkit_tests[] = {
    { 1, "bordered", wlmtk_bordered_test_cases },
    { 1, "box", wlmtk_box_test_cases },
    { 1, "button", wlmtk_button_test_cases },
    { 1, "container", wlmtk_container_test_cases },
    { 1, "content", wlmtk_content_test_cases },
    { 1, "dock", wlmtk_dock_test_cases },
    { 1, "element", wlmtk_element_test_cases },
    { 1, "fsm", wlmtk_fsm_test_cases },
    { 1, "image", wlmtk_image_test_cases },
    { 1, "layer", wlmtk_layer_test_cases },
    { 1, "menu", wlmtk_menu_test_cases },
    { 1, "menu_item", wlmtk_menu_item_test_cases },
    { 1, "pane", wlmtk_pane_test_cases },
    { 1, "panel", wlmtk_panel_test_cases },
    { 1, "surface", wlmtk_surface_test_cases },
    { 1, "rectangle", wlmtk_rectangle_test_cases },
    { 1, "resizebar", wlmtk_resizebar_test_cases },
    { 1, "resizebar_area", wlmtk_resizebar_area_test_cases },
    { 1, "root", wlmtk_root_test_cases },
    { 1, "tile", wlmtk_tile_test_cases },
    { 1, "titlebar", wlmtk_titlebar_test_cases },
    { 1, "titlebar_button", wlmtk_titlebar_button_test_cases },
    { 1, "titlebar_title", wlmtk_titlebar_title_test_cases },
    { 1, "util", wlmtk_util_test_cases },
    { 1, "window", wlmtk_window_test_cases },
    { 1, "workspace", wlmtk_workspace_test_cases },
    { 1, "primitives", wlmaker_primitives_test_cases },
    { 0, NULL, NULL }
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
    return bs_test(toolkit_tests, argc, argv, &params);
}

/* == End of toolkit_test.c ================================================ */
