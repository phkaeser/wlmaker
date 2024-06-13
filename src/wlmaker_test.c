/* ========================================================================= */
/**
 * @file wlmaker_test.c
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

#include "config.h"
#include "decorations.h"
#include "dock.h"
#include "launcher.h"
#include "layer_panel.h"
#include "menu.h"
#include "menu_item.h"
#include "workspace.h"
#include "xwl_content.h"

/** WLMaker unit tests. */
const bs_test_set_t wlmaker_tests[] = {
    { 1, "config", wlmaker_config_test_cases },
    { 1, "decorations", wlmaker_decorations_test_cases },
    { 1, "dock", wlmaker_dock_test_cases },
    { 1, "launcher", wlmaker_launcher_test_cases},
    { 1, "layer_panel", wlmaker_layer_panel_test_cases },
    { 1, "menu", wlmaker_menu_test_cases },
    { 1, "menu_item", wlmaker_menu_item_test_cases },
    { 1, "xwl_content", wlmaker_xwl_content_test_cases },
    // Known to be broken, ignore for now. TODO(kaeser@gubbe.ch): Fix.
    { 0, "workspace", wlmaker_workspace_test_cases },
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
    return bs_test(wlmaker_tests, argc, argv, &params);
}

/* == End of wlmaker_test.c ================================================ */
