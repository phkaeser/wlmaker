/* ========================================================================= */
/**
 * @file keyboard.c
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

#include "keyboard.h"

#include <ctype.h>
#include <ini.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#undef WLR_USE_UNSTABLE
#include <xkbcommon/xkbcommon.h>

#include "idle.h"
#include "server.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Keyboard handle. */
struct _wlmaker_keyboard_t {
    /** Configuration dictionnary, just the "Keyboard" section. */
    bspl_dict_t             *config_dict_ptr;
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
    /** The wlroots keyboard structure. */
    struct wlr_keyboard       *wlr_keyboard_ptr;
    /** The wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;

    /** Listener for the `modifiers` signal of `wl_keyboard`. */
    struct wl_listener        modifiers_listener;
    /** Listener for the `key` signal of `wl_keyboard`. */
    struct wl_listener        key_listener;
};

static bspl_dict_t *_wlmaker_keyboard_populate_rules(
    bspl_dict_t *dict_ptr,
    struct xkb_rule_names *rules_ptr);
static bool _wlmaker_keyboard_populate_repeat(
    bspl_dict_t *dict_ptr,
    int32_t *rate_ptr,
    int32_t *delay_ptr);
static int _wlmaker_keyboard_config_ini_handler(
    void *user_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr);

static void handle_key(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_modifiers(struct wl_listener *listener_ptr,
                             void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_keyboard_t *wlmaker_keyboard_create(
    wlmaker_server_t *server_ptr,
    struct wlr_keyboard *wlr_keyboard_ptr,
    struct wlr_seat *wlr_seat_ptr)
{
    wlmaker_keyboard_t *keyboard_ptr = logged_calloc(
        1, sizeof(wlmaker_keyboard_t));
    if (NULL == keyboard_ptr) return NULL;
    keyboard_ptr->server_ptr = server_ptr;
    keyboard_ptr->wlr_keyboard_ptr = wlr_keyboard_ptr;
    keyboard_ptr->wlr_seat_ptr = wlr_seat_ptr;

    // Retrieve configuration.
    bspl_dict_t *config_dict_ptr = bspl_dict_get_dict(
        server_ptr->config_dict_ptr, "Keyboard");
    if (NULL == config_dict_ptr) {
        bs_log(BS_ERROR, "Failed to retrieve \"Keyboard\" dict from config.");
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }
    keyboard_ptr->config_dict_ptr = BS_ASSERT_NOTNULL(
        bspl_dict_ref(config_dict_ptr));

    struct xkb_rule_names xkb_rule;
    bspl_dict_t *rmlvo_dict_ptr = _wlmaker_keyboard_populate_rules(
        keyboard_ptr->config_dict_ptr, &xkb_rule);
    if (NULL == rmlvo_dict_ptr) {
        bs_log(BS_ERROR, "No rule data found in 'Keyboard' dict.");
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }

    // Set keyboard layout.
    struct xkb_context *xkb_context_ptr = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *xkb_keymap_ptr = xkb_keymap_new_from_names(
        xkb_context_ptr, &xkb_rule, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (NULL == xkb_keymap_ptr) {
        bs_log(BS_ERROR, "Failed xkb_keymap_new_from_names(%p, { .rules = %s, "
               ".model = %s, .layout = %s, variant = %s, .options = %s }, "
               "XKB_KEYMAP_COMPILE_NO_NO_FLAGS)",
               xkb_context_ptr,
               xkb_rule.rules,
               xkb_rule.model,
               xkb_rule.layout,
               xkb_rule.variant,
               xkb_rule.options);
        bspl_dict_unref(rmlvo_dict_ptr);
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }
    bspl_dict_unref(rmlvo_dict_ptr);
    wlr_keyboard_set_keymap(keyboard_ptr->wlr_keyboard_ptr, xkb_keymap_ptr);
    xkb_keymap_unref(xkb_keymap_ptr);
    xkb_context_unref(xkb_context_ptr);

    // Repeat rate and delay.
    int32_t rate, delay;
    if (!_wlmaker_keyboard_populate_repeat(
            keyboard_ptr->config_dict_ptr, &rate, &delay)) {
        bs_log(BS_ERROR, "No repeat data found in 'Keyboard' dict.");
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }
    wlr_keyboard_set_repeat_info(keyboard_ptr->wlr_keyboard_ptr, rate, delay);

    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.key,
        &keyboard_ptr->key_listener,
        handle_key);
    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.modifiers,
        &keyboard_ptr->modifiers_listener,
        handle_modifiers);

    // Set (or restore) keyboard layout group in XKB state, and update modifiers.
    xkb_state_update_mask(
        keyboard_ptr->wlr_keyboard_ptr->xkb_state,
        0,  // depressed_mods
        0,  // latched_mods
        0,  // locked_mods
        0,  // depressed_layout
        0,  // latched_layout
        server_ptr->last_keyboard_group_index);  // locked_layout.
    wlr_keyboard_ptr->modifiers.group = server_ptr->last_keyboard_group_index;
    wlr_seat_keyboard_notify_modifiers(
        wlr_seat_ptr,
        &wlr_keyboard_ptr->modifiers);
    // Also, re-trigger client's XKB state machine by an explicit "Enter".
    if (NULL != wlr_seat_ptr->keyboard_state.focused_surface) {
        wlr_seat_keyboard_enter(
            wlr_seat_ptr,
            wlr_seat_ptr->keyboard_state.focused_surface,
            wlr_keyboard_ptr->keycodes,
            wlr_keyboard_ptr->num_keycodes,
            &wlr_keyboard_ptr->modifiers);
    }

    wlr_seat_set_keyboard(wlr_seat_ptr, keyboard_ptr->wlr_keyboard_ptr);
    return keyboard_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_keyboard_destroy(wlmaker_keyboard_t *keyboard_ptr)
{
    wl_list_remove(&keyboard_ptr->key_listener.link);
    wl_list_remove(&keyboard_ptr->modifiers_listener.link);

    if (NULL != keyboard_ptr->config_dict_ptr) {
        bspl_dict_unref(keyboard_ptr->config_dict_ptr);
        keyboard_ptr->config_dict_ptr = NULL;
    }

    free(keyboard_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Populates the XKB rules struct from the config dict.
 *
 * @param dict_ptr
 * @param rules_ptr
 *
 * @return A referenced bspl_dict_t holding rules details, or NULL on error.
 */
bspl_dict_t *_wlmaker_keyboard_populate_rules(
    bspl_dict_t *dict_ptr,
    struct xkb_rule_names *rules_ptr)
{
    bspl_dict_t *rmlvo = NULL;

    const char *fname_ptr = bspl_dict_get_string_value(
        dict_ptr, "XkbConfigurationFile");
    if (NULL != fname_ptr) {
        rmlvo = bspl_dict_create();
        if (0 != ini_parse(
                fname_ptr,
                _wlmaker_keyboard_config_ini_handler,
                rmlvo)) {
            bs_log(BS_ERROR, "Failed to parse \"XkbConfigurationFile\" at %s",
                   fname_ptr);
            bspl_dict_unref(rmlvo);
            return NULL;
        }
    } else {
        rmlvo = bspl_dict_ref(bspl_dict_get_dict(dict_ptr, "XkbRMLVO"));
    }

    if (NULL == rmlvo) {
        bs_log(BS_ERROR, "No \"XkbConfigurationFile\" nor \"XkbRMLVO\" dict "
               "found in \"Keyboard\" dict.");
        return NULL;
    }

    rules_ptr->rules = bspl_dict_get_string_value(rmlvo, "Rules");
    rules_ptr->model = bspl_dict_get_string_value(rmlvo, "Model");
    rules_ptr->layout = bspl_dict_get_string_value(rmlvo, "Layout");
    rules_ptr->variant = bspl_dict_get_string_value(rmlvo, "Variant");
    rules_ptr->options = bspl_dict_get_string_value(rmlvo, "Options");
    return rmlvo;
}

/* ------------------------------------------------------------------------- */
/**
 * Retrieves and converts the 'Repeat' parameters from the config dict.
 *
 * @param dict_ptr
 * @param rate_ptr
 * @param delay_ptr
 *
 * @return true on success.
 */
bool _wlmaker_keyboard_populate_repeat(
    bspl_dict_t *dict_ptr,
    int32_t *rate_ptr,
    int32_t *delay_ptr)
{
    dict_ptr = bspl_dict_get_dict(dict_ptr, "Repeat");
    if (NULL == dict_ptr) {
        bs_log(BS_ERROR, "No 'Repeat' dict in 'Keyboard' dict.");
        return false;
    }

    uint64_t value;
    if (!bs_strconvert_uint64(
            bspl_dict_get_string_value(dict_ptr, "Delay"),
            &value, 10) ||
        value > INT32_MAX) {
        bs_log(BS_ERROR, "Invalid value for 'Delay': %s",
               bspl_dict_get_string_value(dict_ptr, "Delay"));
        return false;
    }
    *delay_ptr = value;

    if (!bs_strconvert_uint64(
            bspl_dict_get_string_value(dict_ptr, "Rate"),
            &value, 10) ||
        value > INT32_MAX) {
        bs_log(BS_ERROR, "Invalid value for 'Rate': %s",
               bspl_dict_get_string_value(dict_ptr, "Rate"));
        return false;
    }
    *rate_ptr = value;

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * inih parser callback. Fills XKB config file values into the dict.
 *
 * @param user_ptr            Points to a `bspl_dict_t`.
 * @param section_ptr
 * @param name_ptr
 * @param value_ptr
 *
 * @return 0 on success.
 */
int _wlmaker_keyboard_config_ini_handler(
    void *user_ptr,
    __UNUSED__ const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr)
{
    bspl_dict_t *rmlvo_dict_ptr = user_ptr;

    struct field_definition { const char *n; const char *k; };
    struct field_definition definitions[] = {
        { .n = "XKBMODEL", .k = "Model" },
        { .n = "XKBLAYOUT", .k = "Layout" },
        { .n = "XKBVARIANT", .k = "Variant" },
        { .n = "XKBOPTIONS", .k = "Options" },
        { .n = "BACKSPACE", .k = NULL },
        { .n = NULL }
    };

    for (const struct field_definition *def_ptr = &definitions[0];
         NULL != def_ptr->n;
         ++def_ptr) {
        if (0 != strcmp(def_ptr->n, name_ptr)) continue;
        if (NULL == def_ptr->k) return 1;

        // Trim from left, and remove double-quote.
        while ('\0' != *value_ptr && isblank(*value_ptr)) ++value_ptr;
        if ('"' == *value_ptr) ++value_ptr;

        // Trim from right, and also remove double-quote.
        size_t l = strlen(value_ptr);
        while (0 < l && isblank(value_ptr[l-1])) --l;
        if (0 < l && '"' == value_ptr[l-1]) --l;

        char *copied = logged_malloc(l+1);
        if (NULL == copied) return 1;
        memcpy(copied, value_ptr, l);
        copied[l] = '\0';
        bspl_string_t *s = bspl_string_create(copied);
        free(copied);
        if (NULL == s) return 0;

        bspl_dict_add(rmlvo_dict_ptr, def_ptr->k, bspl_object_from_string(s));
        bspl_string_unref(s);
        return 1;
    }

    bs_log(BS_WARNING, "Unknown name: \"%s\"", name_ptr);
    return 1;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles `key` signals, ie. key presses.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a `wlr_keyboard_key_event`.
 */
void handle_key(struct wl_listener *listener_ptr, void *data_ptr)
{
    wlmaker_keyboard_t *keyboard_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_keyboard_t, key_listener);
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr = data_ptr;

    wlmaker_idle_monitor_reset(keyboard_ptr->server_ptr->idle_monitor_ptr);

    // TODO(kaeser@gubbe.ch): Omit consumed modifiers, see xkbcommon.h.
    uint32_t modifiers = wlr_keyboard_get_modifiers(
        keyboard_ptr->wlr_keyboard_ptr);

    // TODO(kaeser@gubbe.ch): Handle this better -- should respect the
    // modifiers of the task list actions, and be more generalized.
    if ((modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT &&
        keyboard_ptr->server_ptr->task_list_enabled) {
        wlmaker_server_deactivate_task_list(keyboard_ptr->server_ptr);
    }

    // Translates libinput keycode -> xkbcommon.
    uint32_t keycode = wlr_keyboard_key_event_ptr->keycode + 8;

    // For key presses: Pass them on to the server, for potential key bindings.
    bool processed = false;
    const xkb_keysym_t *key_syms;
    int key_syms_count = xkb_state_key_get_syms(
        keyboard_ptr->wlr_keyboard_ptr->xkb_state, keycode, &key_syms);
    for (int i = 0; i < key_syms_count; ++i) {
        enum xkb_key_direction direction =
            wlr_keyboard_key_event_ptr->state == WL_KEYBOARD_KEY_STATE_RELEASED ?
            XKB_KEY_UP : XKB_KEY_DOWN;
        xkb_state_update_key(
            keyboard_ptr->wlr_keyboard_ptr->xkb_state,
            keycode,
            direction);

        if (WL_KEYBOARD_KEY_STATE_PRESSED == wlr_keyboard_key_event_ptr->state &&
            wlmaker_keyboard_process_bindings(
                keyboard_ptr->server_ptr, key_syms[i], modifiers)) {
            processed |= true;
        } else {
            processed |= wlmtk_element_keyboard_sym(
                wlmtk_root_element(keyboard_ptr->server_ptr->root_ptr),
                key_syms[i],
                direction,
                modifiers);
        }
    }

    if (processed) return;

    wlmtk_element_keyboard_event(
        wlmtk_root_element(keyboard_ptr->server_ptr->root_ptr),
        wlr_keyboard_key_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles `modifiers` signals, ie. updates to the modifiers.
 *
 * @param listener_ptr
 * @param data_ptr            Points to wlr_keyboard.
 */
void handle_modifiers(struct wl_listener *listener_ptr,
                      __UNUSED__ void *data_ptr)
{
    wlmaker_keyboard_t *keyboard_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_keyboard_t, modifiers_listener);

    wlmaker_idle_monitor_reset(keyboard_ptr->server_ptr->idle_monitor_ptr);

    keyboard_ptr->server_ptr->last_keyboard_group_index =
        xkb_state_serialize_layout(
            keyboard_ptr->wlr_keyboard_ptr->xkb_state,
            XKB_STATE_LAYOUT_EFFECTIVE);

    uint32_t modifiers = wlr_keyboard_get_modifiers(
        keyboard_ptr->wlr_keyboard_ptr);

    if ((modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT) {
        wlmaker_server_deactivate_task_list(keyboard_ptr->server_ptr);
    }

    wlr_seat_set_keyboard(
        keyboard_ptr->wlr_seat_ptr,
        keyboard_ptr->wlr_keyboard_ptr);
    wlr_seat_keyboard_notify_modifiers(
        keyboard_ptr->wlr_seat_ptr,
        &keyboard_ptr->wlr_keyboard_ptr->modifiers);
}

/* == Unit tests =========================================================== */

static void _wlmaker_keyboard_test_rmlvo(bs_test_t *test_ptr);
static void _wlmaker_keyboard_test_keyboard_file(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmaker_keyboard_test_cases[] = {
    { true, "rmlvo", _wlmaker_keyboard_test_rmlvo },
    { true, "keyboard_file", _wlmaker_keyboard_test_keyboard_file },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_keyboard_test_set = BS_TEST_SET(
    true, "keyboard", wlmaker_keyboard_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests keyboard rules are loaded from a given RMLVO dict. */
void _wlmaker_keyboard_test_rmlvo(bs_test_t *test_ptr)
{
    struct xkb_rule_names r = {};
    bspl_dict_t *d = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{XkbRMLVO={Rules=R;Model=M;Layout=L;Variant=V;Options=O}}"));

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, d);
    bspl_dict_t *rmlvo = _wlmaker_keyboard_populate_rules(d, &r);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, rmlvo);
    BS_TEST_VERIFY_STREQ(test_ptr, "R", r.rules);
    BS_TEST_VERIFY_STREQ(test_ptr, "M", r.model);
    BS_TEST_VERIFY_STREQ(test_ptr, "L", r.layout);
    BS_TEST_VERIFY_STREQ(test_ptr, "V", r.variant);
    BS_TEST_VERIFY_STREQ(test_ptr, "O", r.options);
    bspl_dict_unref(rmlvo);
    bspl_dict_unref(d);
}

/* ------------------------------------------------------------------------- */
/** Tests keyboard rules are loaded from XKB configuration file. */
void _wlmaker_keyboard_test_keyboard_file(bs_test_t *test_ptr)
{
    struct xkb_rule_names r = {};
    bspl_dict_t *d = bspl_dict_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, d);

    bspl_object_t *o = bspl_object_from_string(
        bspl_string_create(bs_test_data_path(test_ptr, "keyboard")));
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        bspl_dict_add(d, "XkbConfigurationFile", o));
    bspl_object_unref(o);

    bspl_dict_t *rmlvo = _wlmaker_keyboard_populate_rules(d, &r);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, rmlvo);

    BS_TEST_VERIFY_STREQ(test_ptr, "pc105", r.model);
    BS_TEST_VERIFY_STREQ(test_ptr, "us,ch", r.layout);
    BS_TEST_VERIFY_STREQ(test_ptr, "intl,", r.variant);
    BS_TEST_VERIFY_STREQ(test_ptr, "grp:shift_caps_toggle", r.options);

    bspl_dict_unref(rmlvo);
    bspl_dict_unref(d);
}

/* == End of keyboard.c ==================================================== */
