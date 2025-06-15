/* ========================================================================= */
/**
 * @file button.c
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

#include "button.h"

#include <linux/input-event-codes.h>
#include <math.h>
#include <string.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "gfxbuf.h"  // IWYU pragma: keep
#include "input.h"
#include "util.h"
#include "libbase/libbase.h"

/* == Declarations ========================================================= */

static void _wlmtk_button_clicked(wlmtk_button_t *button_ptr);

static bool _wlmtk_button_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmtk_button_handle_pointer_enter(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_button_handle_pointer_leave(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void apply_state(wlmtk_button_t *button_ptr);

/* == Data ================================================================= */

/** Virtual method table for the button's element super class. */
static const wlmtk_element_vmt_t button_element_vmt = {
    .pointer_button = _wlmtk_button_element_pointer_button,
};

/** Virtual method table for the button. */
static const wlmtk_button_vmt_t button_vmt = {
    .clicked = _wlmtk_button_clicked,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_button_init(wlmtk_button_t *button_ptr)
{
    BS_ASSERT(NULL != button_ptr);
    *button_ptr = (wlmtk_button_t){ .vmt = button_vmt };

    if (!wlmtk_buffer_init(&button_ptr->super_buffer)) {
        wlmtk_button_fini(button_ptr);
        return false;
    }
    button_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &button_ptr->super_buffer.super_element,
        &button_element_vmt);

    wlmtk_util_connect_listener_signal(
        &button_ptr->super_buffer.super_element.events.pointer_enter,
        &button_ptr->pointer_enter_listener,
        _wlmtk_button_handle_pointer_enter);
    wlmtk_util_connect_listener_signal(
        &button_ptr->super_buffer.super_element.events.pointer_leave,
        &button_ptr->pointer_leave_listener,
        _wlmtk_button_handle_pointer_leave);

    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_button_vmt_t wlmtk_button_extend(
    wlmtk_button_t *button_ptr,
    const wlmtk_button_vmt_t *button_vmt_ptr)
{
    wlmtk_button_vmt_t orig_vmt = button_ptr->vmt;

    if (NULL != button_vmt_ptr->clicked) {
        button_ptr->vmt.clicked = button_vmt_ptr->clicked;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_button_fini(wlmtk_button_t *button_ptr)
{
    wlmtk_util_disconnect_listener(&button_ptr->pointer_leave_listener);
    wlmtk_util_disconnect_listener(&button_ptr->pointer_enter_listener);
    wlmtk_util_disconnect_listener(&button_ptr->pointer_leave_listener);
    wlmtk_util_disconnect_listener(&button_ptr->pointer_enter_listener);

    if (NULL != button_ptr->pressed_wlr_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->pressed_wlr_buffer_ptr);
        button_ptr->pressed_wlr_buffer_ptr = NULL;
    }
    if (NULL != button_ptr->released_wlr_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->released_wlr_buffer_ptr);
        button_ptr->released_wlr_buffer_ptr = NULL;
    }

    wlmtk_buffer_fini(&button_ptr->super_buffer);
}

/* ------------------------------------------------------------------------- */
void wlmtk_button_set(
    wlmtk_button_t *button_ptr,
    struct wlr_buffer *released_wlr_buffer_ptr,
    struct wlr_buffer *pressed_wlr_buffer_ptr )
{
    if (NULL == released_wlr_buffer_ptr) {
        BS_ASSERT(NULL == pressed_wlr_buffer_ptr);
    } else {
        BS_ASSERT(released_wlr_buffer_ptr->width ==
                  pressed_wlr_buffer_ptr->width);
        BS_ASSERT(released_wlr_buffer_ptr->height ==
                  pressed_wlr_buffer_ptr->height);
    }

    if (NULL != button_ptr->released_wlr_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->released_wlr_buffer_ptr);
    }
    button_ptr->released_wlr_buffer_ptr = wlr_buffer_lock(
        released_wlr_buffer_ptr);

    if (NULL != button_ptr->pressed_wlr_buffer_ptr) {
        wlr_buffer_unlock(button_ptr->pressed_wlr_buffer_ptr);
    }
    button_ptr->pressed_wlr_buffer_ptr = wlr_buffer_lock(
        pressed_wlr_buffer_ptr);

    apply_state(button_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_button_vmt_t::clicked. Nothing. */
void _wlmtk_button_clicked(__UNUSED__ wlmtk_button_t *button_ptr)
{
    // Nothing.
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_element_vmt_t::pointer_button. */
bool _wlmtk_button_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_button_t, super_buffer.super_element);

    if (button_event_ptr->button != BTN_LEFT) return true;

    switch (button_event_ptr->type) {
    case WLMTK_BUTTON_DOWN:
        button_ptr->pressed = true;
        apply_state(button_ptr);
        break;

    case WLMTK_BUTTON_UP:
        button_ptr->pressed = false;
        apply_state(button_ptr);
        break;

    case WLMTK_BUTTON_CLICK:
        button_ptr->vmt.clicked(button_ptr);
        break;

    default:
        break;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Pointer enters the area: We may need to update visualization. */
void _wlmtk_button_handle_pointer_enter(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_button_t, pointer_enter_listener);
    apply_state(button_ptr);
}

/* ------------------------------------------------------------------------- */
/** Pointer leaves the area: We may need to update visualization. */
void _wlmtk_button_handle_pointer_leave(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_button_t, pointer_leave_listener);
    apply_state(button_ptr);
}

/* ------------------------------------------------------------------------- */
/** Sets the appropriate texture for the button. */
void apply_state(wlmtk_button_t *button_ptr)
{
    if (button_ptr->super_buffer.super_element.pointer_inside &&
        button_ptr->pressed) {
        wlmtk_buffer_set(
            &button_ptr->super_buffer,
            button_ptr->pressed_wlr_buffer_ptr);
    } else {
        wlmtk_buffer_set(
            &button_ptr->super_buffer,
            button_ptr->released_wlr_buffer_ptr);
    }
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_press_release(bs_test_t *test_ptr);
static void test_press_release_outside(bs_test_t *test_ptr);
static void test_press_right(bs_test_t *test_ptr);

/** Test case definition. */
const bs_test_case_t wlmtk_button_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "press_release", test_press_release },
    { 1, "press_release_outside", test_press_release_outside },
    { 1, "press_right", test_press_right },
    { 0, NULL, NULL }
};

/** Test outcome: Whether 'clicked' was called. */
static bool fake_button_got_clicked = false;

/** Fake 'clicked' handler. */
static void fake_button_clicked(__UNUSED__ wlmtk_button_t *button_ptr) {
    fake_button_got_clicked = true;
}

/** Virtual method table of fake button. */
static const wlmtk_button_vmt_t fake_button_vmt = {
    .clicked = fake_button_clicked,
};

/* ------------------------------------------------------------------------- */
/** Exercises @ref wlmtk_button_init and @ref wlmtk_button_fini. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_button_t button;

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_button_init(&button));
    wlmtk_button_fini(&button);
}

/* ------------------------------------------------------------------------- */
/** Tests button pressing & releasing. */
void test_press_release(bs_test_t *test_ptr)
{
    wlmtk_button_t button;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_button_init(&button));
    wlmtk_button_extend(&button, &fake_button_vmt);
    wlmtk_element_set_visible(&button.super_buffer.super_element, true);

    struct wlr_buffer *p_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    struct wlr_buffer *r_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    wlmtk_button_set(&button, r_ptr, p_ptr);

    wlmtk_button_event_t event = { .button = BTN_LEFT, .time_msec = 42 };
    wlmtk_element_t *element_ptr = &button.super_buffer.super_element;
    fake_button_got_clicked = false;

    // Initial state: released.
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);
    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0, .time_msec = 41 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    // Button down: pressed.
    event.type = WLMTK_BUTTON_DOWN;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, p_ptr);

    // Pointer leaves the area: released.
    e = (wlmtk_pointer_motion_event_t){ .x = NAN, .y = NAN, .time_msec = 41 };
    wlmtk_element_pointer_motion(element_ptr, &e);
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    // Pointer re-enters the area: pressed.
    e = (wlmtk_pointer_motion_event_t){ .x = 0, .y = 0, .time_msec = 41 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, p_ptr);

    // Button up: released.
    event.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    event.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_button_got_clicked);

    wlr_buffer_drop(r_ptr);
    wlr_buffer_drop(p_ptr);
    wlmtk_button_fini(&button);
}

