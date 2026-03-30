/* ========================================================================= */
/**
 * @file pointer.c
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

#include "pointer.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <libinput.h>
#include <stddef.h>
#include <stdlib.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Pointer handle */
struct _wlmim_pointer_t {
    /** The WLR input device this pointer corresponds to. */
    struct wlr_input_device   *wlr_input_device_ptr;
    /** Whether this pointer device is enabled. */
    bool                      enabled;
};

/* == Data ================================================================= */

/** Enum values for "ClickMethod" */
static const bspl_enum_desc_t _wlmim_pointer_click_methods[] = {
    BSPL_ENUM("None", LIBINPUT_CONFIG_CLICK_METHOD_NONE),
    BSPL_ENUM("ButtonAreas", LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS),
    BSPL_ENUM("ClickFinger", LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER),
    BSPL_ENUM_SENTINEL()
};

/** Enum values for "ScrollMethod" */
static const bspl_enum_desc_t _wlmim_pointer_scroll_methods[] = {
    BSPL_ENUM("NoScroll", LIBINPUT_CONFIG_SCROLL_NO_SCROLL),
    BSPL_ENUM("TwoFingers", LIBINPUT_CONFIG_SCROLL_2FG),
    BSPL_ENUM("Edge", LIBINPUT_CONFIG_SCROLL_EDGE),
    BSPL_ENUM("OnButtonDown", LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN),
    BSPL_ENUM_SENTINEL()
};

/** Plist descriptor for the "Touchpad" section. */
static const bspl_desc_t _wlmim_pointer_config_touchpad[] = {
    BSPL_DESC_BOOL(
        "Enabled",
        false,
        struct wlmim_pointer_param,
        touchpad.enabled,
        touchpad.enabled,
        true),
    BSPL_DESC_BOOL(
        "DisableWhileTyping",
        false,
        struct wlmim_pointer_param,
        touchpad.disable_while_typing,
        touchpad.disable_while_typing,
        true),
    BSPL_DESC_ENUM(
        "ClickMethod",
        false,
        struct wlmim_pointer_param,
        touchpad.click_method,
        touchpad.click_method,
        LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER,
        _wlmim_pointer_click_methods),
    BSPL_DESC_BOOL(
        "TapToClick",
        false,
        struct wlmim_pointer_param,
        touchpad.tap_to_click,
        touchpad.tap_to_click,
        true),
    BSPL_DESC_ENUM(
        "ScrollMethod",
        false,
        struct wlmim_pointer_param,
        touchpad.scroll_method,
        touchpad.scroll_method,
        LIBINPUT_CONFIG_SCROLL_2FG,
        _wlmim_pointer_scroll_methods),
    BSPL_DESC_BOOL(
        "NaturalScrolling",
        true,
        struct wlmim_pointer_param,
        touchpad.natural_scrolling,
        touchpad.natural_scrolling,
        true),
    BSPL_DESC_SENTINEL()
};

