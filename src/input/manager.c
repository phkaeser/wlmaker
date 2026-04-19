/* ========================================================================= */
/**
 * @file manager.c
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

#include "manager.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdlib.h>
#include <toolkit/toolkit.h>
#include <wayland-server-protocol.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
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

    /** The last-used group of the keyboard layout. Used when using groups. */
    uint32_t                  last_keyboard_group_index;

    /** Pointer configuration. */
    struct wlmim_pointer_param pointer_params;

    /** Cursor handle. */
    wlmim_cursor_t            *cursor_ptr;

    /** wlroots seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** Reference to the config dict. */
    bspl_dict_t               *config_dict_ptr;
    /** The compositor's root. */
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
static void _wlmim_handle_destroy_input_device(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmim_device_register(
    wlmim_t *input_manager_ptr,
    struct wlr_input_device *wlr_input_device_ptr,
    void *handle_ptr);
static void _wlmim_device_unregister(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmim_device_update_capabilities(
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

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmim_t *wlmim_input_manager_create(
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
    input_manager_ptr->config_dict_ptr = BS_ASSERT_NOTNULL(
        bspl_dict_ref(config_dict_ptr));
    input_manager_ptr->root_ptr = root_ptr;
    wl_signal_init(&input_manager_ptr->events.cursor_position_updated);
    wl_signal_init(&input_manager_ptr->events.activity);
    wl_signal_init(&input_manager_ptr->events.deactivate_task_list);

    if (!wlmim_pointer_parse_config(
            config_dict_ptr,
            &input_manager_ptr->pointer_params)) {
        wlmim_input_manager_destroy(input_manager_ptr);
        return NULL;
    }

    if (NULL != wlr_seat_ptr && NULL != wlr_output_layout_ptr) {
        input_manager_ptr->cursor_ptr = wlmim_cursor_create(
            input_manager_ptr,
            cursor_style_ptr,
            wlr_output_layout_ptr,
            wlr_seat_ptr,
            root_ptr);
        if (NULL == input_manager_ptr->cursor_ptr) {
            wlmim_input_manager_destroy(input_manager_ptr);
            return NULL;
        }
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

    bspl_dict_unref(input_manager_ptr->config_dict_ptr);
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
            input_manager_ptr->config_dict_ptr,
            wlr_keyboard_from_input_device(wlr_input_device_ptr),
            input_manager_ptr->wlr_seat_ptr,
            wlmtk_root_element(input_manager_ptr->root_ptr));
        if (NULL != handle_ptr) {
            if (!_wlmim_device_register(
                    input_manager_ptr,
                    wlr_input_device_ptr,
                    handle_ptr)) {
                bs_log(BS_ERROR, "Failed _wlim_device_register()");
                wlmim_keyboard_destroy(handle_ptr);
            }
        }
        break;

    case WLR_INPUT_DEVICE_TOUCH:
        break;

    case WLR_INPUT_DEVICE_TABLET_PAD:
        break;

    case WLR_INPUT_DEVICE_POINTER:
        handle_ptr = wlmim_pointer_create(
            wlr_input_device_ptr,
            &input_manager_ptr->pointer_params);
        if (NULL != handle_ptr) {
            if (!_wlmim_device_register(
                    input_manager_ptr,
                    wlr_input_device_ptr,
                    handle_ptr)) {
                bs_log(BS_ERROR, "Failed _wlim_device_register()");
                wlmim_keyboard_destroy(handle_ptr);
            } else if (wlmim_pointer_enabled(handle_ptr) &&
                       NULL != input_manager_ptr->cursor_ptr) {
                wlmim_cursor_attach_input_device(
                    input_manager_ptr->cursor_ptr,
                    wlr_input_device_ptr);
            }
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
 * @return true on success.
 */
bool _wlmim_device_register(
    wlmim_t *input_manager_ptr,
    struct wlr_input_device *wlr_input_device_ptr,
    void *handle_ptr)
{
    struct wlmim_device *device_ptr = logged_calloc(1, sizeof(*device_ptr));
    if (NULL == device_ptr) return false;

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
    return true;
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

/** Test cases for the input manager. */
static const bs_test_case_t   _wlmim_test_cases[] = {
    { true, "bind", _wlmim_test_bind },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmim_test_set = BS_TEST_SET(
    true, "server", _wlmim_test_cases);

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

/* == End of manager.c ===================================================== */
