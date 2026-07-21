/* ========================================================================= */
/**
 * @file keyboard.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE
#include <xkbcommon/xkbcommon.h>

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Keyboard handle. */
struct _wlmim_keyboard_t {
    /** Listener for the `modifiers` signal of `wl_keyboard`. */
    struct wl_listener        modifiers_listener;
    /** Listener for the `key` signal of `wl_keyboard`. */
    struct wl_listener        key_listener;

    /** Back-link to the input manager. */
    wlmim_t                   *input_manager_ptr;
    /** The wlroots keyboard structure. */
    struct wlr_keyboard       *wlr_keyboard_ptr;
    /** The wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** desktop element, for forwarding events. */
    wlmtk_element_t           *root_element_ptr;

    /** Points to the keymap, or NULL if not configured. */
    struct xkb_keymap         *xkb_keymap_ptr;
};

static bspl_dict_t *_wlmim_keyboard_populate_rules(
    bspl_dict_t *dict_ptr,
    struct xkb_rule_names *rules_ptr);
static int _wlmim_keyboard_config_ini_handler(
    void *user_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr);


static void handle_key(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_modifiers(struct wl_listener *listener_ptr,
                             void *data_ptr);

/* == Data ================================================================= */

const bspl_enum_desc_t wlmim_keyboard_modifiers[] = {
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

/** Plist descriptor for the "Repat" dict within the "Keyboard" dict. */
static const bspl_desc_t _wlmim_keyboard_repeat_options_desc[] = {
    BSPL_DESC_UINT64(
        "Delay", true, struct wlmim_keyboard_options,
        repeat.delay, repeat.delay, 300),
    BSPL_DESC_UINT64(
        "Rate", true, struct wlmim_keyboard_options,
        repeat.rate, repeat.rate, 25),
    BSPL_DESC_SENTINEL()
};

const bspl_desc_t wlmim_keyboard_options_desc[] = {
    BSPL_DESC_DICT("Repeat", false,
                   struct wlmim_keyboard_options, repeat, repeat,
                   _wlmim_keyboard_repeat_options_desc),
    BSPL_DESC_SENTINEL()
 };


/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmim_keyboard_t *wlmim_keyboard_create(
    wlmim_t *input_manager_ptr,
    struct xkb_keymap *xkb_keymap_ptr,
    const struct wlmim_keyboard_options *options_ptr,
    struct wlr_keyboard *wlr_keyboard_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_element_t *root_element_ptr)
{
    wlmim_keyboard_t *keyboard_ptr = logged_calloc(
        1, sizeof(wlmim_keyboard_t));
    if (NULL == keyboard_ptr) return NULL;
    keyboard_ptr->input_manager_ptr = input_manager_ptr;
    keyboard_ptr->wlr_keyboard_ptr = wlr_keyboard_ptr;
    keyboard_ptr->wlr_seat_ptr = wlr_seat_ptr;
    keyboard_ptr->root_element_ptr = root_element_ptr;

    wlmim_keyboard_configure(keyboard_ptr, xkb_keymap_ptr, options_ptr);

    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.key,
        &keyboard_ptr->key_listener,
        handle_key);
    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.modifiers,
        &keyboard_ptr->modifiers_listener,
        handle_modifiers);

    if (NULL != keyboard_ptr->xkb_keymap_ptr) {
        // Set (or restore) keyboard layout group in XKB state and update
        // modifiers. This won't work if no keymap is given.
        xkb_state_update_mask(
            keyboard_ptr->wlr_keyboard_ptr->xkb_state,
            0,  // depressed_mods
            0,  // latched_mods
            0,  // locked_mods
            0,  // depressed_layout
            0,  // latched_layout
            // locked_layout
            wlmim_get_keyboard_group_index(keyboard_ptr->input_manager_ptr));
        wlr_keyboard_ptr->modifiers.group = wlmim_get_keyboard_group_index(
            keyboard_ptr->input_manager_ptr);
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
    }

    wlr_seat_set_keyboard(wlr_seat_ptr, keyboard_ptr->wlr_keyboard_ptr);
    return keyboard_ptr;
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
int _wlmim_keyboard_config_ini_handler(
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
void wlmim_keyboard_destroy(wlmim_keyboard_t *keyboard_ptr)
{
    wlmtk_util_disconnect_listener(&keyboard_ptr->key_listener);
    wlmtk_util_disconnect_listener(&keyboard_ptr->modifiers_listener);

    if (NULL != keyboard_ptr->xkb_keymap_ptr) {
        xkb_keymap_unref(keyboard_ptr->xkb_keymap_ptr);
        keyboard_ptr->xkb_keymap_ptr = NULL;
    }

    free(keyboard_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmim_keyboard_configure(
    wlmim_keyboard_t *keyboard_ptr,
    struct xkb_keymap *xkb_keymap_ptr,
    const struct wlmim_keyboard_options *options_ptr)
{
    if (keyboard_ptr->xkb_keymap_ptr != xkb_keymap_ptr) {
        if (NULL != keyboard_ptr->xkb_keymap_ptr) {
            xkb_keymap_unref(keyboard_ptr->xkb_keymap_ptr);
        }
        keyboard_ptr->xkb_keymap_ptr = xkb_keymap_ptr;
        xkb_keymap_ref(keyboard_ptr->xkb_keymap_ptr);
        wlr_keyboard_set_keymap(
            keyboard_ptr->wlr_keyboard_ptr,
            keyboard_ptr->xkb_keymap_ptr);
    }

    wlr_keyboard_set_repeat_info(
        keyboard_ptr->wlr_keyboard_ptr,
        options_ptr->repeat.rate,
        options_ptr->repeat.delay);
}

/* ------------------------------------------------------------------------- */
struct xkb_keymap *wlmim_keyboard_xkb_from_config(bspl_dict_t *dict_ptr)
{
    if (NULL == dict_ptr) return NULL;
    bspl_dict_t *config_dict_ptr = bspl_dict_get_dict(dict_ptr, "Keyboard");
    if (NULL == config_dict_ptr) {
        bs_log(BS_ERROR, "Failed to retrieve \"Keyboard\" dict from config.");
        return NULL;
    }

    struct xkb_rule_names xkb_rule;
    bspl_dict_t *rmlvo_dict_ptr = _wlmim_keyboard_populate_rules(
        config_dict_ptr, &xkb_rule);
    if (NULL == rmlvo_dict_ptr) {
        bs_log(BS_ERROR, "No rule data found in \"Keyboard\" dict.");
        return NULL;
    }

    // Create keyboard layout.
    struct xkb_keymap *xkb_keymap_ptr = NULL;
    struct xkb_context *xkb_context_ptr = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (NULL == xkb_context_ptr) {
        bs_log(BS_ERROR, "Failed xkb_context_new(XKB_CONTEXT_NO_FLAGS)");
    } else {
        xkb_keymap_ptr = xkb_keymap_new_from_names(
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
        }
        xkb_context_unref(xkb_context_ptr);
    }
    bspl_dict_unref(rmlvo_dict_ptr);
    return xkb_keymap_ptr;
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
bspl_dict_t *_wlmim_keyboard_populate_rules(
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
                _wlmim_keyboard_config_ini_handler,
                rmlvo)) {
            bs_log(BS_WARNING, "Failed to parse \"XkbConfigurationFile\" at "
                   "%s, falling back to \"XkbRMLVO\" section.", fname_ptr);
            bspl_dict_unref(rmlvo);
            rmlvo = NULL;
        }
    }
    if (NULL == rmlvo) {
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
 * Handles `key` signals, ie. key presses.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a `wlr_keyboard_key_event`.
 */
void handle_key(struct wl_listener *listener_ptr, void *data_ptr)
{
    wlmim_keyboard_t *keyboard_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_keyboard_t, key_listener);
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr = data_ptr;

    wlmim_report_activity(keyboard_ptr->input_manager_ptr);

    // TODO(kaeser@gubbe.ch): Omit consumed modifiers, see xkbcommon.h.
    uint32_t modifiers = wlr_keyboard_get_modifiers(
        keyboard_ptr->wlr_keyboard_ptr);

    // TODO(kaeser@gubbe.ch): Handle this better -- should respect the
    // modifiers of the task list actions, and be more generalized.
    if ((modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT) {
        wl_signal_emit(
            &wlmim_events(keyboard_ptr->input_manager_ptr)->deactivate_task_list,
            NULL);
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
            wlmim_process_key(keyboard_ptr->input_manager_ptr,
                              key_syms[i],
                              modifiers)) {
            processed |= true;
        } else {
            processed |= wlmtk_element_keyboard_sym(
                keyboard_ptr->root_element_ptr,
                key_syms[i],
                direction,
                modifiers);
        }
    }

    if (processed) return;

    wlmtk_element_keyboard_event(
        keyboard_ptr->root_element_ptr,
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
    wlmim_keyboard_t *keyboard_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_keyboard_t, modifiers_listener);

    wlmim_report_activity(keyboard_ptr->input_manager_ptr);
    wlmim_set_keyboard_group_index(
        keyboard_ptr->input_manager_ptr,
        xkb_state_serialize_layout(
            keyboard_ptr->wlr_keyboard_ptr->xkb_state,
            XKB_STATE_LAYOUT_EFFECTIVE));

    uint32_t modifiers = wlr_keyboard_get_modifiers(
        keyboard_ptr->wlr_keyboard_ptr);
    if ((modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT) {
        wl_signal_emit(
            &wlmim_events(keyboard_ptr->input_manager_ptr)->deactivate_task_list,
            NULL);
    }

    wlr_seat_set_keyboard(
        keyboard_ptr->wlr_seat_ptr,
        keyboard_ptr->wlr_keyboard_ptr);
    wlr_seat_keyboard_notify_modifiers(
        keyboard_ptr->wlr_seat_ptr,
        &keyboard_ptr->wlr_keyboard_ptr->modifiers);
}

/* == Unit tests =========================================================== */

static void _wlmim_keyboard_test_parse(bs_test_t *test_ptr);
static void _wlmim_keyboard_test_rmlvo(bs_test_t *test_ptr);
static void _wlmim_keyboard_test_file(bs_test_t *test_ptr);

/** Test cases for the keyboard. */
static const bs_test_case_t   _wlmim_keyboard_test_cases[] = {
    { true, "parse", _wlmim_keyboard_test_parse },
    { true, "rmlvo", _wlmim_keyboard_test_rmlvo },
    { true, "file", _wlmim_keyboard_test_file },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmim_keyboard_test_set = BS_TEST_SET(
    true, "keyboard", _wlmim_keyboard_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests parsing the dict. */
void _wlmim_keyboard_test_parse(bs_test_t *test_ptr)
{
    bspl_dict_t *d = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Repeat={Delay=123;Rate=45;};}"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, d);

    struct wlmim_keyboard_options o = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_decode_dict(d, wlmim_keyboard_options_desc, &o));

    BS_TEST_VERIFY_EQ(test_ptr, 123, o.repeat.delay);
    BS_TEST_VERIFY_EQ(test_ptr, 45, o.repeat.rate);
    bspl_dict_unref(d);
}

/* ------------------------------------------------------------------------- */
/** Tests keyboard rules are loaded from a given RMLVO dict. */
void _wlmim_keyboard_test_rmlvo(bs_test_t *test_ptr)
{
    struct xkb_rule_names r = {};
    bspl_dict_t *d = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{XkbRMLVO={Rules=R;Model=M;Layout=L;Variant=V;Options=O}}"));

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, d);
    bspl_dict_t *rmlvo = _wlmim_keyboard_populate_rules(d, &r);
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
void _wlmim_keyboard_test_file(bs_test_t *test_ptr)
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

    bspl_dict_t *rmlvo = _wlmim_keyboard_populate_rules(d, &r);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, rmlvo);

    BS_TEST_VERIFY_STREQ(test_ptr, "pc105", r.model);
    BS_TEST_VERIFY_STREQ(test_ptr, "us,ch", r.layout);
    BS_TEST_VERIFY_STREQ(test_ptr, "intl,", r.variant);
    BS_TEST_VERIFY_STREQ(test_ptr, "grp:shift_caps_toggle", r.options);

    bspl_dict_unref(rmlvo);
    bspl_dict_unref(d);
}

/* == End of keyboard.c ==================================================== */
