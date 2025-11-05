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

#include <libbase/libbase.h>
#include <stddef.h>

#include "action.h"
#include "action_item.h"
#include "clip.h"
#include "config.h"
#include "corner.h"
#include "dock.h"
#include "launcher.h"
#include "layer_panel.h"
#include "lock_mgr.h"
#include "root_menu.h"
#include "server.h"
#include "xdg_decoration.h"
#if defined(WLMAKER_HAVE_XWAYLAND)
#include "xwl_surface.h"
#endif  // defined(WLMAKER_HAVE_XWAYLAND)

/** WLMaker unit tests. */
const bs_test_set_t wlmaker_tests[] = {
    { 1, "action", wlmaker_action_test_cases },
    { 1, "action_item", wlmaker_action_item_test_cases },
    { 1, "clip", wlmaker_clip_test_cases },
    { 1, "config", wlmaker_config_test_cases },
    { 1, "corner", wlmaker_corner_test_cases },
    { 1, "dock", wlmaker_dock_test_cases },
    { 1, "launcher", wlmaker_launcher_test_cases},
    { 1, "layer_panel", wlmaker_layer_panel_test_cases },
    { 1, "lock", wlmaker_lock_mgr_test_cases },
    { 1, "root_menu", wlmaker_root_menu_test_cases },
    { 1, "server", wlmaker_server_test_cases },
    { 1, "xdg_decoration", wlmaker_xdg_decoration_test_cases },
#if defined(WLMAKER_HAVE_XWAYLAND)
    { 1, "xwl_surface", wlmaker_xwl_surface_test_cases },
#endif  // defined(WLMAKER_HAVE_XWAYLAND)
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
