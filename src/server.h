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

#include "cursor.h"
#include "output.h"
#include "keyboard.h"
#include "layer_shell.h"
#include "view.h"
#include "subprocess_monitor.h"
#include "icon_manager.h"
#include "xdg_decoration.h"
#include "xdg_shell.h"
#include "workspace.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the Wayland server. */
struct _wlmaker_server_t {
    /** Wayland display. */
    struct wl_display         *wl_display_ptr;
    /** Name of the socket for clients to connect. */
    const char                *wl_socket_name_ptr;

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

    /** The list of outputs. */
    bs_dllist_t               outputs;
    /** The list of input devices. */
    bs_dllist_t               input_devices;

    /** The current workspace. */
    wlmaker_workspace_t       *current_workspace_ptr;
    /** List of all workspaces. */
    bs_dllist_t               workspaces;
    /**
     * Signal: Raised when the current workspace is changed.
     * Data: Pointer to the new `wlmaker_workspace_t`.
     */
    struct wl_signal          workspace_changed;
    /** Signal: When the task list is enabled. (to be shown) */
    struct wl_signal          task_list_enabled_event;
    /** Signal: When the task list is disabled. (to be hidden) */
    struct wl_signal          task_list_disabled_event;

    /** Keys bound to specific actions. */
    bs_dllist_t               key_bindings;

    /** Clients for this server. */
    bs_dllist_t               clients;

    /** Subprocess monitoring. */
    wlmaker_subprocess_monitor_t *monitor_ptr;

    /** Signal: Triggered whenever a view is created. */
    struct wl_signal          view_created_event;
    /**
     * Signal: Triggered whenever a view is mapped.
     *
     * The signal is raised right after the view was mapped.
     */
    struct wl_signal          view_mapped_event;
    /**
     * Signal: Triggered whenever a view is unmapped.
     *
     * The signal is raised right after the view was unmapped.
     */
    struct wl_signal          view_unmapped_event;
    /** Signal: Triggered whenever a view is destroyed. */
    struct wl_signal          view_destroyed_event;
};

/** Callback for key binding. */
typedef void (*wlmaker_server_bind_key_callback_t)(
    wlmaker_server_t *server_ptr,
    void *callback_arg_ptr);

/** State of a key binding. */
typedef struct _wlmaker_server_key_binding_t wlmaker_server_key_binding_t;

/**
 * Creates the server and initializes all needed sub-modules.
 *
 * @return The server handle or NULL on failure. The handle must be freed by
 * calling wlmaker_server_destroy().
 */
wlmaker_server_t *wlmaker_server_create(void);

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
 * Binds the callback to the specified key and modifiers.
 *
 * @param server_ptr
 * @param key_sym             The key to bind. Both upper- and lower-case will
 *                            be bound!
 * @param modifiers           Modifiers of the bound key.
 * @param callback            Callback for when key is pressed.
 * @param callback_arg_ptr    Argument to pass to |callback|.
 */
wlmaker_server_key_binding_t *wlmaker_server_bind_key(
    wlmaker_server_t *server_ptr,
    xkb_keysym_t key_sym,
    uint32_t modifiers,
    wlmaker_server_bind_key_callback_t callback,
    void *callback_arg_ptr);

/**
 * Releases a previously-bound key binding.
 *
 * @param server_ptr
 * @param key_binding_ptr
 */
void wlmaker_server_unbind_key(
    wlmaker_server_t *server_ptr,
    wlmaker_server_key_binding_t *key_binding_ptr);

/**
 * Processes a key press: Looks for matching bindings and runs the callback.
 *
 * @param server_ptr
 * @param key_sym
 * @param modifiers
 *
 * @return true, if there was a matching binding; false if not.
 */
bool wlmaker_server_process_key(
    wlmaker_server_t *server_ptr,
    xkb_keysym_t key_sym,
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
 * Looks up which output serves the current cursor coordinates and returns that.
 *
 * @param server_ptr
 *
 * @return Pointer to the output at the seat's cursor position.
 */
struct wlr_output *wlmaker_server_get_output_at_cursor(
    wlmaker_server_t *server_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_SERVER_H__ */
/* == End of server.h ================================================== */
