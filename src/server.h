/* ========================================================================= */
/**
 * @file server.h
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
#ifndef __WLMAKER_SERVER_H__
#define __WLMAKER_SERVER_H__

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

/** A handle for a wlmaker server. */
typedef struct _wlmaker_server_t wlmaker_server_t;

/** A key combination. */
typedef struct _wlmaker_key_combo_t wlmaker_key_combo_t;
/** Handle for a key binding. */
typedef struct _wlmaker_key_binding_t wlmaker_key_binding_t;

/**
 * Callback for a key binding.
 *
 * @param kc                  The key combo that triggered the callback.
 *
 * @return true if the key can be considered "consumed".
 */
typedef bool (*wlmaker_keybinding_callback_t)(const wlmaker_key_combo_t *kc);

#include "backend/backend.h"
#include "config.h"
#include "corner.h"  // IWYU pragma: keep
#include "cursor.h"  // IWYU pragma: keep
#include "files.h"
#include "icon_manager.h"  // IWYU pragma: keep
#include "input_observation.h"
#include "idle.h"  // IWYU pragma: keep
#include "layer_shell.h"  // IWYU pragma: keep
#include "lock_mgr.h"  // IWYU pragma: keep
#include "root_menu.h"  // IWYU pragma: keep
#include "subprocess_monitor.h"  // IWYU pragma: keep
#include "toolkit/toolkit.h"
#include "xdg_decoration.h"  // IWYU pragma: keep
#include "xdg_shell.h"  // IWYU pragma: keep
#include "xwl.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Options for the Wayland server. */
typedef struct {
    /** Whether to start XWayland. */
    bool                      start_xwayland;
    /** Desired output width, for windowed mode. 0 for no preference. */
    uint32_t                  width;
    /** Desired output height, for windowed mode. 0 for no preference. */
    uint32_t                  height;
    /** Whether to include 'Logo' to modifiers. */
    bool                      bind_with_logo;
} wlmaker_server_options_t;

/** State of the Wayland server. */
struct _wlmaker_server_t {
    /** Files module handle. */
    wlmaker_files_t           *files_ptr;
    /** Configuration dictionnary. */
    bspl_dict_t               *config_dict_ptr;
    /** Copy of the options. */
    const wlmaker_server_options_t *options_ptr;

    /** Wayland display. */
    struct wl_display         *wl_display_ptr;
    /** Name of the socket for clients to connect. */
    const char                *wl_socket_name_ptr;

    /** Session lock manager. */
    wlmaker_lock_mgr_t        *lock_mgr_ptr;
    /** Idle monitor. */
    wlmaker_idle_monitor_t    *idle_monitor_ptr;

    /** WLR viewporter. */
    struct wlr_viewporter     *wlr_viewporter_ptr;
    /** Fractional scale manager. */
    struct wlr_fractional_scale_manager_v1 *wlr_fractional_scale_manager_ptr;
    /** wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The scene graph API. */
    struct wlr_scene          *wlr_scene_ptr;
    /** wlroots output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Listener for `new_input` signals raised by `wlr_backend`. */
    struct wl_listener        backend_new_input_device_listener;

    // Clipboard and selection support.

    /** Provides the wl_data_device protocol for clipboard (Ctrl+C/V). */
    struct wlr_data_device_manager *wlr_data_device_manager_ptr;

    /** Listener to approve clipboard selection requests. */
    struct wl_listener        request_set_selection_listener;

    /** Provides the primary selection protocol for middle-click paste. */
    struct wlr_primary_selection_v1_device_manager
        *wlr_primary_selection_v1_device_manager_ptr;

    /** Listener to approve primary selection requests. */
    struct wl_listener        request_set_primary_selection_listener;

    /** Enables clipboard managers and tools (wl-copy, wl-paste). */
    struct wlr_data_control_manager_v1 *wlr_data_control_manager_v1_ptr;

    /** The cursor handler. */
    wlmaker_cursor_t          *cursor_ptr;
    /** The XDG Shell handler. */
    wlmaker_xdg_shell_t       *xdg_shell_ptr;
    /** The XDG decoration manager. */
    wlmaker_xdg_decoration_manager_t *xdg_decoration_manager_ptr;
    /** Layer shell handler. */
    wlmaker_layer_shell_t     *layer_shell_ptr;
    /** Backend handler. */
    wlmbe_backend_t           *backend_ptr;
    /** Icon manager. */
    wlmaker_icon_manager_t    *icon_manager_ptr;
    /** Input observation. */
    wlmaker_input_observation_manager_t *input_observation_manager_ptr;
    /**
     * XWayland interface. Will be set only if compiled with XWayland, through
     * WLMAKER_HAVE_XWAYLAND defined.
     * And through setting @ref wlmaker_server_options_t::start_xwayland.
     */
    wlmaker_xwl_t             *xwl_ptr;

