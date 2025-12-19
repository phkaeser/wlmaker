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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

#include "backend/backend.h"
#include "background.h"
#include "cursor.h"
#include "default_configuration.h"
#include "idle.h"
#include "keyboard.h"
#include "root_menu.h"
#include "server.h"
#include "subprocess_monitor.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of the bound actions. */
struct _wlmaker_action_handle_t {
    /** Bindings, linked by @ref _wlmaker_action_binding_t::qnode. */
    bs_dequeue_t              bindings;
    /** Back-link to server state. */
    wlmaker_server_t          *server_ptr;
    /** Whether to add 'Logo' to the bindings. */
    bool                      add_logo;
};

/** Key binding for a standard action. */
typedef struct {
    /** Node of @ref wlmaker_action_handle_t::bindings. */
    bs_dequeue_node_t         qnode;
    /** The key binding. */
    wlmaker_key_combo_t       key_combo;
    /** The associated action. */
    wlmaker_action_t          action;
    /** The key binding it to this node. */
    wlmaker_key_binding_t     *key_binding_ptr;
    /** State of the bound actions. */
    wlmaker_action_handle_t   *handle_ptr;
} _wlmaker_action_binding_t;

static bool _wlmaker_keybindings_parse(
    const char *string_ptr,
    uint32_t *modifiers_ptr,
    xkb_keysym_t *keysym_ptr);

static bool _wlmaker_keybindings_bind_item(
    const char *key_ptr,
    bspl_object_t *object_ptr,
    void *userdata_ptr);

static bool _wlmaker_action_bound_callback(
    const wlmaker_key_combo_t *binding_ptr);

/* == Data ================================================================= */

/** Key to lookup the dict from the config dictionary. */
const char *wlmaker_action_config_dict_key = "KeyBindings";

/** Supported modifiers for key bindings. */
static const bspl_enum_desc_t _wlmaker_keybindings_modifiers[] = {
    BSPL_ENUM("Shift", WLR_MODIFIER_SHIFT),
    // Caps? Maybe not: BSPL_ENUM("Caps", WLR_MODIFIER_CAPS),
    BSPL_ENUM("Ctrl", WLR_MODIFIER_CTRL),
    BSPL_ENUM("Alt", WLR_MODIFIER_ALT),
    BSPL_ENUM("Mod2", WLR_MODIFIER_MOD2),
    BSPL_ENUM("Mod3", WLR_MODIFIER_MOD3),
    BSPL_ENUM("Logo", WLR_MODIFIER_LOGO),
    BSPL_ENUM("Mod5", WLR_MODIFIER_MOD5),
    BSPL_ENUM_SENTINEL(),
};

