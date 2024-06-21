/* ========================================================================= */
/**
 * @file keybindings.c
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

#include "keybindings.h"

#include "server.h"
#include "conf/decode.h"

#include <xkbcommon/xkbcommon.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE

static bool _wlmaker_keybindings_parse(
    const char *string_ptr,
    uint32_t *modifiers_ptr,
    xkb_keysym_t *keysym_ptr);

/* == Declarations ========================================================= */

/** A name/action binding. */
typedef struct {
    /** The name. */
    const char                *name_ptr;
    /** The action. */
    void                      (*action)(wlmaker_server_t* server_ptr);
} _wlmaker_keybindings_action_t;

static void _task_list_next(wlmaker_server_t *server_ptr);
static void _task_list_previous(wlmaker_server_t *server_ptr);

/* == Exported methods ===================================================== */

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
static const _wlmaker_keybindings_action_t _wlmaker_keybindings_actions[] = {
    { "TaskListNext", _task_list_next },
    { "TaskListPrevious", _task_list_previous },
    { NULL, NULL }
};

/* == Local (static) methods =============================================== */

bool _wlmaker_bind_keys(
    __UNUSED__ wlmaker_server_t *server_ptr,
    __UNUSED__ wlmcfg_dict_t *keybindings_dict_ptr)
{
    // Iterate over each of the keys.

    const _wlmaker_keybindings_action_t *action_ptr;
    for (action_ptr = _wlmaker_keybindings_actions;
         action_ptr->name_ptr != NULL;
         ++action_ptr) {
    }
    return true;
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

        size_t len = end_ptr - start_ptr + 1;
        char *token_ptr = malloc(len + 1);
        memcpy(token_ptr, start_ptr, len);
        token_ptr[len - 1] = '\0';

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
    }

    return rv && (XKB_KEY_NoSymbol != *keysym_ptr);
}

/* ------------------------------------------------------------------------- */
/** TOOD(kaeser@gubbe.ch): Move to better place. */
void _task_list_next(wlmaker_server_t *server_ptr)
{
    bs_log(BS_WARNING, "FIXME: Next for %p", server_ptr);
}

/* ------------------------------------------------------------------------- */
/** TOOD(kaeser@gubbe.ch): Move to better place. */
void _task_list_previous(wlmaker_server_t *server_ptr)
{
    bs_log(BS_WARNING, "FIXME: Previous for %p", server_ptr);
}

/* == Unit tests =========================================================== */

static void test_keybindings_parse(bs_test_t *test_ptr);

/** Test cases for key bindings. */
const bs_test_case_t          wlmaker_keybindings_test_cases[] = {
    { 1, "parse", test_keybindings_parse },
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

/* == End of keybindings.c ================================================= */