    /** The list of input devices. */
    bs_dllist_t               input_devices;

    /** The root element. */
    wlmtk_root_t              *root_ptr;
    /** Whether the task list is currently shown. */
    bool                      task_list_enabled;
    /** Signal: When the task list is enabled. (to be shown) */
    struct wl_signal          task_list_enabled_event;
    /** Signal: When the task list is disabled. (to be hidden) */
    struct wl_signal          task_list_disabled_event;

    /** List of all bound keys, see @ref wlmaker_key_binding_t::dlnode. */
    bs_dllist_t               bindings;

    /** Clients for this server. */
    bs_dllist_t               clients;

    /** Subprocess monitoring. */
    wlmaker_subprocess_monitor_t *monitor_ptr;

    /** Montor & handler of 'hot corners'. */
    wlmaker_corner_t          *corner_ptr;

    // TODO(kaeser@gubbe.ch): Move these events into a 'registry' struct, so
    // it can be more easily shared throughout the code.
    /** Signal: Triggered whenever a window is created. */
    struct wl_signal          window_created_event;
    /** Signal: Triggered whenever a window is destroyed. */
    struct wl_signal          window_destroyed_event;

    /** Temporary: Points to the @ref wlmtk_dock_t of the clip. */
    wlmtk_dock_t              *clip_dock_ptr;

    /** Root menu, when active. NULL when not invoked. */
    wlmaker_root_menu_t       *root_menu_ptr;
    /** Parsed contents of the root menu definition, from plist. */
    bspl_array_t            *root_menu_array_ptr;
    /** Listener for `unclaimed_button_event` signal raised by `wlmtk_root`. */
    struct wl_listener        unclaimed_button_event_listener;

    /** The current configuration style. */
    const wlmaker_config_style_t *style_ptr;
};

/** Specifies the key + modifier to bind. */
struct _wlmaker_key_combo_t {
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
 * Creates the server and initializes all needed sub-modules.
 *
 * @param config_dict_ptr     Configuration, as dictionary object. The server
 *                            will keep a reference on it until destroyed.
 * @param files_ptr           Files handle. Must outlive the server.
 * @param style_ptr           Style. Must outlive the server.
 * @param options_ptr         Options for the server. The server expects the
 *                            pointed area to outlive the server.
 *
 * @return The server handle or NULL on failure. The handle must be freed by
 * calling wlmaker_server_destroy().
 */
wlmaker_server_t *wlmaker_server_create(
    bspl_dict_t *config_dict_ptr,
    wlmaker_files_t *files_ptr,
    const wlmaker_config_style_t *style_ptr,
    const wlmaker_server_options_t *options_ptr);

/**
 * Destroys the server handle, as created by wlmaker_server_create().
 *
 * @param server_ptr
 */
void wlmaker_server_destroy(wlmaker_server_t *server_ptr);

/**
 * Binds a particular key to a callback.
 *
 * @param server_ptr
 * @param key_combo_ptr
 * @param callback
 *
 * @return The key binding handle or NULL on error.
 */
wlmaker_key_binding_t *wlmaker_server_bind_key(
    wlmaker_server_t *server_ptr,
    const wlmaker_key_combo_t *key_combo_ptr,
    wlmaker_keybinding_callback_t callback);

/**
 * Releases a key binding. @see wlmaker_bind_key.
 *
 * @param server_ptr
 * @param key_binding_ptr
 */
void wlmaker_server_unbind_key(
    wlmaker_server_t *server_ptr,
    wlmaker_key_binding_t *key_binding_ptr);

/**
 * Processes key bindings: Call back if a matching binding is found.
 *
 * @param server_ptr
 * @param keysym
 * @param modifiers
 *
 * @return true if a binding was found AND the callback returned true.
 */
bool wlmaker_keyboard_process_bindings(
    wlmaker_server_t *server_ptr,
    xkb_keysym_t keysym,
    uint32_t modifiers);

/**
 * Activates the task list.
 *
 * @param server_ptr
 */
void wlmaker_server_activate_task_list(wlmaker_server_t *server_ptr);

/**
 * De-activates the task list.
 *
 * @param server_ptr
 */
void wlmaker_server_deactivate_task_list(wlmaker_server_t *server_ptr);

/**
 * Looks up which output serves the current cursor coordinates and returns that.
 *
 * @param server_ptr
 *
 * @return Pointer to the output at the seat's cursor position.
 */
struct wlr_output *wlmaker_server_get_output_at_cursor(
    wlmaker_server_t *server_ptr);

/** All modifiers to use by default. */
extern const uint32_t wlmaker_modifier_default_mask;

/** Unit test set. */
extern const bs_test_set_t   wlmaker_server_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_SERVER_H__ */
/* == End of server.h ================================================== */