/** The actions that can be bound. */
const bspl_enum_desc_t wlmaker_action_desc[] = {
    BSPL_ENUM("None", WLMAKER_ACTION_NONE),
    BSPL_ENUM("Quit", WLMAKER_ACTION_QUIT),
    BSPL_ENUM("LockScreen", WLMAKER_ACTION_LOCK_SCREEN),
    BSPL_ENUM("InhibitLockBegin", WLMAKER_ACTION_LOCK_INHIBIT_BEGIN),
    BSPL_ENUM("InhibitLockEnd", WLMAKER_ACTION_LOCK_INHIBIT_END),
    BSPL_ENUM("LaunchTerminal", WLMAKER_ACTION_LAUNCH_TERMINAL),
    BSPL_ENUM("ShellExecute", WLMAKER_ACTION_SHELL_EXECUTE),
    BSPL_ENUM("Execute", WLMAKER_ACTION_EXECUTE),

    BSPL_ENUM("WorkspacePrevious", WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS),
    BSPL_ENUM("WorkspaceNext", WLMAKER_ACTION_WORKSPACE_TO_NEXT),
    BSPL_ENUM("WorkspaceAdd", WLMAKER_ACTION_WORKSPACE_ADD),
    BSPL_ENUM("WorkspaceDestroyLast", WLMAKER_ACTION_WORKSPACE_DESTROY_LAST),

    BSPL_ENUM("TaskPrevious", WLMAKER_ACTION_TASK_TO_PREVIOUS),
    BSPL_ENUM("TaskNext", WLMAKER_ACTION_TASK_TO_NEXT),

    BSPL_ENUM("WindowRaise", WLMAKER_ACTION_WINDOW_RAISE),
    BSPL_ENUM("WindowLower", WLMAKER_ACTION_WINDOW_LOWER),
    BSPL_ENUM("WindowToggleFullscreen", WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN),
    BSPL_ENUM("WindowToggleMaximized", WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED),

    BSPL_ENUM("WindowMaximize", WLMAKER_ACTION_WINDOW_MAXIMIZE),
    BSPL_ENUM("WindowUnmaximize", WLMAKER_ACTION_WINDOW_UNMAXIMIZE),
    BSPL_ENUM("WindowFullscreen", WLMAKER_ACTION_WINDOW_FULLSCREEN),
    BSPL_ENUM("WindowShade", WLMAKER_ACTION_WINDOW_SHADE),
    BSPL_ENUM("WindowUnshade", WLMAKER_ACTION_WINDOW_UNSHADE),
    BSPL_ENUM("WindowClose", WLMAKER_ACTION_WINDOW_CLOSE),
    BSPL_ENUM("WindowToNextWorkspace", WLMAKER_ACTION_WINDOW_TO_NEXT_WORKSPACE),
    BSPL_ENUM("WindowToPreviousWorkspace", WLMAKER_ACTION_WINDOW_TO_PREVIOUS_WORKSPACE),

    BSPL_ENUM("RootMenu", WLMAKER_ACTION_ROOT_MENU),

    BSPL_ENUM("OutputMagnify", WLMAKER_ACTION_OUTPUT_MAGNIFY),
    BSPL_ENUM("OutputReduce", WLMAKER_ACTION_OUTPUT_REDUCE),
    BSPL_ENUM("OutputSaveState", WLMAKER_ACTION_OUTPUT_SAVE_STATE),

    BSPL_ENUM("SwitchToVT1", WLMAKER_ACTION_SWITCH_TO_VT1),
    BSPL_ENUM("SwitchToVT2", WLMAKER_ACTION_SWITCH_TO_VT2),
    BSPL_ENUM("SwitchToVT3", WLMAKER_ACTION_SWITCH_TO_VT3),
    BSPL_ENUM("SwitchToVT4", WLMAKER_ACTION_SWITCH_TO_VT4),
    BSPL_ENUM("SwitchToVT5", WLMAKER_ACTION_SWITCH_TO_VT5),
    BSPL_ENUM("SwitchToVT6", WLMAKER_ACTION_SWITCH_TO_VT6),
    BSPL_ENUM("SwitchToVT7", WLMAKER_ACTION_SWITCH_TO_VT7),
    BSPL_ENUM("SwitchToVT8", WLMAKER_ACTION_SWITCH_TO_VT8),
    BSPL_ENUM("SwitchToVT9", WLMAKER_ACTION_SWITCH_TO_VT9),
    BSPL_ENUM("SwitchToVT10", WLMAKER_ACTION_SWITCH_TO_VT10),
    BSPL_ENUM("SwitchToVT11", WLMAKER_ACTION_SWITCH_TO_VT11),
    BSPL_ENUM("SwitchToVT12", WLMAKER_ACTION_SWITCH_TO_VT12),

    // A duplicate to ShellExecute, permits `wmmenugen` compatibility.
    BSPL_ENUM("SHEXEC", WLMAKER_ACTION_SHELL_EXECUTE),
    // A duplicate to Execute, permits compatibility with Window Maker.
    BSPL_ENUM("EXEC", WLMAKER_ACTION_EXECUTE),

    BSPL_ENUM_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_action_handle_t *wlmaker_action_bind_keys(
    wlmaker_server_t *server_ptr,
    bspl_dict_t *keybindings_dict_ptr,
    bool add_logo)
{
    wlmaker_action_handle_t *handle_ptr = logged_calloc(
        1, sizeof(wlmaker_action_handle_t));
    if (NULL == handle_ptr) return NULL;
    handle_ptr->server_ptr = server_ptr;
    handle_ptr->add_logo = add_logo;

    if (bspl_dict_foreach(
            keybindings_dict_ptr,
            _wlmaker_keybindings_bind_item,
            handle_ptr)) {
        return handle_ptr;
    }

    wlmaker_action_unbind_keys(handle_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_action_unbind_keys(wlmaker_action_handle_t *handle_ptr)
{
    bs_dequeue_node_t *qnode_ptr = handle_ptr->bindings.head_ptr;
    while (NULL != qnode_ptr) {
        _wlmaker_action_binding_t *binding_ptr = BS_CONTAINER_OF(
            qnode_ptr, _wlmaker_action_binding_t, qnode);
        qnode_ptr = qnode_ptr->next_ptr;
        wlmaker_server_unbind_key(
            handle_ptr->server_ptr,
            binding_ptr->key_binding_ptr);
        free(binding_ptr);
    }

    free(handle_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_action_execute(wlmaker_server_t *server_ptr,
                            wlmaker_action_t action,
                            void *arg_ptr)
{
    wlmtk_workspace_t *workspace_ptr, *next_workspace_ptr;
    wlmtk_window_t *window_ptr;
    const char **argv;

    switch (action) {
    case WLMAKER_ACTION_NONE:
        break;

    case WLMAKER_ACTION_QUIT:
        wl_display_terminate(server_ptr->wl_display_ptr);
        break;

    case WLMAKER_ACTION_LOCK_SCREEN:
        if (NULL != server_ptr->idle_monitor_ptr) {
            wlmaker_idle_monitor_lock(server_ptr->idle_monitor_ptr);
        }
        break;

    case WLMAKER_ACTION_LOCK_INHIBIT_BEGIN:
        wlmaker_idle_monitor_inhibit(server_ptr->idle_monitor_ptr);
        break;

    case WLMAKER_ACTION_LOCK_INHIBIT_END:
        wlmaker_idle_monitor_uninhibit(server_ptr->idle_monitor_ptr);
        break;

    case WLMAKER_ACTION_LAUNCH_TERMINAL:
        argv = (const char*[]){ "/bin/sh", "-c", "/usr/bin/foot", NULL };
        wlmaker_subprocess_monitor_run(
            server_ptr->monitor_ptr,
            bs_subprocess_create(argv[0], argv, NULL));
        break;

    case WLMAKER_ACTION_SHELL_EXECUTE:
        argv = (const char*[]){ "/bin/sh", "-c", arg_ptr, NULL };
        wlmaker_subprocess_monitor_run(
            server_ptr->monitor_ptr,
            bs_subprocess_create(argv[0], argv, NULL));
        break;

    case WLMAKER_ACTION_EXECUTE:
        wlmaker_subprocess_monitor_run(
            server_ptr->monitor_ptr,
            bs_subprocess_create_cmdline(arg_ptr));
        break;

    case WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS:
        wlmtk_root_switch_to_previous_workspace(server_ptr->root_ptr);
        break;

    case WLMAKER_ACTION_WORKSPACE_TO_NEXT:
        wlmtk_root_switch_to_next_workspace(server_ptr->root_ptr);
        break;

    case WLMAKER_ACTION_WORKSPACE_ADD:
        workspace_ptr = wlmtk_workspace_create(
            server_ptr->wlr_output_layout_ptr,
            "New",
            &server_ptr->style.tile);
        if (NULL != workspace_ptr) {
            BS_ASSERT_NOTNULL(wlmaker_background_create(
                                  workspace_ptr,
                                  server_ptr->wlr_output_layout_ptr,
                                  server_ptr->style.background_color));
            wlmtk_root_add_workspace(server_ptr->root_ptr, workspace_ptr);
        }
        break;

    case WLMAKER_ACTION_WORKSPACE_DESTROY_LAST:
        wlmtk_root_destroy_last_workspace(server_ptr->root_ptr);
        break;

    case WLMAKER_ACTION_TASK_TO_PREVIOUS:
        wlmtk_workspace_activate_previous_window(
            wlmtk_root_get_current_workspace(server_ptr->root_ptr));
        wlmaker_server_activate_task_list(server_ptr);
        break;

    case WLMAKER_ACTION_TASK_TO_NEXT:
        wlmtk_workspace_activate_next_window(
            wlmtk_root_get_current_workspace(server_ptr->root_ptr));
        wlmaker_server_activate_task_list(server_ptr);
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
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_fullscreen(
                window_ptr, !wlmtk_window_is_fullscreen(window_ptr));
        }
        break;

    case WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_maximized(
                window_ptr, !wlmtk_window_is_maximized(window_ptr));
        }
        break;

    case WLMAKER_ACTION_WINDOW_MAXIMIZE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_maximized(window_ptr, true);
        }
        break;

    case WLMAKER_ACTION_WINDOW_UNMAXIMIZE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_maximized(window_ptr, false);
        }
        break;

    case WLMAKER_ACTION_WINDOW_FULLSCREEN:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_fullscreen(window_ptr, true);
        }
        break;

    case WLMAKER_ACTION_WINDOW_SHADE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_shaded(window_ptr, true);
        }
        break;

    case WLMAKER_ACTION_WINDOW_UNSHADE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_shaded(window_ptr, false);
        }
        break;

    case WLMAKER_ACTION_WINDOW_TO_NEXT_WORKSPACE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        next_workspace_ptr = wlmtk_workspace_from_dlnode(
            wlmtk_dlnode_from_workspace(workspace_ptr)->next_ptr);
        if (NULL != window_ptr &&
            NULL != next_workspace_ptr) {
            wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
            wlmtk_workspace_map_window(next_workspace_ptr, window_ptr);
        }
        break;

    case WLMAKER_ACTION_WINDOW_TO_PREVIOUS_WORKSPACE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        next_workspace_ptr = wlmtk_workspace_from_dlnode(
            wlmtk_dlnode_from_workspace(workspace_ptr)->prev_ptr);
        if (NULL != window_ptr &&
            NULL != next_workspace_ptr) {
            wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
            wlmtk_workspace_map_window(next_workspace_ptr, window_ptr);
        }
        break;

    case WLMAKER_ACTION_WINDOW_CLOSE:
        workspace_ptr = wlmtk_root_get_current_workspace(
            server_ptr->root_ptr);
        window_ptr = wlmtk_workspace_get_activated_window(workspace_ptr);
        if (NULL != window_ptr) {
            wlmtk_window_request_close(window_ptr);
        }
        break;

    case WLMAKER_ACTION_ROOT_MENU:
        // TODO(kaeser@gubbe.ch): Clean up.
        if (NULL != server_ptr->root_menu_ptr &&
            NULL == wlmtk_window_get_workspace(
                wlmaker_root_menu_window(server_ptr->root_menu_ptr))) {
            wlmtk_workspace_map_window(
                wlmtk_root_get_current_workspace(server_ptr->root_ptr),
                wlmaker_root_menu_window(server_ptr->root_menu_ptr));
            wlmtk_workspace_set_window_position(
                wlmtk_root_get_current_workspace(server_ptr->root_ptr),
                wlmaker_root_menu_window(server_ptr->root_menu_ptr),
                server_ptr->cursor_ptr->wlr_cursor_ptr->x,
                server_ptr->cursor_ptr->wlr_cursor_ptr->y);
            wlmtk_workspace_confine_within(
                wlmtk_root_get_current_workspace(server_ptr->root_ptr),
                wlmaker_root_menu_window(server_ptr->root_menu_ptr));
            wlmtk_menu_set_mode(
                wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
                WLMTK_MENU_MODE_NORMAL);
            wlmtk_menu_set_open(
                wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
                true);
        }
        break;

    case WLMAKER_ACTION_OUTPUT_MAGNIFY:
        wlmbe_backend_magnify(server_ptr->backend_ptr);
        break;
    case WLMAKER_ACTION_OUTPUT_REDUCE:
        wlmbe_backend_reduce(server_ptr->backend_ptr);
        break;

    case WLMAKER_ACTION_OUTPUT_SAVE_STATE:
        wlmbe_backend_save_ephemeral_output_configs(server_ptr->backend_ptr);
        break;

    case WLMAKER_ACTION_SWITCH_TO_VT1:
    case WLMAKER_ACTION_SWITCH_TO_VT2:
    case WLMAKER_ACTION_SWITCH_TO_VT3:
    case WLMAKER_ACTION_SWITCH_TO_VT4:
    case WLMAKER_ACTION_SWITCH_TO_VT5:
    case WLMAKER_ACTION_SWITCH_TO_VT6:
    case WLMAKER_ACTION_SWITCH_TO_VT7:
    case WLMAKER_ACTION_SWITCH_TO_VT8:
    case WLMAKER_ACTION_SWITCH_TO_VT9:
    case WLMAKER_ACTION_SWITCH_TO_VT10:
    case WLMAKER_ACTION_SWITCH_TO_VT11:
    case WLMAKER_ACTION_SWITCH_TO_VT12:
        // Enums are required to be defined consecutively, so we can compute
        // the VT number from the action code.
        wlmbe_backend_switch_to_vt(
            server_ptr->backend_ptr,
            action - WLMAKER_ACTION_SWITCH_TO_VT1 + 1);
        break;

    default:
        bs_log(BS_WARNING, "Unhandled action %d.", action);
        break;
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Binds an action for one item of the 'KeyBindings' dict.
 *
 * @param key_ptr             Name of the action to bind the key to.
 * @param object_ptr          Configuration object, must be a string and
 *                            contain a parse-able modifier + keysym.
 * @param userdata_ptr        Points to @ref wlmaker_server_t.
 *
 * @return true on success.
 */
