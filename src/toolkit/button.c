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

/* == Data ================================================================= */

/** Virtual method table for @ref wlmtk_button_t::super_buffer. */
static const wlmtk_buffer_impl_t button_buffer_impl = {
    .destroy = button_buffer_destroy,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_button_init(
    wlmtk_button_t *button_ptr,
    const wlmtk_button_impl_t *button_impl_ptr,
    struct wlr_buffer *released_wlr_buffer_ptr,
    struct wlr_buffer *pressed_wlr_buffer_ptr)
{
    BS_ASSERT(NULL != button_ptr);
    memset(button_ptr, 0, sizeof(wlmtk_button_t));
    BS_ASSERT(NULL != button_impl_ptr);
    BS_ASSERT(NULL != button_impl_ptr->destroy);
    memcpy(&button_ptr->impl, button_impl_ptr, sizeof(wlmtk_button_impl_t));

    button_ptr->released_wlr_buffer_ptr = wlr_buffer_lock(
        released_wlr_buffer_ptr);
    button_ptr->pressed_wlr_buffer_ptr = wlr_buffer_lock(
        pressed_wlr_buffer_ptr);

    if (!wlmtk_buffer_init(
            &button_ptr->super_buffer,
            &button_buffer_impl,
            released_wlr_buffer_ptr)) {
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
    __UNUSED__ wlmtk_button_t *button_ptr,
    __UNUSED__ struct wlr_buffer *pressed_wlr_buffer_ptr,
    __UNUSED__ struct wlr_buffer *released_wlr_buffer_ptr)
{
    // FIXME.
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
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(1, 1);
    wlmtk_button_t button;

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_button_init(&button,  &fake_button_impl, wlr_buffer_ptr, wlr_buffer_ptr));
    wlr_buffer_drop(wlr_buffer_ptr);

    wlmtk_element_destroy(&button.super_buffer.super_element);
}

/* == End of button.c ====================================================== */
