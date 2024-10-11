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

/** wlmaker actions. Can be bound to keys. Also @see wlmaker_action_desc. */
typedef enum {
    WLMAKER_ACTION_NONE,

    WLMAKER_ACTION_QUIT,
    WLMAKER_ACTION_LOCK_SCREEN,
    WLMAKER_ACTION_LOCK_INHIBIT_BEGIN,
    WLMAKER_ACTION_LOCK_INHIBIT_END,
    WLMAKER_ACTION_LAUNCH_TERMINAL,

    WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS,
    WLMAKER_ACTION_WORKSPACE_TO_NEXT,

    WLMAKER_ACTION_TASK_TO_PREVIOUS,
    WLMAKER_ACTION_TASK_TO_NEXT,

    WLMAKER_ACTION_WINDOW_RAISE,
    WLMAKER_ACTION_WINDOW_LOWER,
    WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN,
    WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED,

    // Note: Keep these numbered consecutively.
    WLMAKER_ACTION_SWITCH_TO_VT1,
    WLMAKER_ACTION_SWITCH_TO_VT2,
    WLMAKER_ACTION_SWITCH_TO_VT3,
    WLMAKER_ACTION_SWITCH_TO_VT4,
    WLMAKER_ACTION_SWITCH_TO_VT5,
    WLMAKER_ACTION_SWITCH_TO_VT6,
    WLMAKER_ACTION_SWITCH_TO_VT7,
    WLMAKER_ACTION_SWITCH_TO_VT8,
    WLMAKER_ACTION_SWITCH_TO_VT9,
    WLMAKER_ACTION_SWITCH_TO_VT10,
    WLMAKER_ACTION_SWITCH_TO_VT11,
    WLMAKER_ACTION_SWITCH_TO_VT12,
} wlmaker_action_t;

extern const char *wlmaker_action_config_dict_key;

extern const wlmcfg_enum_desc_t wlmaker_action_desc[];

/** Forward declaration: Handle for bound actions. */
typedef struct _wlmaker_action_handle_t wlmaker_action_handle_t;

/**
 * Binds the actions specified in the config dictionary.
 *
 * @param server_ptr
 * @param keybindings_dict_ptr
 *
 * @return A bound action handle, or NULL on error.
 */
wlmaker_action_handle_t *wlmaker_action_bind_keys(
    wlmaker_server_t *server_ptr,
    wlmcfg_dict_t *keybindings_dict_ptr);

/**
 * Unbinds actions previously bound by @ref wlmaker_action_bind_keys.
 *
 * @param handle_ptr
 */
void wlmaker_action_unbind_keys(wlmaker_action_handle_t *handle_ptr);

/**
 * Executes the given action on wlmaker.
 *
 * @param server_ptr
 * @param action
 */
void wlmaker_action_execute(
    wlmaker_server_t *server_ptr,
    wlmaker_action_t action);

/** Unit test cases. */
extern const bs_test_case_t   wlmaker_action_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ACTION_H__ */
/* == End of action.h ====================================================== */
