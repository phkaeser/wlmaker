/* ========================================================================= */
/**
 * @file server.c
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

#include "server.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "keyboard.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Name of the "seat". */
static const char             *seat_name_ptr = "seat0";

/** Wraps an input device. */
typedef struct {
    /** List node, as an element of `wlmaker_server_t.input_devices`. */
    bs_dllist_node_t          node;
    /** Back-link to the server this belongs. */
    wlmaker_server_t          *server_ptr;
    /** The input device. */
    struct wlr_input_device   *wlr_input_device_ptr;
    /** Handle to the wlmaker actual device. */
    void                      *handle_ptr;
    /** Listener for the `destroy` signal of `wlr_input_device`. */
    struct wl_listener        destroy_listener;
} wlmaker_input_device_t;

/** Internal struct holding a keybinding. */
struct _wlmaker_key_binding_t {
    /** Node within @ref wlmaker_server_t::bindings. */
    bs_dllist_node_t          dlnode;
    /** The key binding: Modifier and keysym to bind to. */
    const wlmaker_key_combo_t *key_combo_ptr;
    /** Callback for when this modifier + key is encountered. */
    wlmaker_keybinding_callback_t callback;
};

static bool register_input_device(
    wlmaker_server_t *server_ptr,
    struct wlr_input_device *wlr_input_device_ptr,
    void *handle_ptr);

static void handle_new_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_server_unclaimed_button_event_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

