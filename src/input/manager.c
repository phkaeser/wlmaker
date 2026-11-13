/* ========================================================================= */
/**
 * @file manager.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

#include "manager.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <toolkit/toolkit.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE
#include <xkbcommon/xkbcommon-keysyms.h>

#include "cursor.h"
#include "keyboard.h"
#include "pointer.h"

/* == Declarations ========================================================= */

/** Handle for the input manager. */
struct _wlmim_t {
    /** Events provided by the input manager. */
    struct wlmim_events       events;

    /** Holds multiple @ref wlmim_device through @ref wlmim_device::dlnode. */
    bs_dllist_t               devices;
    /** List of all bound keys, see @ref wlmim_keybinding_t::dlnode */
    bs_dllist_t               keybindings;

    /** Listener for `new_input` signals raised by `wlr_backend`. */
    struct wl_listener        backend_new_input_device_listener;
    /** Output layout associated with this input manager, or NULL. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Listener for `change` signals raised by `wlr_output_layout`. */
    struct wl_listener        output_layout_change_listener;
    /** Listener for `destroy` signals raised by `wlr_output_layout`. */
    struct wl_listener        output_layout_destroy_listener;

    /** The last-used group of the keyboard layout. Used when using groups. */
    uint32_t                  last_keyboard_group_index;

    /** Cursor configuration. */
    struct wlmim_cursor_options cursor_options;
    /** Keyboard configuration. */
    struct wlmim_keyboard_options keyboard_options;
    /** Pointer configuration. */
    struct wlmim_pointer_options pointer_options;

    /** Cursor handle. */
    wlmim_cursor_t            *cursor_ptr;

    /** Active XKB keymap (if dynamically set). */
    struct xkb_keymap         *xkb_keymap_ptr;

    /** wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The compositor's root wrapper. */
    wlmtk_root_t              *root_ptr;

};

/** Wraps an input device. */
struct wlmim_device {
    /** List node, as an element of @ref wlmim_t::devices. */
    bs_dllist_node_t          dlnode;

    /** The input device. */
    struct wlr_input_device   *wlr_input_device_ptr;
    /** Handle to the wlmaker actual device. */
    void                      *handle_ptr;
    /** Listener for the `destroy` signal of `wlr_input_device`. */
    struct wl_listener        destroy_listener;

    /** Back-link to input manager. */
    wlmim_t                   *input_manager_ptr;
};

/** Internal struct holding a keybinding. */
struct _wlmim_keybinding_t {
    /** Node within @ref wlmim_t::keybindings. */
    bs_dllist_node_t          dlnode;
    /** The key binding: Modifier and keysym to bind to. */
    const struct wlmim_keybinding_combo *keybinding_combo_ptr;
    /** Callback for when this modifier + key is encountered. */
    wlmim_keybinding_callback_t callback;
};

/** Argument to @ref _wlmim_process_keybinding. */
struct _wlmim_process_arg {
    /** Keysym. */
    xkb_keysym_t keysym;
    /** Modifiers currently set. */
    uint32_t modifiers;
};

static void _wlmim_handle_new_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_handle_output_layout_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmim_handle_destroy_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static struct wlmim_device *_wlmim_device_register(
    wlmim_t *input_manager_ptr,
    struct wlr_input_device *wlr_input_device_ptr,
    void *handle_ptr);
