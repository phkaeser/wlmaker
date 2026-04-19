/* ========================================================================= */
/**
 * @file manager.h
 *
 * TODO(kaeser@gubbe.ch): Minor cleanups:
 * - Make wlr_seat (and similar) as accessors, and use in device modules.
 * - Add abstract device module with a dtor method.
 * - Make the config dict parsing part of the file here.
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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
#ifndef __WLMAKER_INPUT_MANAGER_H__
#define __WLMAKER_INPUT_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>

struct wlr_backend;
struct wlr_output_layout;
struct wlr_seat;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmim_cursor_style;

/** Forward declaration. */
typedef struct _wlmim_t wlmim_t;
/** Forward declaration. */
typedef struct _wlmtk_root_t wlmtk_root_t;

/** Events provided by the input manager. */
struct wlmim_events {
    /**
     * Signals when the cursor's position is updated.
     *
     * Will be called from @ref _wlmim_cursor_handle_motion and
     * @ref _wlmim_cursor_handle_motion_absolute
     * handlers, after issuing wlr_cursor_move(), respectively
     * wlr_cursor_warp_absolute().
     *
     * Offers struct wlr_cursor as argument.
     */
    struct wl_signal          cursor_position_updated;

    /**
     * Signals when there has been any input activity.
     *
     * Useful to eg. reset an idle timer.
     */
    struct wl_signal          activity;

    /**
     * Hack: Signals when ALT modifier is lost, indicating that the
     * task list should be de-activated.
     *
     * TODO(kaeser@gubbe.ch): Handle this better -- should respect the
     * modifiers of the task list actions, and be more generalized.
     */
    struct wl_signal          deactivate_task_list;

};

/** Handle for a key binding. */
typedef struct _wlmim_keybinding_t wlmim_keybinding_t;

/** Specifies the key + modifier to bind. */
struct wlmim_keybinding_combo {
    /** Modifiers required. See `enum wlr_keyboard_modifiers`. */
    uint32_t                  modifiers;
    /** Modifier mask: Only masked modifiers are considered. */
    uint32_t                  modifiers_mask;
    /** XKB Keysym to trigger on. */
    xkb_keysym_t              keysym;
    /** Whether to ignore case when matching. */
    bool                      ignore_case;
};

/**
 * Callback for a key binding.
 *
 * @param kc                  The key combo that triggered the callback.
 *
 * @return true if the key can be considered "consumed".
 */
typedef bool (*wlmim_keybinding_callback_t)(
    const struct wlmim_keybinding_combo *kc);

/**
 * Creates an input manager: Registers input devices and ensures event routing
 * will be set up as desired.
 *
 * @param wlr_backend_ptr
 * @param wlr_output_layout_ptr
 * @param wlr_seat_ptr
 * @param config_dict_ptr
 * @param cursor_style_ptr
 * @param root_ptr
 *
 * @return Pointer to the input manager handle.
 */
wlmim_t *wlmim_input_manager_create(
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    bspl_dict_t *config_dict_ptr,
    const struct wlmim_cursor_style *cursor_style_ptr,
    wlmtk_root_t *root_ptr);

/**
 * Destroys the input manager.
 *
 * @param input_manager_ptr
 */
void wlmim_input_manager_destroy(wlmim_t *input_manager_ptr);

/** @return Pointer to wlmim_t::events. */
struct wlmim_events *wlmim_events(wlmim_t *input_manager_ptr);

/**
 * Updates the cursor style of the input manager.
 *
 * @param input_manager_ptr
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmim_set_style(
    wlmim_t *input_manager_ptr,
    const struct wlmim_cursor_style *style_ptr);

/** @return Pointer to struct wlr_cursor of @ref wlmim_t::cursor_ptr. */
struct wlr_cursor *wlmim_wlr_cursor(wlmim_t *input_manager_ptr);

/**
 * Reports input activity. Used to reset idle timers.
 *
 * @param input_manager_ptr
 */
void wlmim_report_activity(wlmim_t *input_manager_ptr);

/** Sets @ref wlmim_t::last_keyboard_group_index. */
void wlmim_set_keyboard_group_index(
    wlmim_t *input_manager_ptr,
    uint32_t index);
/** @return @ref wlmim_t::last_keyboard_group_index. */
uint32_t wlmim_get_keyboard_group_index(
    wlmim_t *input_manager_ptr);

/**
 * Binds a particular key combination to a callback.
 *
 * @param input_manager_ptr
 * @param key_combo_ptr
 * @param callback
 *
 * @return The key binding handle or NULL on error.
 */
wlmim_keybinding_t *wlmim_bind_key(
    wlmim_t *input_manager_ptr,
    const struct wlmim_keybinding_combo *key_combo_ptr,
    wlmim_keybinding_callback_t callback);

/**
 * Releases a key binding. @see wlmim_bind_key.
 *
 * @param input_manager_ptr
 * @param keybinding_ptr
 */
void wlmim_unbind_key(
    wlmim_t *input_manager_ptr,
    wlmim_keybinding_t *keybinding_ptr);

/**
 * Processes key combo: Call back if a matching binding is found.
 *
 * @param input_manager_ptr
 * @param keysym
 * @param modifiers
 *
 * @return true if a binding was found AND the callback returned true.
 */
bool wlmim_process_key(
    wlmim_t *input_manager_ptr,
    xkb_keysym_t keysym,
    uint32_t modifiers);

/** All modifiers to use by default. */
extern const uint32_t wlmim_modifiers_default_mask;

/** Unit test set. */
extern const bs_test_set_t    wlmim_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_INPUT_MANAGER_H__
/* == End of manager.h ===================================================== */
