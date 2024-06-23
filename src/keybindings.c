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

#include "default_configuration.h"
#include "server.h"
#include "conf/decode.h"
#include "conf/plist.h"

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
static const wlmcfg_enum_desc_t _wlmaker_keybindings_actions[] = {
    WLMCFG_ENUM("TaskListNext", 1 ),
    WLMCFG_ENUM("TaskListPrevious", 2),
    WLMCFG_ENUM_SENTINEL(),
};

static bool _wlmaker_keybindings_bind_item(
    const char *key_ptr,
    wlmcfg_object_t *object_ptr,
    void *userdata_ptr);

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
    __UNUSED__ void *userdata_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(object_ptr);
    if (NULL == string_ptr) {
        bs_log(BS_WARNING, "Not a string value for keybinding action '%s'",
               key_ptr);
        return false;
    }

    uint32_t modifiers;
    xkb_keysym_t keysym;
    if (!_wlmaker_keybindings_parse(
            wlmcfg_string_value(string_ptr), &modifiers, &keysym)) {
        bs_log(BS_WARNING,
               "Failed to parse binding '%s' for keybinding action '%s'",
               wlmcfg_string_value(string_ptr), key_ptr);
        return false;
    }

    int action;
    if (!wlmcfg_enum_name_to_value(
            _wlmaker_keybindings_actions, key_ptr, &action)) {
        bs_log(BS_WARNING, "Not a valid keybinding action: '%s'", key_ptr);
        return false;
    }

    // FIXME: Lookup the action code & register with server_ptr.
    char buf[10];
    xkb_keysym_get_name(keysym, buf, sizeof(buf));
    bs_log(BS_WARNING, "FIXME: Action %d for %s to %"PRIx32" and %s",
           action, key_ptr, modifiers, buf);
    return true;
}

/* ------------------------------------------------------------------------- */
/** FIXME */
bool _wlmaker_bind_keys(
    __UNUSED__ wlmaker_server_t *server_ptr,
    wlmcfg_dict_t *keybindings_dict_ptr)
{
    if (!wlmcfg_dict_foreach(
            keybindings_dict_ptr,
            _wlmaker_keybindings_bind_item,
            server_ptr)) return false;

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

/* == Unit tests =========================================================== */

static void test_keybindings_parse(bs_test_t *test_ptr);
static void test_default_keybindings(bs_test_t *test_ptr);

/** Test cases for key bindings. */
const bs_test_case_t          wlmaker_keybindings_test_cases[] = {
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
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_data(
        embedded_binary_default_configuration_data,
        embedded_binary_default_configuration_size);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, wlmcfg_dict_from_object(obj_ptr));

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_get_dict(
        wlmcfg_dict_from_object(obj_ptr), "KeyBindings");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, _wlmaker_bind_keys(NULL, dict_ptr));
    wlmcfg_object_unref(obj_ptr);
}

/* == End of keybindings.c ================================================= */
