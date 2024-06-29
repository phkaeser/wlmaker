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
#include <wayland-server-core.h>

#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
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

#include "config.h"
#include "cursor.h"
#include "idle.h"
#include "output.h"
#include "keyboard.h"
#include "layer_shell.h"
#include "lock_mgr.h"
#include "root.h"
#include "subprocess_monitor.h"
#include "icon_manager.h"
#include "xdg_decoration.h"
#include "xdg_shell.h"
#include "xwl.h"
#include "workspace.h"

#include "conf/model.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Options for the Wayland server. */
typedef struct {
    /** Whether to start XWayland. */
    bool                      start_xwayland;
} wlmaker_server_options_t;

/** State of the Wayland server. */
struct _wlmaker_server_t {
    /** Configuration dictionnary. */
    wlmcfg_dict_t             *config_dict_ptr;
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

    /** wlroots allocator. */
    struct wlr_allocator      *wlr_allocator_ptr;
    /** wlroots backend. */
    struct wlr_backend        *wlr_backend_ptr;
    /** wlroots output layout helper. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** wlroots renderer. */
    struct wlr_renderer       *wlr_renderer_ptr;
    /** wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The scene graph API. */
    struct wlr_scene          *wlr_scene_ptr;
    /** The scene output layout. */
    struct wlr_scene_output_layout *wlr_scene_output_layout_ptr;
    /**
     * Another scene graph, not connected to any output.
     *
     * We're using this graph's scene tree for "parking" scene nodes when they
     * are not part of a workspace.
     *
     * TODO(kaeser@gubbe.ch): Consider whether this is actually needed.
     */
    struct wlr_scene          *void_wlr_scene_ptr;

    /** Listener for `new_output` signals raised by `wlr_backend`. */
    struct wl_listener        backend_new_output_listener;
    /** Listener for `new_input` signals raised by `wlr_backend`. */
    struct wl_listener        backend_new_input_device_listener;
    /** Listener for `change` signals raised by `wlr_output_layout`. */
    struct wl_listener        output_layout_change_listener;

    // From tinywl.c: A few hands-off wlroots interfaces.

    /** The compositor is necessary for clients to allocate surfaces. */
    struct wlr_compositor     *wlr_compositor_ptr;
    /**
     * The subcompositor allows to assign the role of subsurfaces to
     * surfaces.
     */
    struct wlr_subcompositor  *wlr_subcompositor_ptr;
    /** The data device manager handles the clipboard. */
    struct wlr_data_device_manager *wlr_data_device_manager_ptr;

    /** The cursor handler. */
    wlmaker_cursor_t          *cursor_ptr;
    /** The XDG Shell handler. */
    wlmaker_xdg_shell_t       *xdg_shell_ptr;
    /** The XDG decoration manager. */
    wlmaker_xdg_decoration_manager_t *xdg_decoration_manager_ptr;
    /** Layer shell handler. */
    wlmaker_layer_shell_t     *layer_shell_ptr;
    /** Icon manager. */
    wlmaker_icon_manager_t    *icon_manager_ptr;
    /**
     * XWayland interface. Will be set only if compiled with XWayland, through
     * WLMAKER_HAVE_XWAYLAND defined.
     * And through setting @ref wlmaker_server_options_t::start_xwayland.
     */
    wlmaker_xwl_t             *xwl_ptr;

    /** The list of outputs. */
    bs_dllist_t               outputs;
    /** The list of input devices. */
    bs_dllist_t               input_devices;

    /** Toolkit environment. */
    wlmtk_env_t               *env_ptr;

    /** The root element. */
    wlmaker_root_t            *root_ptr;
    /** The current workspace. */
    wlmaker_workspace_t       *current_workspace_ptr;
    /** List of all workspaces. */
    bs_dllist_t               workspaces;
    /** Fake workspace, injectable for tests. */
    wlmtk_fake_workspace_t    *fake_wlmtk_workspace_ptr;
    /**
     * Signal: Raised when the current workspace is changed.
     * Data: Pointer to the new `wlmaker_workspace_t`.
     */
    struct wl_signal          workspace_changed;
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

    // TODO(kaeser@gubbe.ch): Move these events into a 'registry' struct, so
    // it can be more easily shared throughout the code.
    /** Signal: Triggered whenever a window is created. */
    struct wl_signal          window_created_event;
    /** Signal: Triggered whenever a window is destroyed. */
    struct wl_signal          window_destroyed_event;
    /**
     * Signal: Triggered whenever a window is mapped.
     *
     * The signal is raised right after the window was mapped.
     */
    struct wl_signal          window_mapped_event;
    /**
     * Signal: Triggered whenever a window is unmapped.
     *
     * The signal is raised right after the window was unmapped.
     */
    struct wl_signal          window_unmapped_event;

    /** Temporary: Points to the @ref wlmtk_dock_t of the clip. */
    wlmtk_dock_t              *clip_dock_ptr;

    /** The current configuration style. */
    wlmaker_config_style_t    style;
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
 * @param options_ptr         Options for the server. The server expects the
 *                            pointed area to outlive the server.
 *
 * @return The server handle or NULL on failure. The handle must be freed by
 * calling wlmaker_server_destroy().
 */
wlmaker_server_t *wlmaker_server_create(
    wlmcfg_dict_t *config_dict_ptr,
    const wlmaker_server_options_t *options_ptr);

/**
 * Destroys the server handle, as created by wlmaker_server_create().
 *
 * @param server_ptr
 */
void wlmaker_server_destroy(wlmaker_server_t *server_ptr);

/**
 * Adds the output.
 *
 * @param server_ptr
 * @param output_ptr
 */
void wlmaker_server_output_add(wlmaker_server_t *server_ptr,
                               wlmaker_output_t *output_ptr);

/**
 * Removes the output.
 *
 * @param server_ptr
 * @param output_ptr
 */
void wlmaker_server_output_remove(wlmaker_server_t *server_ptr,
                                  wlmaker_output_t *output_ptr);

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
 * Returns the currently active workspace.
 *
 * @param server_ptr
 *
 * @return Pointer to the `wlmaker_workspace_t` currently active.
 */
wlmaker_workspace_t *wlmaker_server_get_current_workspace(
    wlmaker_server_t *server_ptr);

/**
 * Returns the currently active workspace.
 *
 * @param server_ptr
 *
 * @return Pointer to the `wlmtk_workspace_t` currently active.
 */
wlmtk_workspace_t *wlmaker_server_get_current_wlmtk_workspace(
    wlmaker_server_t *server_ptr);

/**
 * Switches to the next workspace.
 *
 * @param server_ptr
 */
void wlmaker_server_switch_to_next_workspace(wlmaker_server_t *server_ptr);

/**
 * Switches to the previous workspace.
 *
 * @param server_ptr
 */
void wlmaker_server_switch_to_previous_workspace(wlmaker_server_t *server_ptr);

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

/** Unit test cases. */
extern const bs_test_case_t   wlmaker_server_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_SERVER_H__ */
/* == End of server.h ================================================== */
