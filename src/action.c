/* ========================================================================= */
/**
 * @file action.c
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

#include "action.h"

#include "server.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmaker_action_execute(wlmaker_server_t *server_ptr,
                            wlmaker_action_t action)
{
    wlmaker_workspace_t *workspace_ptr;
    wlmtk_workspace_t *wlmtk_workspace_ptr;
    wlmtk_window_t *window_ptr;

    switch (action) {
    case WLMAKER_ACTION_QUIT:
        wl_display_terminate(server_ptr->wl_display_ptr);
        break;

    case WLMAKER_ACTION_LOCK_SCREEN:
        if (NULL != server_ptr->idle_monitor_ptr) {
            wlmaker_idle_monitor_lock(server_ptr->idle_monitor_ptr);
        }
        break;

    case WLMAKER_ACTION_LAUNCH_TERMINAL:
        if (0 == fork()) {
            execl("/bin/sh", "/bin/sh", "-c", "/usr/bin/foot", (void *)NULL);
        }
        break;

    case WLMAKER_ACTION_WORKSPACE_SWITCH_TO_PREVIOUS:
        wlmaker_server_switch_to_previous_workspace(server_ptr);
        break;

    case WLMAKER_ACTION_WORKSPACE_SWITCH_TO_NEXT:
        wlmaker_server_switch_to_next_workspace(server_ptr);
        break;

    case WLMAKER_ACTION_WINDOW_RAISE:
        // TODO(kaeser@gubbe.ch): (re)implement using toolkit.
        bs_log(BS_WARNING, "Raise window: Unimplemented.");
        break;

    case WLMAKER_ACTION_WINDOW_LOWER:
        // TODO(kaeser@gubbe.ch): (re)implement using toolkit.
        bs_log(BS_WARNING, "Lower window: Unimplemented.");
        break;

    case WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN:
        workspace_ptr = wlmaker_server_get_current_workspace(server_ptr);
        wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(workspace_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(wlmtk_workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_fullscreen(
                window_ptr, !wlmtk_window_is_fullscreen(window_ptr));
        }
        break;

    case WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED:
        workspace_ptr = wlmaker_server_get_current_workspace(server_ptr);
        wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(workspace_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(wlmtk_workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_maximized(
                window_ptr, !wlmtk_window_is_maximized(window_ptr));
        }
        break;

    default:
        bs_log(BS_WARNING, "Unhandled action %d.", action);
        break;
    }
}

/* == Local (static) methods =============================================== */

/* == End of action.c ====================================================== */
