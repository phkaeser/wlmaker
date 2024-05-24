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

#include "config.h"
#include "toolkit/toolkit.h"
#include "server.h"

/* == Declarations ========================================================= */

/** Keyboard handle. */
struct _wlmaker_keyboard_t {
    /** Configuration dictionnary, just the "Keyboard" section. */
    wlmcfg_dict_t             *config_dict_ptr;
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

    /** Whether the task switching mode is currently enabled. */
    bool                      task_switch_mode_enabled;
};

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
    wlmcfg_object_t *object_ptr = wlmcfg_dict_get(
        server_ptr->config_dict_ptr, "Keyboard");
    wlmcfg_dict_t *config_dict_ptr;
    if (NULL == object_ptr ||
        NULL == (config_dict_ptr = wlmcfg_dict_from_object(object_ptr))) {
        bs_log(BS_ERROR, "Failed to retrieve \"Keyboard\" dict from config.");
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }
    BS_ASSERT_NOTNULL(wlmcfg_object_dup(object_ptr));
    keyboard_ptr->config_dict_ptr = config_dict_ptr;

    // Set keyboard layout.
    struct xkb_context *xkb_context_ptr = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *xkb_keymap_ptr = xkb_keymap_new_from_names(
        xkb_context_ptr,
        config_keyboard_rule_names,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (NULL == xkb_keymap_ptr) {
        bs_log(BS_ERROR, "Failed xkb_keymap_new_from_names(%p, { .rules = %s, "
               ".model = %s, .layout = %s, variant = %s, .options = %s }, "
               "XKB_KEYMAP_COMPILE_NO_NO_FLAGS)",
               xkb_context_ptr,
               config_keyboard_rule_names->rules,
               config_keyboard_rule_names->model,
               config_keyboard_rule_names->layout,
               config_keyboard_rule_names->variant,
               config_keyboard_rule_names->options);
        wlmaker_keyboard_destroy(keyboard_ptr);
        return NULL;
    }
    wlr_keyboard_set_keymap(keyboard_ptr->wlr_keyboard_ptr, xkb_keymap_ptr);
    xkb_keymap_unref(xkb_keymap_ptr);
    xkb_context_unref(xkb_context_ptr);

    // Repeat rate and delay.
    wlr_keyboard_set_repeat_info(
        keyboard_ptr->wlr_keyboard_ptr,
        config_keyboard_repeat_rate,
        config_keyboard_repeat_delay);

    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.key,
        &keyboard_ptr->key_listener,
        handle_key);
    wlmtk_util_connect_listener_signal(
        &keyboard_ptr->wlr_keyboard_ptr->events.modifiers,
        &keyboard_ptr->modifiers_listener,
        handle_modifiers);

    wlr_seat_set_keyboard(wlr_seat_ptr, keyboard_ptr->wlr_keyboard_ptr);
    return keyboard_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_keyboard_destroy(wlmaker_keyboard_t *keyboard_ptr)
{
    wl_list_remove(&keyboard_ptr->key_listener.link);
    wl_list_remove(&keyboard_ptr->modifiers_listener.link);

    if (NULL != keyboard_ptr->config_dict_ptr) {
        wlmcfg_object_destroy(wlmcfg_object_from_dict(
                                  keyboard_ptr->config_dict_ptr));
        keyboard_ptr->config_dict_ptr = NULL;
    }

    free(keyboard_ptr);
}