static void _wlmim_device_unregister(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static struct wlr_output *_wlmim_find_layout_output_by_name(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr);
static void _wlmim_device_sync_cursor(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmim_device_update_capabilities(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmim_device_configure_keyboard(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool _wlmim_process_keybinding(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmim_unbind_node(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/* == Data ================================================================= */

const uint32_t wlmim_modifiers_default_mask = (
    WLR_MODIFIER_SHIFT |
    // Excluding: WLR_MODIFIER_CAPS.
    WLR_MODIFIER_CTRL |
    WLR_MODIFIER_ALT |
    WLR_MODIFIER_MOD2 |
    WLR_MODIFIER_MOD3 |
    WLR_MODIFIER_LOGO |
    WLR_MODIFIER_MOD5);

/** Enum definition for input configuration sections. */
static const bspl_desc_t _wlmim_manager_config[] = {
    BSPL_DESC_DICT("Keyboard", false,
                   wlmim_t, keyboard_options, keyboard_options,
                   wlmim_keyboard_options_desc),
    BSPL_DESC_DICT("Touchpad", false,
                   wlmim_t, pointer_options, pointer_options,
                   wlmim_pointer_options_desc),
    BSPL_DESC_DICT("Cursor", false,
                   wlmim_t, cursor_options, cursor_options,
                   wlmim_cursor_options_desc),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmim_t *wlmim_input_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    bspl_dict_t *config_dict_ptr,
    const struct wlmim_cursor_style *cursor_style_ptr,
    wlmtk_root_t *root_ptr)
{
    wlmim_t *input_manager_ptr = logged_calloc(1, sizeof(*input_manager_ptr));
    if (NULL == input_manager_ptr) return NULL;
    input_manager_ptr->wlr_seat_ptr = wlr_seat_ptr;
    input_manager_ptr->root_ptr = root_ptr;
    wl_signal_init(&input_manager_ptr->events.cursor_position_updated);
    wl_signal_init(&input_manager_ptr->events.activity);
    wl_signal_init(&input_manager_ptr->events.deactivate_task_list);

    // Retrieve configuration, if given.
    input_manager_ptr->xkb_keymap_ptr = wlmim_keyboard_xkb_from_config(
        config_dict_ptr);
    if (NULL != config_dict_ptr && NULL == input_manager_ptr->xkb_keymap_ptr) {
        wlmim_input_manager_destroy(input_manager_ptr);
        return NULL;
    }

    if (!bspl_decode_dict(
            config_dict_ptr,
            _wlmim_manager_config,
            input_manager_ptr)) {
        wlmim_input_manager_destroy(input_manager_ptr);
        return NULL;
    }

    if (NULL != wlr_seat_ptr && NULL != wlr_output_layout_ptr) {
        input_manager_ptr->cursor_ptr = wlmim_cursor_create(
            wl_display_ptr,
            input_manager_ptr,
            cursor_style_ptr,
            &input_manager_ptr->cursor_options,
            wlr_output_layout_ptr,
            wlr_seat_ptr,
            root_ptr);
        if (NULL == input_manager_ptr->cursor_ptr) {
            wlmim_input_manager_destroy(input_manager_ptr);
            return NULL;
        }
    }

    if (NULL != wlr_output_layout_ptr) {
        input_manager_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
        wlmtk_util_connect_listener_signal(
            &wlr_output_layout_ptr->events.change,
            &input_manager_ptr->output_layout_change_listener,
            _wlmim_handle_output_layout_change);
        wlmtk_util_connect_listener_signal(
            &wlr_output_layout_ptr->events.destroy,
            &input_manager_ptr->output_layout_destroy_listener,
            _wlmim_handle_output_layout_destroy);
    }

    if (NULL != wlr_backend_ptr) {
        wlmtk_util_connect_listener_signal(
            &wlr_backend_ptr->events.new_input,
            &input_manager_ptr->backend_new_input_device_listener,
            _wlmim_handle_new_input_device);
    }

    return input_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmim_input_manager_destroy(wlmim_t *input_manager_ptr)
{
    wlmtk_util_disconnect_listener(
        &input_manager_ptr->backend_new_input_device_listener);
    _wlmim_handle_output_layout_destroy(
        &input_manager_ptr->output_layout_destroy_listener,
        NULL);

    bs_dllist_for_each(
        &input_manager_ptr->keybindings,
        _wlmim_unbind_node,
        input_manager_ptr);

    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_unregister,
        input_manager_ptr);

    if (NULL != input_manager_ptr->cursor_ptr) {
        wlmim_cursor_destroy(input_manager_ptr->cursor_ptr);
        input_manager_ptr->cursor_ptr = NULL;
    }

    if (NULL != input_manager_ptr->xkb_keymap_ptr) {
        xkb_keymap_unref(input_manager_ptr->xkb_keymap_ptr);
        input_manager_ptr->xkb_keymap_ptr = NULL;
    }

    free(input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlmim_events *wlmim_events(wlmim_t *input_manager_ptr)
{
    return &input_manager_ptr->events;
}

/* ------------------------------------------------------------------------- */
bool wlmim_set_style(
    wlmim_t *input_manager_ptr,
    const struct wlmim_cursor_style *style_ptr)
{
    if (NULL != input_manager_ptr->cursor_ptr) {
        return wlmim_cursor_set_style(
            input_manager_ptr->cursor_ptr,
            style_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
struct wlr_cursor *wlmim_wlr_cursor(wlmim_t *input_manager_ptr)
{
    return wlmim_cursor_wlr_cursor(input_manager_ptr->cursor_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmim_report_activity(wlmim_t *input_manager_ptr)
{
    wl_signal_emit(&input_manager_ptr->events.activity, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmim_set_keyboard_group_index(
    wlmim_t *input_manager_ptr,
    uint32_t index)
{
    input_manager_ptr->last_keyboard_group_index = index;
}

/* ------------------------------------------------------------------------- */
uint32_t wlmim_get_keyboard_group_index(
    wlmim_t *input_manager_ptr)
{
    return input_manager_ptr->last_keyboard_group_index;
}

/* ------------------------------------------------------------------------- */
void wlmim_input_manager_set_keymap(
    wlmim_t *input_manager_ptr,
    struct xkb_keymap *xkb_keymap_ptr)
{
    if (input_manager_ptr->xkb_keymap_ptr == xkb_keymap_ptr ||
        NULL == xkb_keymap_ptr) return;

    if (NULL != input_manager_ptr->xkb_keymap_ptr) {
        xkb_keymap_unref(input_manager_ptr->xkb_keymap_ptr);
    }
    input_manager_ptr->xkb_keymap_ptr = xkb_keymap_ptr;
    xkb_keymap_ref(input_manager_ptr->xkb_keymap_ptr);

    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_configure_keyboard,
        input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmim_input_manager_set_repeat_info(
    wlmim_t *input_manager_ptr,
    int32_t rate,
    int32_t delay)
{
    input_manager_ptr->keyboard_options.repeat.rate = rate;
    input_manager_ptr->keyboard_options.repeat.delay = delay;
    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_configure_keyboard,
        input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
wlmim_keybinding_t *wlmim_bind_key(
    wlmim_t *input_manager_ptr,
    const struct wlmim_keybinding_combo *keybinding_combo_ptr,
    wlmim_keybinding_callback_t callback)
{
    wlmim_keybinding_t *keybinding_ptr = logged_calloc(
        1, sizeof(*keybinding_ptr));
    if (NULL == keybinding_ptr) return NULL;

    keybinding_ptr->keybinding_combo_ptr = keybinding_combo_ptr;
    keybinding_ptr->callback = callback;
    bs_dllist_push_back(
        &input_manager_ptr->keybindings,
        &keybinding_ptr->dlnode);
    return keybinding_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmim_unbind_key(
    wlmim_t *input_manager_ptr,
    wlmim_keybinding_t *keybinding_ptr)
{
    bs_dllist_remove(
        &input_manager_ptr->keybindings,
        &keybinding_ptr->dlnode);
    free(keybinding_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmim_process_key(
    wlmim_t *im_ptr,
    xkb_keysym_t keysym,
    uint32_t modifiers)
{
    if (bs_will_log(BS_DEBUG)) {
        char keysym_name[128] = {};
        xkb_keysym_get_name(keysym, keysym_name, sizeof(keysym_name));
        bs_log(BS_DEBUG, "Process key '%s' (sym %d, modifiers %"PRIx32")",
               keysym_name, keysym, modifiers);
    }
    struct _wlmim_process_arg a = { .keysym = keysym, .modifiers = modifiers };
    return bs_dllist_any(&im_ptr->keybindings, _wlmim_process_keybinding, &a);
}

/* ------------------------------------------------------------------------- */
const struct wlmim_cursor_options *wlmim_cursor_options(
    wlmim_t *input_manager_ptr)
{
    return &input_manager_ptr->cursor_options;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_input` signal raised by `wlr_backend`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmim_handle_new_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    struct wlr_input_device *wlr_input_device_ptr = data_ptr;
    wlmim_t *input_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_t, backend_new_input_device_listener);

    void *handle_ptr = NULL;
    switch (wlr_input_device_ptr->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        handle_ptr = wlmim_keyboard_create(
            input_manager_ptr,
            input_manager_ptr->xkb_keymap_ptr,
            &input_manager_ptr->keyboard_options,
            wlr_keyboard_from_input_device(wlr_input_device_ptr),
            input_manager_ptr->wlr_seat_ptr,
            wlmtk_root_element(input_manager_ptr->root_ptr));
        if (NULL == handle_ptr) {
            // Abort if we cannot create the keyboard for the device.
            bs_log(BS_FATAL, "Failed wlmim_keyboard_create()");
            break;
        }

        if (NULL == _wlmim_device_register(
                input_manager_ptr,
                wlr_input_device_ptr,
                handle_ptr)) {
            bs_log(BS_FATAL, "Failed _wlim_device_register()");
            wlmim_keyboard_destroy(handle_ptr);
        }
        break;

    case WLR_INPUT_DEVICE_TOUCH:
        break;

    case WLR_INPUT_DEVICE_TABLET_PAD:
        break;

    case WLR_INPUT_DEVICE_POINTER:
        handle_ptr = wlmim_pointer_create(
            wlr_input_device_ptr,
            &input_manager_ptr->pointer_options);
        if (NULL == handle_ptr) {
            // Abort if we cannot create the keyboard for the device.
            bs_log(BS_FATAL, "Failed wlmim_pointer_create()");
            break;
        }

        struct wlmim_device *device_ptr = _wlmim_device_register(
            input_manager_ptr,
            wlr_input_device_ptr,
            handle_ptr);
        if (NULL == device_ptr) {
            bs_log(BS_ERROR, "Failed _wlim_device_register()");
            wlmim_pointer_destroy(handle_ptr);
        } else {
            _wlmim_device_sync_cursor(&device_ptr->dlnode, input_manager_ptr);
        }
        break;

    case WLR_INPUT_DEVICE_SWITCH:
        break;

    default:
        bs_log(BS_INFO, "Input manager %p: Unhandled new input device type %d",
               input_manager_ptr,wlr_input_device_ptr->type);
    }

    // If the KEYBOARD capability isn't set, keys won't be forwarded...
    uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER;
    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_update_capabilities,
        &capabilities);
    wlr_seat_set_capabilities(input_manager_ptr->wlr_seat_ptr, capabilities);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `change` signal raised by `wlr_output_layout`.
 *
 * Re-evaluates output-bound pointer devices against the updated output layout.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmim_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmim_t *input_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_t, output_layout_change_listener);

    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_sync_cursor,
        input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal raised by `wlr_output_layout`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmim_handle_output_layout_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmim_t *input_manager_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmim_t, output_layout_destroy_listener);
    if (NULL == input_manager_ptr->wlr_output_layout_ptr) return;

    wlmtk_util_disconnect_listener(
        &input_manager_ptr->output_layout_change_listener);
    wlmtk_util_disconnect_listener(
        &input_manager_ptr->output_layout_destroy_listener);
    input_manager_ptr->wlr_output_layout_ptr = NULL;

    bs_dllist_for_each(
        &input_manager_ptr->devices,
        _wlmim_device_sync_cursor,
        input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal raised by `wlr_input_device`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmim_handle_destroy_input_device(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    struct wlmim_device *device_ptr = BS_CONTAINER_OF(
        listener_ptr, struct wlmim_device, destroy_listener);
    _wlmim_device_unregister(
        &device_ptr->dlnode,
        device_ptr->input_manager_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates and registers the device node.
 *
 * @param input_manager_ptr
 * @param wlr_input_device_ptr
 * @param handle_ptr
 *
 * @return Pointer to the registered @ref wlmim_device, or NULL on error.
 */
struct wlmim_device *_wlmim_device_register(
    wlmim_t *input_manager_ptr,
    struct wlr_input_device *wlr_input_device_ptr,
    void *handle_ptr)
{
    struct wlmim_device *device_ptr = logged_calloc(1, sizeof(*device_ptr));
    if (NULL == device_ptr) return NULL;

    device_ptr->input_manager_ptr = input_manager_ptr;
    device_ptr->wlr_input_device_ptr = wlr_input_device_ptr;
    device_ptr->handle_ptr = handle_ptr;
    wlmtk_util_connect_listener_signal(
        &wlr_input_device_ptr->events.destroy,
        &device_ptr->destroy_listener,
        _wlmim_handle_destroy_input_device);

    bs_dllist_push_back(
        &input_manager_ptr->devices,
        &device_ptr->dlnode);
    return device_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Unregisters and destroys the device node.
 *
 * @param dlnode_ptr
 * @param ud_ptr
 */
void _wlmim_device_unregister(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlmim_device *device_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmim_device, dlnode);
    wlmim_t *input_manager_ptr = ud_ptr;

    switch (device_ptr->wlr_input_device_ptr->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        wlmim_keyboard_destroy(device_ptr->handle_ptr);
        break;

    case WLR_INPUT_DEVICE_POINTER:
        if (wlmim_pointer_enabled(device_ptr->handle_ptr) &&
            NULL != input_manager_ptr->cursor_ptr) {
            wlmim_cursor_detach_input_device(
                input_manager_ptr->cursor_ptr,
                device_ptr->wlr_input_device_ptr);
        }
        wlmim_pointer_destroy(device_ptr->handle_ptr);
        break;
    default:
        break;
    }

    bs_dllist_remove(
        &device_ptr->input_manager_ptr->devices,
        &device_ptr->dlnode);
    wlmtk_util_disconnect_listener(&device_ptr->destroy_listener);
    free(device_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Looks up an output in `wlr_output_layout_ptr` matching `name_ptr`.
 *
 * @param wlr_output_layout_ptr
 * @param name_ptr
 *
 * @return Matching `struct wlr_output`, or NULL if not found.
 */
struct wlr_output *_wlmim_find_layout_output_by_name(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr)
{
    if (NULL == wlr_output_layout_ptr || NULL == name_ptr) return NULL;

    struct wlr_output_layout_output *wlr_output_layout_output_ptr;
    wl_list_for_each(
        wlr_output_layout_output_ptr,
        &wlr_output_layout_ptr->outputs,
        link) {
        struct wlr_output *wlr_output_ptr =
            wlr_output_layout_output_ptr->output;
        if (NULL != wlr_output_ptr->name &&
            0 == strcmp(wlr_output_ptr->name, name_ptr)) {
            return wlr_output_ptr;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Attaches or detaches a pointer device on the cursor depending on whether its
 * `output_name` (if set) is present in `wlr_output_layout_ptr`.
 *
 * @param dlnode_ptr
 * @param ud_ptr              Points to @ref wlmim_t.
 */
void _wlmim_device_sync_cursor(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlmim_device *device_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmim_device, dlnode);
    wlmim_t *input_manager_ptr = ud_ptr;

    if (device_ptr->wlr_input_device_ptr->type != WLR_INPUT_DEVICE_POINTER ||
        !wlmim_pointer_enabled(device_ptr->handle_ptr) ||
        NULL == input_manager_ptr->cursor_ptr) {
        return;
    }

    struct wlr_pointer *wlr_pointer_ptr = wlr_pointer_from_input_device(
        device_ptr->wlr_input_device_ptr);
    if (NULL == wlr_pointer_ptr->output_name) {
        wlmim_cursor_attach_input_device(
            input_manager_ptr->cursor_ptr,
            device_ptr->wlr_input_device_ptr,
            NULL);
        return;
    }

    struct wlr_output *wlr_output_ptr = _wlmim_find_layout_output_by_name(
        input_manager_ptr->wlr_output_layout_ptr,
        wlr_pointer_ptr->output_name);
    if (NULL != wlr_output_ptr) {
        wlmim_cursor_attach_input_device(
            input_manager_ptr->cursor_ptr,
            device_ptr->wlr_input_device_ptr,
            wlr_output_ptr);
    } else {
        wlmim_cursor_detach_input_device(
            input_manager_ptr->cursor_ptr,
            device_ptr->wlr_input_device_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the capabilities bitmask. Iterator for `bs_dllist_for_each`.
 *
 * @param dlnode_ptr
 * @param ud_ptr              Points to a uint32_t capabilities bitmask.
 */
void _wlmim_device_update_capabilities(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    uint32_t *capabilities_ptr = ud_ptr;
    struct wlmim_device *device_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmim_device, dlnode);

    if (device_ptr->wlr_input_device_ptr->type == WLR_INPUT_DEVICE_KEYBOARD) {
        *capabilities_ptr |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Configures devices of type keyboard. Iterator for `bs_dllist_for_each`.
 *
 * @param dlnode_ptr
 * @param ud_ptr              Points to a @ref wlmim_t.
 */
void _wlmim_device_configure_keyboard(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlmim_device *device_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmim_device, dlnode);
    wlmim_t *input_manager_ptr = ud_ptr;

    if (device_ptr->wlr_input_device_ptr->type == WLR_INPUT_DEVICE_KEYBOARD) {
        wlmim_keyboard_configure(
            device_ptr->handle_ptr,
            input_manager_ptr->xkb_keymap_ptr,
            &input_manager_ptr->keyboard_options);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Process one keybinding, as iterator to `bs_dllist_all`.
 *
 * @param dlnode_ptr
 * @param ud_ptr
 *
 * @return true if the keybinding matched, and the callback returned true.
 */
bool _wlmim_process_keybinding(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmim_keybinding_t *keybinding_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmim_keybinding_t, dlnode);
    struct _wlmim_process_arg *arg_ptr = ud_ptr;

    const struct wlmim_keybinding_combo *keybinding_combo_ptr =
        keybinding_ptr->keybinding_combo_ptr;

    uint32_t mask = keybinding_combo_ptr->modifiers_mask;
    if (!mask) mask = UINT32_MAX;
    xkb_keysym_t bound_ks = keybinding_combo_ptr->keysym;

    if ((arg_ptr->modifiers & mask) !=  keybinding_combo_ptr->modifiers) {
        return false;
    }
    if (!keybinding_combo_ptr->ignore_case && arg_ptr->keysym != bound_ks) {
        return false;
    }
    if (keybinding_combo_ptr->ignore_case &&
        arg_ptr->keysym != xkb_keysym_to_lower(bound_ks) &&
        arg_ptr->keysym != xkb_keysym_to_upper(bound_ks)) return false;
    return keybinding_ptr->callback(keybinding_combo_ptr);
}

/* ------------------------------------------------------------------------- */
/** Unbinds keybinding at the node, as iterator to `bs_dllist_for_each`. */
void _wlmim_unbind_node(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmim_keybinding_t *keybinding_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmim_keybinding_t, dlnode);
    wlmim_unbind_key(ud_ptr, keybinding_ptr);
}

/* == Unit Tests =========================================================== */

static void _wlmim_test_bind(bs_test_t *test_ptr);
static void _wlmim_test_pointer_output_filter(bs_test_t *test_ptr);

/** Test cases for the input manager. */
static const bs_test_case_t   _wlmim_test_cases[] = {
    { true, "bind", _wlmim_test_bind },
    { true, "pointer_output_filter", _wlmim_test_pointer_output_filter },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmim_test_set = BS_TEST_SET(
    true, "manager", _wlmim_test_cases);

/** Test helper: Callback for a keybinding. */
bool _wlmim_test_binding_callback(
    __UNUSED__ const struct wlmim_keybinding_combo *keybinding_combo_ptr) {
    return true;
}

/* ------------------------------------------------------------------------- */
/** Tests key bindings. */
void _wlmim_test_bind(bs_test_t *test_ptr)
{
    wlmim_t im = {};
    struct wlmim_keybinding_combo binding_a = {
        .modifiers = WLR_MODIFIER_CTRL,
        .modifiers_mask = WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT,
        .keysym = XKB_KEY_A,
        .ignore_case = true
    };
    struct wlmim_keybinding_combo binding_b = {
        .keysym = XKB_KEY_b
    };

    wlmim_keybinding_t *kb1_ptr = wlmim_bind_key(
        &im, &binding_a, _wlmim_test_binding_callback);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, kb1_ptr);
    wlmim_keybinding_t *kb2_ptr = wlmim_bind_key(
        &im, &binding_b, _wlmim_test_binding_callback);
        BS_TEST_VERIFY_NEQ(test_ptr, NULL, kb2_ptr);

    // First binding. Ctrl-A, permitting other modifiers except Ctrl.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmim_process_key(&im, XKB_KEY_A, WLR_MODIFIER_CTRL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmim_process_key(&im, XKB_KEY_a, WLR_MODIFIER_CTRL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmim_process_key(
            &im, XKB_KEY_a, WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT));

    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmim_process_key(
            &im, XKB_KEY_a, WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmim_process_key(&im, XKB_KEY_A, 0));

    // Second binding. Triggers only on lower-case 'b'.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmim_process_key(&im, XKB_KEY_b, 0));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmim_process_key(&im, XKB_KEY_B, 0));

    wlmim_unbind_key(&im, kb2_ptr);
    wlmim_unbind_key(&im, kb1_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests filtering output-bound pointers across separate output layouts. */
void _wlmim_test_pointer_output_filter(bs_test_t *test_ptr)
{
    static const struct wlr_pointer_impl test_pointer_impl = {
        .name = "test-pointer",
    };
    struct wlmim_cursor_style cursor_style = {
        .override_system_configuration = true,
        .name_ptr = "default",
        .size = 24,
    };

    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_backend *backend_ptr = wlr_headless_backend_create(
        wl_display_get_event_loop(display_ptr));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, backend_ptr);
    struct wlr_renderer *renderer_ptr = wlr_renderer_autocreate(backend_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, renderer_ptr);
    struct wlr_allocator *allocator_ptr = wlr_allocator_autocreate(
        backend_ptr, renderer_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, allocator_ptr);

    struct wlr_output *output1_ptr = wlr_headless_add_output(
        backend_ptr, 200, 100);
    struct wlr_output *output2_ptr = wlr_headless_add_output(
        backend_ptr, 200, 100);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, output1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, output2_ptr);
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        wlr_output_init_render(output1_ptr, allocator_ptr, renderer_ptr));
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        wlr_output_init_render(output2_ptr, allocator_ptr, renderer_ptr));

    struct wlr_output_layout *layout1_ptr = wlr_output_layout_create(
        display_ptr);
    struct wlr_output_layout *layout2_ptr = wlr_output_layout_create(
        display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layout1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, layout2_ptr);

    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_element_set_visible(&fe1_ptr->element, true);
    wlmtk_element_set_visible(&fe2_ptr->element, true);
    wlmtk_fake_element_set_dimensions(fe1_ptr, 200, 100);
    wlmtk_fake_element_set_dimensions(fe2_ptr, 200, 100);

    wlmtk_root_t *root1_ptr = wlmtk_root_create(&fe1_ptr->element, layout1_ptr);
    wlmtk_root_t *root2_ptr = wlmtk_root_create(&fe2_ptr->element, layout2_ptr);
    struct wlr_seat *seat1_ptr = wlr_seat_create(display_ptr, "seat1");
    struct wlr_seat *seat2_ptr = wlr_seat_create(display_ptr, "seat2");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root2_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, seat1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, seat2_ptr);

    // Mirror wlmdock + jetty creation order:
    // 1. First input manager created; pointer1 new_input fires BEFORE output1
    //    is added to layout1.
    wlmim_t *im1_ptr = wlmim_input_manager_create(
        NULL, backend_ptr, layout1_ptr, seat1_ptr,
        NULL, &cursor_style, root1_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, im1_ptr);

    struct wlr_pointer pointer1;
    wlr_pointer_init(&pointer1, &test_pointer_impl, "ptr1");
    pointer1.output_name = logged_strdup(output1_ptr->name);
    wl_signal_emit_mutable(&backend_ptr->events.new_input, &pointer1.base);
    wlr_output_layout_add_auto(layout1_ptr, output1_ptr);

    // 2. Second input manager created; pointer2 new_input fires while BOTH
    //    input managers listen, BEFORE output2 is added to layout2.
    wlmim_t *im2_ptr = wlmim_input_manager_create(
        NULL, backend_ptr, layout2_ptr, seat2_ptr,
        NULL, &cursor_style, root2_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, im2_ptr);

    struct wlr_pointer pointer2;
    wlr_pointer_init(&pointer2, &test_pointer_impl, "ptr2");
    pointer2.output_name = logged_strdup(output2_ptr->name);
    wl_signal_emit_mutable(&backend_ptr->events.new_input, &pointer2.base);
    wlr_output_layout_add_auto(layout2_ptr, output2_ptr);

    // Motion + click on pointer2 must reach ONLY root2 / fe2, NOT fe1.
    struct wlr_pointer_motion_absolute_event motion2 = {
        .pointer = &pointer2, .time_msec = 10, .x = 0.5, .y = 0.5,
    };
    wl_signal_emit_mutable(&pointer2.events.motion_absolute, &motion2);
    struct wlr_pointer_button_event button2 = {
        .pointer = &pointer2,
        .time_msec = 11,
        .button = BTN_LEFT,
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
    };
    wlr_pointer_notify_button(&pointer2, &button2);
    BS_TEST_VERIFY_TRUE(test_ptr, fe2_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_EQ(
        test_ptr, BTN_LEFT, fe2_ptr->pointer_button_event.button);
    BS_TEST_VERIFY_FALSE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, fe1_ptr->pointer_button_event.button);

    // Motion + click on pointer1 must reach ONLY root1 / fe1, leaving fe2.
    struct wlr_pointer_motion_absolute_event motion1 = {
        .pointer = &pointer1, .time_msec = 20, .x = 0.25, .y = 0.25,
    };
    wl_signal_emit_mutable(&pointer1.events.motion_absolute, &motion1);
    struct wlr_pointer_button_event button1 = {
        .pointer = &pointer1,
        .time_msec = 21,
        .button = BTN_RIGHT,
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
    };
    wlr_pointer_notify_button(&pointer1, &button1);
    BS_TEST_VERIFY_TRUE(test_ptr, fe1_ptr->pointer_accepts_motion_called);
    BS_TEST_VERIFY_EQ(
        test_ptr, BTN_RIGHT, fe1_ptr->pointer_button_event.button);
    BS_TEST_VERIFY_EQ(
        test_ptr, BTN_LEFT, fe2_ptr->pointer_button_event.button);

    // Removing output2 from layout2 detaches pointer2 from im2's cursor.
    wlr_output_layout_remove(layout2_ptr, output2_ptr);
    struct wlr_pointer_button_event button2_after_remove = {
        .pointer = &pointer2,
        .time_msec = 30,
        .button = BTN_MIDDLE,
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
    };
    wlr_pointer_notify_button(&pointer2, &button2_after_remove);
    BS_TEST_VERIFY_EQ(
        test_ptr, BTN_LEFT, fe2_ptr->pointer_button_event.button);

    // Unbound pointer (output_name == NULL) still attaches to both cursors.
    struct wlr_pointer pointer_unbound;
    wlr_pointer_init(&pointer_unbound, &test_pointer_impl, "ptr-unbound");
    wl_signal_emit_mutable(
        &backend_ptr->events.new_input, &pointer_unbound.base);
    struct wlr_pointer_button_event button_unbound = {
        .pointer = &pointer_unbound,
        .time_msec = 40,
        .button = BTN_SIDE,
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
    };
    wlr_pointer_notify_button(&pointer_unbound, &button_unbound);
    BS_TEST_VERIFY_EQ(
        test_ptr, BTN_SIDE, fe1_ptr->pointer_button_event.button);

    wlr_pointer_finish(&pointer_unbound);
    wlr_pointer_finish(&pointer2);
    wlr_pointer_finish(&pointer1);

    wlmim_input_manager_destroy(im2_ptr);
    wlmim_input_manager_destroy(im1_ptr);
    wlmtk_root_destroy(root2_ptr);
    wlmtk_root_destroy(root1_ptr);
    wlmtk_element_destroy(&fe2_ptr->element);
    wlmtk_element_destroy(&fe1_ptr->element);
    wlr_backend_destroy(backend_ptr);
    wlr_allocator_destroy(allocator_ptr);
    wlr_renderer_destroy(renderer_ptr);
    wl_display_destroy(display_ptr);
}

/* == End of manager.c ===================================================== */
