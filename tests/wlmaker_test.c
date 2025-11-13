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
#include <stdlib.h>

#include "action.h"
#include "action_item.h"
#include "backtrace.h"
#include "clip.h"
#include "config.h"
#include "corner.h"
#include "dock.h"
#include "files.h"
#include "launcher.h"
#include "layer_panel.h"
#include "lock_mgr.h"
#include "root_menu.h"
#include "server.h"
#include "xdg_decoration.h"
#include "xdg_toplevel.h"
#if defined(WLMAKER_HAVE_XWAYLAND)
#include "xwl_surface.h"
#endif  // defined(WLMAKER_HAVE_XWAYLAND)

#if !defined(TEST_DATA_DIR)
/** Directory root for looking up test data. See `bs_test_resolve_path`. */
#define TEST_DATA_DIR "./"
#endif  // TEST_DATA_DIR

/** Main program, runs the unit tests. */
int main(int argc, const char **argv)
{
    if (!wlmaker_backtrace_setup(argv[0])) return EXIT_FAILURE;

    const bs_test_param_t params = { .test_data_dir_ptr = TEST_DATA_DIR };
    const bs_test_set_t* sets[] = {
        &wlmaker_action_item_test_set,
        &wlmaker_action_test_set,
        &wlmaker_clip_test_set,
        &wlmaker_config_test_set,
        &wlmaker_corner_test_set,
        &wlmaker_dock_test_set,
        &wlmaker_files_test_set,
        &wlmaker_launcher_test_set,
        &wlmaker_layer_panel_test_set,
        &wlmaker_lock_mgr_test_set,
        &wlmaker_root_menu_test_set,
        &wlmaker_server_test_set,
        &wlmaker_xdg_decoration_test_set,
        &wlmaker_xdg_toplevel_test_set,
#if defined(WLMAKER_HAVE_XWAYLAND)
        &wlmaker_xwl_surface_test_set,
#endif  // defined(WLMAKER_HAVE_XWAYLAND)
        NULL
    };
    return bs_test_sets(sets, argc, argv, &params);
}

/* == End of wlmaker_test.c ================================================ */