/* == Local (static) methods =============================================== */

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

    if ((modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT &&
        keyboard_ptr->task_switch_mode_enabled) {
        keyboard_ptr->task_switch_mode_enabled = false;
        wl_signal_emit(
            &keyboard_ptr->server_ptr->task_list_disabled_event, NULL);
        wlmaker_workspace_t *workspace_ptr =
            wlmaker_server_get_current_workspace(keyboard_ptr->server_ptr);
        wlmtk_workspace_t *wlmtk_ptr = wlmaker_workspace_wlmtk(workspace_ptr);
        wlmtk_window_t *window_ptr =
            wlmtk_workspace_get_activated_window(wlmtk_ptr);
        if (NULL != window_ptr) {
            wlmtk_workspace_raise_window(wlmtk_ptr, window_ptr);
        }
    }

    // For key presses: Pass them on to the server, for potential key bindings.
    bool processed = false;
    if (WL_KEYBOARD_KEY_STATE_PRESSED == wlr_keyboard_key_event_ptr->state) {
        // Translates libinput keycode -> xkbcommon.
        uint32_t keycode = wlr_keyboard_key_event_ptr->keycode + 8;

        // A key may have multiple syms associated; get them here.
        const xkb_keysym_t *key_syms;
        int key_syms_count = xkb_state_key_get_syms(
            keyboard_ptr->wlr_keyboard_ptr->xkb_state, keycode, &key_syms);
        for (int i = 0; i < key_syms_count; ++i) {

            if (((modifiers & WLR_MODIFIER_ALT) == WLR_MODIFIER_ALT) &&
                (key_syms[i] == XKB_KEY_Escape)) {
                if ((modifiers & WLR_MODIFIER_SHIFT) == WLR_MODIFIER_SHIFT) {
                    wlmtk_workspace_activate_previous_window(
                        wlmaker_workspace_wlmtk(
                            wlmaker_server_get_current_workspace(
                                keyboard_ptr->server_ptr)));
                } else {
                    wlmtk_workspace_activate_next_window(
                        wlmaker_workspace_wlmtk(
                            wlmaker_server_get_current_workspace(
                                keyboard_ptr->server_ptr)));
                    }
                keyboard_ptr->task_switch_mode_enabled = true;
                wl_signal_emit(
                    &keyboard_ptr->server_ptr->task_list_enabled_event, NULL);
                processed = true;
            }  else {
                processed = wlmaker_server_process_key(
                    keyboard_ptr->server_ptr,
                    key_syms[i],
                    modifiers);
            }
        }
    }

    if (processed) return;

    processed = wlmtk_element_keyboard_event(
        wlmaker_root_element(keyboard_ptr->server_ptr->root_ptr),
        wlr_keyboard_key_event_ptr,
        NULL,
        0,
        modifiers);
    if (processed) return;

    processed = wlmtk_element_keyboard_event(
        wlmtk_workspace_element(
            wlmaker_workspace_wlmtk(wlmaker_server_get_current_workspace(
                                        keyboard_ptr->server_ptr))),
        wlr_keyboard_key_event_ptr,
        NULL,
        0,
        modifiers);
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

    uint32_t modifiers = wlr_keyboard_get_modifiers(
        keyboard_ptr->wlr_keyboard_ptr);

    if (keyboard_ptr->task_switch_mode_enabled &&
        (modifiers & WLR_MODIFIER_ALT) != WLR_MODIFIER_ALT) {
        keyboard_ptr->task_switch_mode_enabled = false;
        wl_signal_emit(
            &keyboard_ptr->server_ptr->task_list_disabled_event, NULL);
        wlmaker_workspace_t *workspace_ptr =
            wlmaker_server_get_current_workspace(keyboard_ptr->server_ptr);
        wlmtk_workspace_t *wlmtk_ptr = wlmaker_workspace_wlmtk(workspace_ptr);
        wlmtk_window_t *window_ptr =
            wlmtk_workspace_get_activated_window(wlmtk_ptr);
        if (NULL != window_ptr) {
            wlmtk_workspace_raise_window(wlmtk_ptr, window_ptr);
        }
    }

    wlr_seat_set_keyboard(
        keyboard_ptr->wlr_seat_ptr,
        keyboard_ptr->wlr_keyboard_ptr);
    wlr_seat_keyboard_notify_modifiers(
        keyboard_ptr->wlr_seat_ptr,
        &keyboard_ptr->wlr_keyboard_ptr->modifiers);
}

/* == End of keyboard.c ==================================================== */