bool _wlmaker_keybindings_bind_item(
    const char *key_ptr,
    bspl_object_t *object_ptr,
    void *userdata_ptr)
{
    wlmaker_action_handle_t *handle_ptr = userdata_ptr;
    bspl_string_t *string_ptr = bspl_string_from_object(object_ptr);
    if (NULL == string_ptr) {
        bs_log(BS_WARNING, "Action must be a string for key binding \"%s\"",
               key_ptr);
        return false;
    }

    uint32_t modifiers;
    xkb_keysym_t keysym;
    if (!_wlmaker_keybindings_parse(key_ptr, &modifiers, &keysym)) {
        bs_log(BS_WARNING,
               "Failed to parse binding '%s' for keybinding action '%s'",
               key_ptr, bspl_string_value(string_ptr));
        return false;
    }
    if (handle_ptr->add_logo) modifiers |= WLR_MODIFIER_LOGO;

    int action;
    if (!bspl_enum_name_to_value(
            wlmaker_action_desc, bspl_string_value(string_ptr), &action)) {
        bs_log(BS_WARNING, "Not a valid keybinding action: '%s'",
               bspl_string_value(string_ptr));
        return false;
    }

    _wlmaker_action_binding_t *action_binding_ptr = logged_calloc(
        1, sizeof(_wlmaker_action_binding_t));
    if (NULL == action_binding_ptr) return false;
    action_binding_ptr->handle_ptr = handle_ptr;
    action_binding_ptr->action = action;
    action_binding_ptr->key_combo.keysym = keysym;
    action_binding_ptr->key_combo.ignore_case = true;
    action_binding_ptr->key_combo.modifiers = modifiers;
    action_binding_ptr->key_combo.modifiers_mask =
        wlmaker_modifier_default_mask;
    action_binding_ptr->key_binding_ptr = wlmaker_server_bind_key(
        handle_ptr->server_ptr,
        &action_binding_ptr->key_combo,
        _wlmaker_action_bound_callback);
    if (NULL != action_binding_ptr->key_binding_ptr) {
        bs_dequeue_push_back(
            &handle_ptr->bindings, &action_binding_ptr->qnode);
        return true;
    }

    free(action_binding_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Parses a keybinding string: Tokenizes into modifiers and keysym.
 *
 * @param string_ptr
 * @param modifiers_ptr
 * @param keysym_ptr
 *
 * @return true on success.
 */
bool _wlmaker_keybindings_parse(
    const char *string_ptr,
    uint32_t *modifiers_ptr,
    xkb_keysym_t *keysym_ptr)
{
    *keysym_ptr = XKB_KEY_NoSymbol;
    *modifiers_ptr = 0;
    bool rv = true;

    // Tokenize along '+', then lookup each of the keys.
    for (const char *start_ptr = string_ptr; *start_ptr != '\0'; ++start_ptr) {

        const char *end_ptr = start_ptr;
        while (*end_ptr != '\0' && *end_ptr != '+') ++end_ptr;

        size_t len = end_ptr - start_ptr;
        char *token_ptr = malloc(len + 1);
        memcpy(token_ptr, start_ptr, len);
        token_ptr[len] = '\0';

        start_ptr = end_ptr;

        int new_modifier;
        if (bspl_enum_name_to_value(
                _wlmaker_keybindings_modifiers, token_ptr, &new_modifier)) {
            *modifiers_ptr |= new_modifier;
        } else if (*keysym_ptr == XKB_KEY_NoSymbol) {
            *keysym_ptr = xkb_keysym_to_upper(
                xkb_keysym_from_name(token_ptr, XKB_KEYSYM_CASE_INSENSITIVE));
        } else {
            rv = false;
        }

        free(token_ptr);
        if (!*start_ptr) break;
    }

    return rv && (XKB_KEY_NoSymbol != *keysym_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for when the bound key is triggered: Executes the corresponding
 * action.
 *
 * @param key_combo_ptr
 *
 * @return true always.
 */
bool _wlmaker_action_bound_callback(const wlmaker_key_combo_t *key_combo_ptr)
{
    _wlmaker_action_binding_t *action_binding_ptr = BS_CONTAINER_OF(
        key_combo_ptr, _wlmaker_action_binding_t, key_combo);

    wlmaker_action_execute(
        action_binding_ptr->handle_ptr->server_ptr,
        action_binding_ptr->action,
        NULL);
    return true;
}

/* == Unit tests =========================================================== */

static void test_keybindings_parse(bs_test_t *test_ptr);
static void test_default_keybindings(bs_test_t *test_ptr);

/** Test cases for key bindings. */
static const bs_test_case_t   wlmaker_action_test_cases[] = {
    { true, "parse", test_keybindings_parse },
    { true, "default_keybindings", test_default_keybindings },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_action_test_set = BS_TEST_SET(
    true, "action", wlmaker_action_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests @ref _wlmaker_keybindings_parse. */
void test_keybindings_parse(bs_test_t *test_ptr)
{
    uint32_t m;
    xkb_keysym_t ks;

    // Lower- and upper case.
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmaker_keybindings_parse("A", &m, &ks));
    BS_TEST_VERIFY_EQ(test_ptr, 0, m);
    BS_TEST_VERIFY_EQ(test_ptr, XKB_KEY_A, ks);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmaker_keybindings_parse("a", &m, &ks));
    BS_TEST_VERIFY_EQ(test_ptr, 0, m);
    BS_TEST_VERIFY_EQ(test_ptr, XKB_KEY_A, ks);

    // Modifier.
    BS_TEST_VERIFY_TRUE(
        test_ptr, _wlmaker_keybindings_parse("Ctrl+Logo+Q", &m, &ks));
    BS_TEST_VERIFY_EQ(test_ptr, WLR_MODIFIER_CTRL | WLR_MODIFIER_LOGO, m);
    BS_TEST_VERIFY_EQ(test_ptr, XKB_KEY_Q, ks);

    // Test some fancier keys.
    BS_TEST_VERIFY_TRUE(
        test_ptr, _wlmaker_keybindings_parse("Escape", &m, &ks));
    BS_TEST_VERIFY_EQ(test_ptr, XKB_KEY_Escape, ks);
    BS_TEST_VERIFY_TRUE(
        test_ptr, _wlmaker_keybindings_parse("XF86AudioLowerVolume", &m, &ks));
    BS_TEST_VERIFY_EQ(test_ptr, XKB_KEY_XF86AudioLowerVolume, ks);

    // Not peritted: Empty, just modifiers, more than one keysym.
    BS_TEST_VERIFY_FALSE(
        test_ptr, _wlmaker_keybindings_parse("", &m, &ks));
    BS_TEST_VERIFY_FALSE(
        test_ptr, _wlmaker_keybindings_parse("A+B", &m, &ks));
    BS_TEST_VERIFY_FALSE(
        test_ptr, _wlmaker_keybindings_parse("Shift+Ctrl", &m, &ks));
}

/* ------------------------------------------------------------------------- */
/** Tests the default configuration's 'KeyBindings' section. */
void test_default_keybindings(bs_test_t *test_ptr)
{
    wlmaker_server_t server = {};
    bspl_object_t *obj_ptr = bspl_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, bspl_dict_from_object(obj_ptr));

    bspl_dict_t *dict_ptr = bspl_dict_get_dict(
        bspl_dict_from_object(obj_ptr), wlmaker_action_config_dict_key);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);

    wlmaker_action_handle_t *handle_ptr = wlmaker_action_bind_keys(
        &server, dict_ptr, false);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, handle_ptr);
    bspl_object_unref(obj_ptr);
    wlmaker_action_unbind_keys(handle_ptr);
}

/* == End of action.c ====================================================== */