/** Enum definition for pointer-related config section(s). */
static const bspl_desc_t _wlmim_pointer_config[] = {
    BSPL_DESC_DICT(
        "Touchpad",
        false,
        struct wlmim_pointer_param,
        touchpad,
        touchpad,
        _wlmim_pointer_config_touchpad),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

void _wlmim_pointer_apply(
    wlmim_pointer_t *pointer_ptr,
    const struct wlmim_pointer_param *param_ptr);

/* ------------------------------------------------------------------------- */
wlmim_pointer_t *wlmim_pointer_create(
    struct wlr_input_device *wlr_input_device_ptr,
    const struct wlmim_pointer_param *param_ptr)
{
    wlmim_pointer_t *pointer_ptr = logged_calloc(1, sizeof(*pointer_ptr));
    if (NULL == pointer_ptr) return NULL;
    pointer_ptr->enabled = true;
    pointer_ptr->wlr_input_device_ptr = wlr_input_device_ptr;

    _wlmim_pointer_apply(pointer_ptr, param_ptr);

    return pointer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmim_pointer_destroy(wlmim_pointer_t *pointer_ptr)
{
    free(pointer_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmim_pointer_parse_config(
    bspl_dict_t *config_dict_ptr,
    struct wlmim_pointer_param *param_ptr)
{
    return bspl_decode_dict(
        config_dict_ptr,
        _wlmim_pointer_config,
        param_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmim_pointer_enabled(wlmim_pointer_t *pointer_ptr)
{
    return pointer_ptr->enabled;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Applies the touchpad configuration to the pointer input device.
 *
 * A no-op if the device is not a touchpad, ie. has zero 'tap fingers'.
 *
 * @param p
 * @param param_ptr
 */
void _wlmim_pointer_apply(
    wlmim_pointer_t *p,
    const struct wlmim_pointer_param *param_ptr)
{
    enum libinput_config_status s;
    struct libinput_device *lid_ptr;

    if (!wlr_input_device_is_libinput(p->wlr_input_device_ptr)) return;
    lid_ptr = wlr_libinput_get_device_handle(p->wlr_input_device_ptr);

    // Guard clause: Ignore non-touchpad pointers.
    if (0 >= libinput_device_config_tap_get_finger_count(lid_ptr)) return;

    // (only) touchpads can be disabled.
    p->enabled = param_ptr->touchpad.enabled;

    // Configure disable-while-typing.
    if (libinput_device_config_dwt_is_available(lid_ptr)) {
        s = libinput_device_config_dwt_set_enabled(
            lid_ptr,
            param_ptr->touchpad.disable_while_typing ?
            LIBINPUT_CONFIG_DWT_ENABLED :
            LIBINPUT_CONFIG_DWT_DISABLED);
        if (LIBINPUT_CONFIG_STATUS_SUCCESS != s) {
            bs_log(BS_WARNING,
                   "Failed libinput_device_config_dwt_set_enabled(%p, %d)",
                   lid_ptr,
                   param_ptr->touchpad.disable_while_typing ?
                   LIBINPUT_CONFIG_DWT_ENABLED :
                   LIBINPUT_CONFIG_DWT_DISABLED);
        }
    } else if (param_ptr->touchpad.disable_while_typing) {
        bs_log(BS_WARNING, "disable-while-typing not supported on device %p",
               lid_ptr);
    }

    // Configure click method.
    uint32_t methods = libinput_device_config_click_get_methods(lid_ptr);
    if ((methods & param_ptr->touchpad.click_method) ==
        param_ptr->touchpad.click_method) {
        s = libinput_device_config_click_set_method(
            lid_ptr,
            param_ptr->touchpad.click_method);
        if (LIBINPUT_CONFIG_STATUS_SUCCESS != s) {
            bs_log(BS_WARNING,
                   "Failed libinput_device_config_click_set_method(%p, %u)",
                   lid_ptr, param_ptr->touchpad.click_method);
        }
    } else if (LIBINPUT_CONFIG_CLICK_METHOD_NONE !=
               param_ptr->touchpad.click_method) {
        bs_log(BS_WARNING,
               "Click method %d not in supported set %"PRIx32" on device %p",
               param_ptr->touchpad.click_method, methods, lid_ptr);
    }

    // Configure tap to click: Note - capability check isn't needed, since we
    // expect to only get here if this is a touchpad, ie. having a non-zero
    // finger count.
    s = libinput_device_config_tap_set_enabled(
        lid_ptr,
        param_ptr->touchpad.tap_to_click ?
        LIBINPUT_CONFIG_TAP_ENABLED :
        LIBINPUT_CONFIG_TAP_DISABLED);
    if (LIBINPUT_CONFIG_STATUS_SUCCESS != s) {
        bs_log(BS_WARNING,
               "Failed libinput_device_config_tap_set_enabled(%p, %d)",
               lid_ptr,
               param_ptr->touchpad.tap_to_click ?
               LIBINPUT_CONFIG_TAP_ENABLED :
               LIBINPUT_CONFIG_TAP_DISABLED);
    }

    // Configure scroll_method.
    methods = libinput_device_config_scroll_get_methods(lid_ptr);
    if ((methods & param_ptr->touchpad.scroll_method) ==
        param_ptr->touchpad.scroll_method) {
        s = libinput_device_config_scroll_set_method(
            lid_ptr, param_ptr->touchpad.scroll_method);
        if (LIBINPUT_CONFIG_STATUS_SUCCESS != s) {
            bs_log(BS_WARNING,
                   "Failed libinput_device_config_scroll_set_method(%p, %d)",
                   lid_ptr, param_ptr->touchpad.scroll_method);
        }
    } else if (LIBINPUT_CONFIG_SCROLL_NO_SCROLL !=
               param_ptr->touchpad.scroll_method) {
        bs_log(BS_WARNING,
               "Scroll method %d not in supported set %"PRIx32" on device %p",
               param_ptr->touchpad.scroll_method, methods, lid_ptr);
    }

    // Configure natural_scrolling -- if set, the scrolled content moves in the
    // same direction as the finger movements ("natural"). If not, it's the view
    // (the scroll bar) that moves aligned with the finger movements.
    if (libinput_device_config_scroll_has_natural_scroll(lid_ptr)) {
        s = libinput_device_config_scroll_set_natural_scroll_enabled(
            lid_ptr, param_ptr->touchpad.natural_scrolling);
        if (LIBINPUT_CONFIG_STATUS_SUCCESS != s) {
            bs_log(BS_WARNING, "Failed libinput_device_config_scroll_set"
                   "_natural_scroll_enabled(%p, %d)",
                   lid_ptr, param_ptr->touchpad.natural_scrolling);
        }
    } else if (param_ptr->touchpad.natural_scrolling) {
        bs_log(BS_WARNING, "NaturalScrolling not supported on device %p",
               lid_ptr);
    }
}


/* == Unit Tests =========================================================== */

static void _wlmim_pointer_test_parse(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmim_pointer_test_cases[] = {
    { true, "parse", _wlmim_pointer_test_parse },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmim_pointer_test_set = BS_TEST_SET(
    true, "pointer", wlmim_pointer_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests parsing the dict. */
void _wlmim_pointer_test_parse(bs_test_t *test_ptr)
{
    bspl_dict_t *d = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Touchpad={"
            "Enabled=Yes;"
            "DisableWhileTyping=False;"
            "ClickMethod=ButtonAreas;"
            "TapToClick=Disabled;"
            "ScrollMethod=OnButtonDown;"
            "NaturalScrolling=False;"
            "};}"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, d);

    struct wlmim_pointer_param p = {};

    BS_TEST_VERIFY_TRUE(test_ptr, wlmim_pointer_parse_config(d, &p));
    BS_TEST_VERIFY_TRUE(test_ptr, p.touchpad.enabled);
    BS_TEST_VERIFY_FALSE(test_ptr, p.touchpad.disable_while_typing);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS,
        p.touchpad.click_method);
    BS_TEST_VERIFY_FALSE(test_ptr, p.touchpad.tap_to_click);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN,
        p.touchpad.scroll_method);
    BS_TEST_VERIFY_FALSE(test_ptr, p.touchpad.natural_scrolling);
}

/* == End of pointer.c ===================================================== */