/* ------------------------------------------------------------------------- */
/** Tests button when releasing outside the pointer focus. */
void test_press_release_outside(bs_test_t *test_ptr)
{
    wlmtk_button_t button;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_button_init(&button));
    wlmtk_element_set_visible(&button.super_buffer.super_element, true);

    struct wlr_buffer *p_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    struct wlr_buffer *r_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    wlmtk_button_set(&button, r_ptr, p_ptr);

    wlmtk_button_event_t event = { .button = BTN_LEFT, .time_msec = 42 };
    wlmtk_element_t *element_ptr = &button.super_buffer.super_element;

    // Enter the ara. Released.
    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0, .time_msec = 41 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    // Button down: pressed.
    event.type = WLMTK_BUTTON_DOWN;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, p_ptr);

    // Pointer leaves the area: released.
    e = (wlmtk_pointer_motion_event_t){ .x = NAN, .y = NAN, .time_msec = 41 };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    // Button up, outside the area. Then, re-enter: Still released.
    event.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);
    e = (wlmtk_pointer_motion_event_t){ .x = 0, .y = 0, .time_msec = 41 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    wlr_buffer_drop(r_ptr);
    wlr_buffer_drop(p_ptr);
    wlmtk_button_fini(&button);
}

/* ------------------------------------------------------------------------- */
/** Tests button when releasing outside the pointer focus. */
void test_press_right(bs_test_t *test_ptr)
{
    wlmtk_button_t button;
    BS_TEST_VERIFY_TRUE_OR_RETURN(test_ptr, wlmtk_button_init(&button));
    wlmtk_element_set_visible(&button.super_buffer.super_element, true);

    struct wlr_buffer *p_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    struct wlr_buffer *r_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    wlmtk_button_set(&button, r_ptr, p_ptr);

    wlmtk_button_event_t event = {
        .button = BTN_RIGHT, .type = WLMTK_BUTTON_DOWN, .time_msec = 42,
    };
    wlmtk_element_t *element_ptr = &button.super_buffer.super_element;

    // Enter the ara. Released.
    wlmtk_pointer_motion_event_t e = { .x = 0, .y = 0, .time_msec = 41 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &e));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    // Right button down: Remains released, reports claimed.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr, &event));
    BS_TEST_VERIFY_EQ(test_ptr, button.super_buffer.wlr_buffer_ptr, r_ptr);

    wlr_buffer_drop(r_ptr);
    wlr_buffer_drop(p_ptr);
    wlmtk_button_fini(&button);
}

/* == End of button.c ====================================================== */