const uint32_t wlmaker_modifier_default_mask = (
    WLR_MODIFIER_SHIFT |
    // Excluding: WLR_MODIFIER_CAPS.
    WLR_MODIFIER_CTRL |
    WLR_MODIFIER_ALT |
    WLR_MODIFIER_MOD2 |
    WLR_MODIFIER_MOD3 |
    WLR_MODIFIER_LOGO |
    WLR_MODIFIER_MOD5);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_server_t *wlmaker_server_create(
    bspl_dict_t *config_dict_ptr,
    const wlmaker_server_options_t *options_ptr)
{
    wlmaker_server_t *server_ptr = logged_calloc(1, sizeof(wlmaker_server_t));
    if (NULL == server_ptr) return NULL;
    server_ptr->options_ptr = options_ptr;

    server_ptr->config_dict_ptr = bspl_dict_ref(config_dict_ptr);
    if (NULL == server_ptr->config_dict_ptr) {
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    wl_signal_init(&server_ptr->task_list_enabled_event);
    wl_signal_init(&server_ptr->task_list_disabled_event);

    wl_signal_init(&server_ptr->window_created_event);
    wl_signal_init(&server_ptr->window_destroyed_event);

    // Prepare display and socket.
    server_ptr->wl_display_ptr = wl_display_create();
    if (NULL == server_ptr->wl_display_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }
    server_ptr->wl_socket_name_ptr = wl_display_add_socket_auto(
        server_ptr->wl_display_ptr);
    if (NULL == server_ptr->wl_socket_name_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_add_socket_auto()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    // Configure the seat, which is the potential set of input devices operated
    // by one user at a computer's "seat".
    server_ptr->wlr_seat_ptr = wlr_seat_create(
        server_ptr->wl_display_ptr, seat_name_ptr);
    if (NULL == server_ptr->wlr_seat_ptr) {
        bs_log(BS_ERROR, "Failed wlr_seat_create()");
        wlmaker_server_destroy(server_ptr);
    }

    // The scene graph.
    server_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == server_ptr->wlr_scene_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->wlr_output_layout_ptr = wlr_output_layout_create(
        server_ptr->wl_display_ptr);
    if (NULL == server_ptr->wlr_output_layout_ptr) {
        bs_log(BS_ERROR, "Failed wlr_output_layout_create(%p)",
               server_ptr->wl_display_ptr);
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->backend_ptr = wlmbe_backend_create(
        server_ptr->wl_display_ptr,
        server_ptr->wlr_scene_ptr,
        server_ptr->wlr_output_layout_ptr,
        server_ptr->options_ptr->width,
        server_ptr->options_ptr->height,
        server_ptr->config_dict_ptr);
    if (NULL == server_ptr->backend_ptr) {
        bs_log(BS_ERROR, "Failed wlmbe_backend_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }
    // Listen for new (or newly recognized) output and input devices.
    wlmtk_util_connect_listener_signal(
        &wlmbe_backend_wlr(server_ptr->backend_ptr)->events.new_input,
        &server_ptr->backend_new_input_device_listener,
        handle_new_input_device);

    server_ptr->cursor_ptr = wlmaker_cursor_create(
        server_ptr,
        server_ptr->wlr_output_layout_ptr);
    if (NULL == server_ptr->cursor_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_cursor_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->env_ptr = wlmtk_env_create(
        server_ptr->cursor_ptr->wlr_cursor_ptr,
        server_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
        server_ptr->wlr_seat_ptr);
    if (NULL == server_ptr->env_ptr) {
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    // Root element.
    server_ptr->root_ptr = wlmtk_root_create(
        server_ptr->wlr_scene_ptr,
        server_ptr->wlr_output_layout_ptr,
        server_ptr->env_ptr);
    if (NULL == server_ptr->root_ptr) {
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }
    wlmtk_util_connect_listener_signal(
        &wlmtk_root_events(server_ptr->root_ptr)->unclaimed_button_event,
        &server_ptr->unclaimed_button_event_listener,
        _wlmaker_server_unclaimed_button_event_handler);

    // Session lock manager.
    server_ptr->lock_mgr_ptr = wlmaker_lock_mgr_create(server_ptr);
    if (NULL == server_ptr->lock_mgr_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_lock_mgr_create(%p)", server_ptr);
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    // Idle monitor.
    server_ptr->idle_monitor_ptr = wlmaker_idle_monitor_create(server_ptr);
    if (NULL == server_ptr->idle_monitor_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_idle_monitor_create(%p)", server_ptr);
        return NULL;
    }

    // The below helpers all setup a listener |display_destroy| for freeing the
    // assets held via the respective create() calls. Hence no need to call a
    // clean-up method from our end.
    server_ptr->wlr_data_device_manager_ptr = wlr_data_device_manager_create(
        server_ptr->wl_display_ptr);
    if (NULL == server_ptr->wlr_data_device_manager_ptr) {
        bs_log(BS_ERROR, "Failed wlr_data_device_manager_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->xdg_shell_ptr = wlmaker_xdg_shell_create(server_ptr);
    if (NULL == server_ptr->xdg_shell_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_xdg_shell_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->xdg_decoration_manager_ptr =
        wlmaker_xdg_decoration_manager_create(server_ptr);
    if (NULL == server_ptr->xdg_decoration_manager_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_xdg_decoration_manager_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->layer_shell_ptr = wlmaker_layer_shell_create(server_ptr);
    if (NULL == server_ptr->layer_shell_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_layer_shell_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->icon_manager_ptr = wlmaker_icon_manager_create(
        server_ptr->wl_display_ptr, server_ptr);
    if (NULL == server_ptr->icon_manager_ptr) {
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    if (server_ptr->options_ptr->start_xwayland) {
        server_ptr->xwl_ptr = wlmaker_xwl_create(server_ptr);
        if (NULL == server_ptr->xwl_ptr) {
            wlmaker_server_destroy(server_ptr);
            return NULL;
        }
    }

    server_ptr->monitor_ptr = wlmaker_subprocess_monitor_create(server_ptr);
    if (NULL == server_ptr->monitor_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_subprocess_monitor_create()");
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    server_ptr->corner_ptr = wlmaker_corner_create(
        bspl_dict_get_dict(server_ptr->config_dict_ptr, "HotCorner"),
        wl_display_get_event_loop(server_ptr->wl_display_ptr),
        server_ptr->wlr_output_layout_ptr,
        server_ptr->cursor_ptr,
        server_ptr);
    if (NULL == server_ptr->corner_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_corner_create(%p, %p, %p, %p, %p)",
               bspl_dict_get_dict(server_ptr->config_dict_ptr, "HotCorner"),
               wl_display_get_event_loop(server_ptr->wl_display_ptr),
               server_ptr->wlr_output_layout_ptr,
               server_ptr->cursor_ptr,
               server_ptr);
        wlmaker_server_destroy(server_ptr);
        return NULL;
    }

    return server_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_server_destroy(wlmaker_server_t *server_ptr)
{
    if (NULL != server_ptr->root_menu_ptr) {
        wlmaker_root_menu_destroy(server_ptr->root_menu_ptr);
        server_ptr->root_menu_ptr = NULL;
    }

    // We don't destroy a few of the handlers, since wlroots will crash if
    // they are destroyed -- and apparently, wlroots cleans them up anyway.
    // These are:
    // * server_ptr->wlr_seat_ptr
    // * server_ptr->wlr_backend_ptr
    // * server_ptr->wlr_scene_ptr  (there is no "destroy" function)
    {
        bs_dllist_node_t *dlnode_ptr = server_ptr->bindings.head_ptr;
        while (NULL != dlnode_ptr) {
            wlmaker_key_binding_t *key_binding_ptr = BS_CONTAINER_OF(
                dlnode_ptr, wlmaker_key_binding_t, dlnode);
            dlnode_ptr = dlnode_ptr->next_ptr;
            wlmaker_server_unbind_key(server_ptr, key_binding_ptr);
        }
    }

    if (NULL != server_ptr->corner_ptr) {
        wlmaker_corner_destroy(server_ptr->corner_ptr);
        server_ptr->corner_ptr = NULL;
    }

    if (NULL != server_ptr->monitor_ptr) {
        wlmaker_subprocess_monitor_destroy(server_ptr->monitor_ptr);
        server_ptr->monitor_ptr =NULL;
    }

    if (NULL != server_ptr->xwl_ptr) {
        wlmaker_xwl_destroy(server_ptr->xwl_ptr);
        server_ptr->xwl_ptr = NULL;
    }

    if (NULL != server_ptr->icon_manager_ptr) {
        wlmaker_icon_manager_destroy(server_ptr->icon_manager_ptr);
        server_ptr->icon_manager_ptr = NULL;
    }

    if (NULL != server_ptr->layer_shell_ptr) {
        wlmaker_layer_shell_destroy(server_ptr->layer_shell_ptr);
        server_ptr->layer_shell_ptr = NULL;
    }

    if (NULL != server_ptr->xdg_decoration_manager_ptr) {
        wlmaker_xdg_decoration_manager_destroy(
            server_ptr->xdg_decoration_manager_ptr);
        server_ptr->xdg_decoration_manager_ptr = NULL;
    }

    if (NULL != server_ptr->xdg_shell_ptr) {
        wlmaker_xdg_shell_destroy(server_ptr->xdg_shell_ptr);
        server_ptr->xdg_shell_ptr = NULL;
    }

    if (NULL != server_ptr->wl_display_ptr) {
        wl_display_destroy_clients(server_ptr->wl_display_ptr);
        server_ptr->wl_display_ptr = NULL;
    }

    if (NULL != server_ptr->idle_monitor_ptr) {
        wlmaker_idle_monitor_destroy(server_ptr->idle_monitor_ptr);
        server_ptr->idle_monitor_ptr = NULL;
    }

    if (NULL != server_ptr->lock_mgr_ptr) {
        wlmaker_lock_mgr_destroy(server_ptr->lock_mgr_ptr);
        server_ptr->lock_mgr_ptr = NULL;
    }

    if (NULL != server_ptr->root_ptr) {
        wlmtk_util_disconnect_listener(
            &server_ptr->unclaimed_button_event_listener);
        wlmtk_root_destroy(server_ptr->root_ptr);
        server_ptr->root_ptr = NULL;
    }

    if (NULL != server_ptr->env_ptr) {
        wlmtk_env_destroy(server_ptr->env_ptr);
        server_ptr->env_ptr = NULL;
    }

    if (NULL != server_ptr->cursor_ptr) {
        wlmaker_cursor_destroy(server_ptr->cursor_ptr);
        server_ptr->cursor_ptr = NULL;
    }

    if (NULL != server_ptr->backend_ptr) {
        wlmbe_backend_destroy(server_ptr->backend_ptr);
        server_ptr->backend_ptr = NULL;
    }

    if (NULL != server_ptr->wl_display_ptr) {
        wl_display_destroy(server_ptr->wl_display_ptr);
        server_ptr->wl_display_ptr = NULL;
    }

    if (NULL != server_ptr->config_dict_ptr) {
        bspl_dict_unref(server_ptr->config_dict_ptr);
        server_ptr->config_dict_ptr = NULL;
    }

    free(server_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_server_activate_task_list(wlmaker_server_t *server_ptr)
{
    server_ptr->task_list_enabled = true;
    wl_signal_emit(&server_ptr->task_list_enabled_event, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmaker_server_deactivate_task_list(wlmaker_server_t *server_ptr)
{
    if (!server_ptr->task_list_enabled) return;

    server_ptr->task_list_enabled = false;
    wl_signal_emit(&server_ptr->task_list_disabled_event, NULL);

    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(server_ptr->root_ptr);
    wlmtk_window_t *window_ptr =
        wlmtk_workspace_get_activated_window(workspace_ptr);
    if (NULL != window_ptr) {
        wlmtk_workspace_raise_window(workspace_ptr, window_ptr);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmaker_server_get_output_at_cursor(
    wlmaker_server_t *server_ptr)
{
    return wlr_output_layout_output_at(
        server_ptr->wlr_output_layout_ptr,
        server_ptr->cursor_ptr->wlr_cursor_ptr->x,
        server_ptr->cursor_ptr->wlr_cursor_ptr->y);
}

/* ------------------------------------------------------------------------- */
wlmaker_key_binding_t *wlmaker_server_bind_key(
    wlmaker_server_t *server_ptr,
    const wlmaker_key_combo_t *key_combo_ptr,
    wlmaker_keybinding_callback_t callback)
{
    wlmaker_key_binding_t *key_binding_ptr = logged_calloc(
        1, sizeof(wlmaker_key_binding_t));
    if (NULL == key_binding_ptr) return NULL;

    key_binding_ptr->key_combo_ptr = key_combo_ptr;
    key_binding_ptr->callback = callback;
    bs_dllist_push_back(&server_ptr->bindings, &key_binding_ptr->dlnode);
    return key_binding_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_server_unbind_key(
    wlmaker_server_t *server_ptr,
    wlmaker_key_binding_t *key_binding_ptr)
{
    bs_dllist_remove(&server_ptr->bindings, &key_binding_ptr->dlnode);
    free(key_binding_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_keyboard_process_bindings(
    wlmaker_server_t *server_ptr,
    xkb_keysym_t keysym,
    uint32_t modifiers)
{
    if (bs_will_log(BS_DEBUG)) {
        char keysym_name[128] = {};
        xkb_keysym_get_name(keysym, keysym_name, sizeof(keysym_name));
        bs_log(BS_DEBUG, "Process key '%s' (sym %d, modifiers %"PRIx32")",
               keysym_name, keysym, modifiers);
    }

    for (bs_dllist_node_t *dlnode_ptr = server_ptr->bindings.head_ptr;
         NULL != dlnode_ptr;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmaker_key_binding_t *key_binding_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmaker_key_binding_t, dlnode);
        const wlmaker_key_combo_t *key_combo_ptr =
            key_binding_ptr->key_combo_ptr;

        uint32_t mask = key_combo_ptr->modifiers_mask;
        if (!mask) mask = UINT32_MAX;
        if ((modifiers & mask) != key_combo_ptr->modifiers) continue;

        xkb_keysym_t bound_ks = key_combo_ptr->keysym;
        if (!key_combo_ptr->ignore_case && keysym != bound_ks) continue;

        if (key_combo_ptr->ignore_case &&
            keysym != xkb_keysym_to_lower(bound_ks) &&
            keysym != xkb_keysym_to_upper(bound_ks)) continue;

        if (key_binding_ptr->callback(key_combo_ptr)) return true;
    }
    return false;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Registers the input device at |handle_ptr| with |server_ptr|.
 *
 * @param server_ptr
 * @param wlr_input_device_ptr
 * @param handle_ptr
 *
 * @return true on success.
 */
bool register_input_device(wlmaker_server_t *server_ptr,
                           struct wlr_input_device *wlr_input_device_ptr,
                           void *handle_ptr)
{
    wlmaker_input_device_t *input_device_ptr = logged_calloc(
        1, sizeof(wlmaker_input_device_t));
    if (NULL == input_device_ptr) return false;

    input_device_ptr->server_ptr = server_ptr;
    input_device_ptr->wlr_input_device_ptr = wlr_input_device_ptr;
    input_device_ptr->handle_ptr = handle_ptr;

    wlmtk_util_connect_listener_signal(
        &wlr_input_device_ptr->events.destroy,
        &input_device_ptr->destroy_listener,
        handle_destroy_input_device);

    bs_dllist_push_back(&server_ptr->input_devices, &input_device_ptr->node);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handler for the `new_input` signal raised by `wlr_backend`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_input_device(struct wl_listener *listener_ptr, void *data_ptr)
{
    struct wlr_input_device *wlr_input_device_ptr = data_ptr;
    wlmaker_server_t *server_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_server_t, backend_new_input_device_listener);

    wlmaker_keyboard_t *keyboard_ptr;
    switch (wlr_input_device_ptr->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        keyboard_ptr = wlmaker_keyboard_create(
            server_ptr,
            wlr_keyboard_from_input_device(wlr_input_device_ptr),
            server_ptr->wlr_seat_ptr);
        if (NULL != keyboard_ptr) {
            if (!register_input_device(
                    server_ptr, wlr_input_device_ptr, (void*)keyboard_ptr)) {
                bs_log(BS_ERROR, "Failed wlmaker_server_keyboard_register()");
                wlmaker_keyboard_destroy(keyboard_ptr);
            }
        }
        break;

    case WLR_INPUT_DEVICE_POINTER:
    case WLR_INPUT_DEVICE_TOUCH:
    case WLR_INPUT_DEVICE_TABLET_PAD:
        wlmaker_cursor_attach_input_device(
            server_ptr->cursor_ptr,
            wlr_input_device_ptr);
        break;

    default:
        bs_log(BS_INFO, "Server %p: Unhandled new input device type %d",
               server_ptr, wlr_input_device_ptr->type);
    }

    // If the KEYBOARD capability isn't set, keys won't be forwarded...
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    for (bs_dllist_node_t *node_ptr = server_ptr->input_devices.head_ptr;
         node_ptr != NULL;
         node_ptr = node_ptr->next_ptr) {
        if (((wlmaker_input_device_t*)node_ptr)->wlr_input_device_ptr->type ==
            WLR_INPUT_DEVICE_KEYBOARD) {
            capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
        }
    }
    wlr_seat_set_capabilities(server_ptr->wlr_seat_ptr, capabilities);
}

/* ------------------------------------------------------------------------- */
/** Handler for the `destroy` signal raised by `wlr_input_device`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy_input_device(struct wl_listener *listener_ptr,
                                 __UNUSED__ void *data_ptr)
{
    wlmaker_input_device_t *input_device_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_input_device_t, destroy_listener);

    wlmaker_keyboard_t *keyboard_ptr;
    switch (input_device_ptr->wlr_input_device_ptr->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        keyboard_ptr = (wlmaker_keyboard_t*)input_device_ptr->handle_ptr;
        wlmaker_keyboard_destroy(keyboard_ptr);
        break;

    default:
        break;
    }

    wl_list_remove(&input_device_ptr->destroy_listener.link);
    bs_dllist_remove(&input_device_ptr->server_ptr->input_devices,
                     &input_device_ptr->node);
    free(input_device_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles unclaimed button events: Right 'down' opens root menu. */
void _wlmaker_server_unclaimed_button_event_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_server_t *server_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_server_t, unclaimed_button_event_listener);
    wlmtk_button_event_t *button_event_ptr = data_ptr;

    if (BTN_RIGHT == button_event_ptr->button &&
        WLMTK_BUTTON_DOWN == button_event_ptr->type &&
        NULL != server_ptr->root_menu_ptr &&
        // TODO(kaeser@gubbe.ch): Clean up.
        !wlmtk_menu_is_open(
            wlmaker_root_menu_menu(server_ptr->root_menu_ptr))) {
        wlmtk_workspace_map_window(
            wlmtk_root_get_current_workspace(server_ptr->root_ptr),
            wlmaker_root_menu_window(server_ptr->root_menu_ptr));
        wlmtk_window_set_position(
            wlmaker_root_menu_window(server_ptr->root_menu_ptr),
            server_ptr->cursor_ptr->wlr_cursor_ptr->x,
            server_ptr->cursor_ptr->wlr_cursor_ptr->y);
        wlmtk_workspace_confine_within(
            wlmtk_root_get_current_workspace(server_ptr->root_ptr),
            wlmaker_root_menu_window(server_ptr->root_menu_ptr));
        wlmtk_menu_set_mode(
            wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
            WLMTK_MENU_MODE_RIGHTCLICK);
        wlmtk_menu_set_open(
            wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
            true);
    }
}

/* == Unit tests =========================================================== */

static void test_bind(bs_test_t *test_ptr);

/** Test cases for the server. */
const bs_test_case_t          wlmaker_server_test_cases[] = {
    { 1, "bind", test_bind },
    { 0, NULL, NULL }
};

/** Test helper: Callback for a keybinding. */
bool test_binding_callback(
    __UNUSED__ const wlmaker_key_combo_t *key_combo_ptr) {
    return true;
}

/* ------------------------------------------------------------------------- */
/** Tests key bindings. */
void test_bind(bs_test_t *test_ptr)
{
    wlmaker_server_t          srv = {};
    wlmaker_key_combo_t      binding_a = {
        .modifiers = WLR_MODIFIER_CTRL,
        .modifiers_mask = WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT,
        .keysym = XKB_KEY_A,
        .ignore_case = true
    };
    wlmaker_key_combo_t      binding_b = {
        .keysym = XKB_KEY_b
    };

    wlmaker_key_binding_t *kb1_ptr = wlmaker_server_bind_key(
        &srv, &binding_a, test_binding_callback);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, kb1_ptr);
    wlmaker_key_binding_t *kb2_ptr = wlmaker_server_bind_key(
        &srv, &binding_b, test_binding_callback);
        BS_TEST_VERIFY_NEQ(test_ptr, NULL, kb2_ptr);

    // First binding. Ctrl-A, permitting other modifiers except Ctrl.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmaker_keyboard_process_bindings(&srv, XKB_KEY_A, WLR_MODIFIER_CTRL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmaker_keyboard_process_bindings(&srv, XKB_KEY_a, WLR_MODIFIER_CTRL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmaker_keyboard_process_bindings(
            &srv, XKB_KEY_a, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT));

    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmaker_keyboard_process_bindings(
            &srv, XKB_KEY_a, WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmaker_keyboard_process_bindings(&srv, XKB_KEY_A, 0));

    // Second binding. Triggers only on lower-case 'b'.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmaker_keyboard_process_bindings(&srv, XKB_KEY_b, 0));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmaker_keyboard_process_bindings(&srv, XKB_KEY_B, 0));

    wlmaker_server_unbind_key(&srv, kb2_ptr);
    wlmaker_server_unbind_key(&srv, kb1_ptr);
}

/* == End of server.c ====================================================== */
