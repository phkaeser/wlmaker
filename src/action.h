/* ========================================================================= */
/**
 * @file action.h
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
#ifndef __WLMAKER_ACTION_H__
#define __WLMAKER_ACTION_H__

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** wlmaker actions. Can be bound to keys. */
typedef enum {
    WLMAKER_ACTION_QUIT,
    WLMAKER_ACTION_LOCK_SCREEN,
    WLMAKER_ACTION_LAUNCH_TERMINAL,

    WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS,
    WLMAKER_ACTION_WORKSPACE_TO_NEXT,

    WLMAKER_ACTION_WINDOW_RAISE,
    WLMAKER_ACTION_WINDOW_LOWER,
    WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN,
    WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED,
} wlmaker_action_t;

/**
 * Executes the given action on wlmaker.
 *
 * @param server_ptr
 * @param action
 */
void wlmaker_action_execute(
    wlmaker_server_t *server_ptr,
    wlmaker_action_t action);

/**
 * Binds the actions specified in the config dictionary.
 *
 * @param server_ptr
 * @param keybindings_dict_ptr
 *
 * @return true on success.
 */
bool wlmaker_action_bind_keys(
    wlmaker_server_t *server_ptr,
    wlmcfg_dict_t *keybindings_dict_ptr);

/** Unit test cases. */
extern const bs_test_case_t   wlmaker_action_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ACTION_H__ */
/* == End of action.h ====================================================== */
