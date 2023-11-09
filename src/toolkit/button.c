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

#include "gfxbuf.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void button_buffer_destroy(wlmtk_buffer_t *buffer_ptr);
static bool buffer_pointer_motion(
    wlmtk_buffer_t *buffer_ptr,
    double x, double y,
    uint32_t time_msec);
static bool buffer_pointer_button(
    wlmtk_buffer_t *buffer_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void buffer_pointer_leave(
    wlmtk_buffer_t *buffer_ptr);

/* == Data ================================================================= */

/** Virtual method table for @ref wlmtk_button_t::super_buffer. */
static const wlmtk_buffer_impl_t button_buffer_impl = {
    .destroy = button_buffer_destroy,
    .pointer_motion = buffer_pointer_motion,
    .pointer_button = buffer_pointer_button,
    .pointer_leave = buffer_pointer_leave,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_button_init(
    wlmtk_button_t *button_ptr,
    const wlmtk_button_impl_t *button_impl_ptr)
{
    BS_ASSERT(NULL != button_ptr);
    memset(button_ptr, 0, sizeof(wlmtk_button_t));
    BS_ASSERT(NULL != button_impl_ptr);
    BS_ASSERT(NULL != button_impl_ptr->destroy);
    memcpy(&button_ptr->impl, button_impl_ptr, sizeof(wlmtk_button_impl_t));

    if (!wlmtk_buffer_init(
            &button_ptr->super_buffer,
            &button_buffer_impl)) {
        wlmtk_button_fini(button_ptr);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_button_fini(wlmtk_button_t *button_ptr)
{
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
    struct wlr_buffer *pressed_wlr_buffer_ptr)
{
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

    if (button_ptr->pressed) {
        wlmtk_buffer_set(
            &button_ptr->super_buffer,
            button_ptr->pressed_wlr_buffer_ptr);
    } else {
        wlmtk_buffer_set(
            &button_ptr->super_buffer,
            button_ptr->released_wlr_buffer_ptr);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Destructor: Wraps to @ref wlmtk_button_impl_t::destroy. */
void button_buffer_destroy(wlmtk_buffer_t *buffer_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_button_t, super_buffer);
    button_ptr->impl.destroy(button_ptr);
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_buffer_impl_t::pointer_motion. */
bool buffer_pointer_motion(
    wlmtk_buffer_t *buffer_ptr,
    __UNUSED__ double x,
    __UNUSED__ double y,
    __UNUSED__ uint32_t time_msec)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_button_t, super_buffer);

    if (!button_ptr->pointer_inside) {
        bs_log(BS_INFO, "FIXME: enter.");
    }
    button_ptr->pointer_inside = true;
    return true;
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_buffer_impl_t::pointer_button. */
bool buffer_pointer_button(
    wlmtk_buffer_t *buffer_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_button_t, super_buffer);

    bs_log(BS_INFO, "FIXME: event %p for %p", button_event_ptr, button_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_buffer_impl_t::pointer_leave. */
void buffer_pointer_leave(
    wlmtk_buffer_t *buffer_ptr)
{
    wlmtk_button_t *button_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_button_t, super_buffer);

    if (button_ptr->pointer_inside) {
        bs_log(BS_INFO, "FIXME: leave.");
    }
    button_ptr->pointer_inside = false;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

/** Test case definition. */
const bs_test_case_t wlmtk_button_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/** Fake destructor. */
static void fake_button_destroy(__UNUSED__ wlmtk_button_t *button_ptr) {}

/** Virtual method table of fake button. */
const wlmtk_button_impl_t fake_button_impl = {
    .destroy = fake_button_destroy,
};

/* ------------------------------------------------------------------------- */
/** Exercises @ref wlmtk_button_init and @ref wlmtk_button_fini. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_button_t button;

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_button_init(&button,  &fake_button_impl));
    wlmtk_element_destroy(&button.super_buffer.super_element);
}

/* == End of button.c ====================================================== */
