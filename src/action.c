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

#include "default_configuration.h"
#include "server.h"
#include "conf/decode.h"
#include "conf/plist.h"

#include <xkbcommon/xkbcommon.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the bound actions. */
struct _wlmaker_action_handle_t {
    /** Bindings, linked by @ref wlmaker_action_binding_t::qnode. */
    bs_dequeue_t              bindings;
    /** Back-link to server state. */
    wlmaker_server_t          *server_ptr;
};

/** Key binding for a standard action. */
typedef struct {
    /** Node of @ref wlmaker_action_handle_t::bindings. */
    bs_dequeue_node_t         qnode;
    /** The key binding. */
    wlmaker_keybinding_t      binding;
    /** The associated action. */
    wlmaker_action_t          action;
    /** State of the bound actions. */
    wlmaker_action_handle_t   *handle_ptr;
} _wlmaker_action_binding_t;

static bool _wlmaker_keybindings_parse(
    const char *string_ptr,
    uint32_t *modifiers_ptr,
    xkb_keysym_t *keysym_ptr);

static bool _wlmaker_keybindings_bind_item(
    const char *key_ptr,
    wlmcfg_object_t *object_ptr,
    void *userdata_ptr);

static bool _wlmaker_action_bound_callback(
    const wlmaker_keybinding_t *binding_ptr);

/* == Data ================================================================= */

/** Supported modifiers for key bindings. */
static const wlmcfg_enum_desc_t _wlmaker_keybindings_modifiers[] = {
    WLMCFG_ENUM("Shift", WLR_MODIFIER_SHIFT),
    // Caps? Maybe not: WLMCFG_ENUM("Caps", WLR_MODIFIER_CAPS),
    WLMCFG_ENUM("Ctrl", WLR_MODIFIER_CTRL),
    WLMCFG_ENUM("Alt", WLR_MODIFIER_ALT),
    WLMCFG_ENUM("Mod2", WLR_MODIFIER_MOD2),
    WLMCFG_ENUM("Mod3", WLR_MODIFIER_MOD3),
    WLMCFG_ENUM("Logo", WLR_MODIFIER_LOGO),
    WLMCFG_ENUM("Mod5", WLR_MODIFIER_MOD5),
    WLMCFG_ENUM_SENTINEL(),
};

/** The actions that can be bound. */
static const wlmcfg_enum_desc_t wlmaker_action_desc[] = {
    WLMCFG_ENUM("Quit", WLMAKER_ACTION_QUIT),
    WLMCFG_ENUM("LockScreen", WLMAKER_ACTION_LOCK_SCREEN),
    WLMCFG_ENUM("LaunchTerminal", WLMAKER_ACTION_LAUNCH_TERMINAL),

    WLMCFG_ENUM("WorkspacePrevious", WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS),
    WLMCFG_ENUM("WorkspaceNext", WLMAKER_ACTION_WORKSPACE_TO_NEXT),

    WLMCFG_ENUM("WindowRaise", WLMAKER_ACTION_WINDOW_RAISE),
    WLMCFG_ENUM("WindowLower", WLMAKER_ACTION_WINDOW_LOWER),
    WLMCFG_ENUM("WindowFullscreen", WLMAKER_ACTION_WINDOW_TOGGLE_FULLSCREEN),
    WLMCFG_ENUM("WindowMaximize", WLMAKER_ACTION_WINDOW_TOGGLE_MAXIMIZED),

    WLMCFG_ENUM_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_action_handle_t *wlmaker_action_bind_keys(
    wlmaker_server_t *server_ptr,
    wlmcfg_dict_t *keybindings_dict_ptr)
{
    wlmaker_action_handle_t *handle_ptr = logged_calloc(
        1, sizeof(wlmaker_action_handle_t));
    if (NULL == handle_ptr) return NULL;
    handle_ptr->server_ptr = server_ptr;

    if (wlmcfg_dict_foreach(
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
        wlmaker_server_keyboard_release(
            handle_ptr->server_ptr,
            &binding_ptr->binding);
        free(binding_ptr);
    }

    free(handle_ptr);
}

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

    case WLMAKER_ACTION_WORKSPACE_TO_PREVIOUS:
        wlmaker_server_switch_to_previous_workspace(server_ptr);
        break;

    case WLMAKER_ACTION_WORKSPACE_TO_NEXT:
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
    wlmcfg_object_t *object_ptr,
    void *userdata_ptr)
{
    wlmaker_action_handle_t *handle_ptr = userdata_ptr;
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(object_ptr);
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
               key_ptr, wlmcfg_string_value(string_ptr));
        return false;
    }

    int action;
    if (!wlmcfg_enum_name_to_value(
            wlmaker_action_desc, wlmcfg_string_value(string_ptr), &action)) {
        bs_log(BS_WARNING, "Not a valid keybinding action: '%s'",
               wlmcfg_string_value(string_ptr));
        return false;
    }

    _wlmaker_action_binding_t *action_binding_ptr = logged_calloc(
        1, sizeof(_wlmaker_action_binding_t));
    if (NULL == action_binding_ptr) return false;
    action_binding_ptr->handle_ptr = handle_ptr;
    action_binding_ptr->action = action;
    action_binding_ptr->binding.keysym = keysym;
    action_binding_ptr->binding.ignore_case = true;
    action_binding_ptr->binding.modifiers = modifiers;
    action_binding_ptr->binding.modifiers_mask = 0;  // FIXME
    bool rv = wlmaker_server_keyboard_bind(
        handle_ptr->server_ptr,
        &action_binding_ptr->binding,
        _wlmaker_action_bound_callback);
    if (rv) {
        bs_dequeue_push_back(
            &handle_ptr->bindings, &action_binding_ptr->qnode);
    } else {
        free(action_binding_ptr);
    }
    return rv;
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
        if (wlmcfg_enum_name_to_value(
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
 * @param binding_ptr
 *
 * @return true always.
 */
bool _wlmaker_action_bound_callback(
    const wlmaker_keybinding_t *binding_ptr)
{
    _wlmaker_action_binding_t *action_binding_ptr = BS_CONTAINER_OF(
        binding_ptr, _wlmaker_action_binding_t, binding);

    wlmaker_action_execute(
        action_binding_ptr->handle_ptr->server_ptr,
        action_binding_ptr->action);
    return true;
}

/* == Unit tests =========================================================== */

static void test_keybindings_parse(bs_test_t *test_ptr);
static void test_default_keybindings(bs_test_t *test_ptr);

/** Test cases for key bindings. */
const bs_test_case_t          wlmaker_action_test_cases[] = {
    { 1, "parse", test_keybindings_parse },
    { 1, "default_keybindings", test_default_keybindings },
    { 0, NULL, NULL }
};

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
    BS_TEST_VERIFY_EQ(test_ptr, WLR_MODIFIER_CTRL |  WLR_MODIFIER_LOGO, m);
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
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_get_dict(
        wlmcfg_dict_from_object(obj_ptr), "KeyBindings");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);

    wlmaker_action_handle_t *handle_ptr = wlmaker_action_bind_keys(
        &server, dict_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, handle_ptr);
    wlmcfg_object_unref(obj_ptr);
    wlmaker_action_unbind_keys(handle_ptr);
}

/* == End of action.c ====================================================== */
