/* ========================================================================= */
/**
 * @file titlebar_button.c
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

#include "titlebar_button.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

#include "buffer.h"
#include "button.h"
#include "gfxbuf.h"  // IWYU pragma: keep
#include "input.h"
#include "primitives.h"
#include "util.h"

/* == Declarations ========================================================= */

/** State of a titlebar button. */
struct _wlmtk_titlebar_button_t {
    /** Superclass: Button. */
    wlmtk_button_t            super_button;
    /** Whether the titlebar button is activated (focussed). */
    bool                      activated;

    /** Callback for when the button is clicked. */
    void                      (*click_handler2)(wlmtk_window_t *window_ptr);
    /** Points to the @ref wlmtk_window_t that carries this titlebar. */
    wlmtk_window_t           *window_ptr;
    /** For drawing the button contents. */
    wlmtk_titlebar_button_draw_t draw;

    /** WLR buffer of the button when focussed & released. */
    struct wlr_buffer         *focussed_released_wlr_buffer_ptr;
    /** WLR buffer of the button when focussed & pressed. */
    struct wlr_buffer         *focussed_pressed_wlr_buffer_ptr;
    /** WLR buffer of the button when blurred. */
    struct wlr_buffer         *blurred_wlr_buffer_ptr;
};

static void titlebar_button_element_destroy(wlmtk_element_t *element_ptr);
static void titlebar_button_clicked(wlmtk_button_t *button_ptr);
static void update_buffers(wlmtk_titlebar_button_t *titlebar_button_ptr);
static struct wlr_buffer *create_buf(
    bs_gfxbuf_t *gfxbuf_ptr,
    int position,
    bool pressed,
    bool focussed,
    const wlmtk_titlebar_style_t *style_ptr,
    wlmtk_titlebar_button_draw_t draw);

/* == Data ================================================================= */

/** Extension to the superclass element's virtual method table. */
static const wlmtk_element_vmt_t titlebar_button_element_vmt = {
    .destroy = titlebar_button_element_destroy,
};

/** Extension to the parent button class' virtual methods. */
static const wlmtk_button_vmt_t titlebar_button_vmt = {
    .clicked = titlebar_button_clicked,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_button_t *wlmtk_titlebar2_button_create(
    void (*click_handler)(wlmtk_window_t *window_ptr),
    wlmtk_window_t *window_ptr,
    wlmtk_titlebar_button_draw_t draw)
{
    BS_ASSERT(NULL != window_ptr);
    BS_ASSERT(NULL != click_handler);
    BS_ASSERT(NULL != draw);
    wlmtk_titlebar_button_t *titlebar_button_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_button_t));
    if (NULL == titlebar_button_ptr) return NULL;
    titlebar_button_ptr->click_handler2 = click_handler;
    titlebar_button_ptr->window_ptr = window_ptr;
    titlebar_button_ptr->draw = draw;

    if (!wlmtk_button_init(&titlebar_button_ptr->super_button)) {
        wlmtk_titlebar_button_destroy(titlebar_button_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &titlebar_button_ptr->super_button.super_buffer.super_element,
        &titlebar_button_element_vmt);
    wlmtk_button_extend(
        &titlebar_button_ptr->super_button,
        &titlebar_button_vmt);

    return titlebar_button_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_button_destroy(
    wlmtk_titlebar_button_t *titlebar_button_ptr)
{
    wlr_buffer_drop_nullify(
        &titlebar_button_ptr->focussed_released_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(
        &titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(
        &titlebar_button_ptr->blurred_wlr_buffer_ptr);

    wlmtk_button_fini(&titlebar_button_ptr->super_button);
    free(titlebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_button_set_activated(
    wlmtk_titlebar_button_t *titlebar_button_ptr,
    bool activated)
{
    if (titlebar_button_ptr->activated == activated) return;
    titlebar_button_ptr->activated = activated;
    update_buffers(titlebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_titlebar_button_redraw(
    wlmtk_titlebar_button_t *titlebar_button_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    const wlmtk_titlebar_style_t *style_ptr)
{
    BS_ASSERT(focussed_gfxbuf_ptr->width == blurred_gfxbuf_ptr->width);
    BS_ASSERT(focussed_gfxbuf_ptr->height == blurred_gfxbuf_ptr->height);
    BS_ASSERT(style_ptr->height == focussed_gfxbuf_ptr->height);
    BS_ASSERT(position + style_ptr->height <= focussed_gfxbuf_ptr->width);

    struct wlr_buffer *focussed_released_ptr = create_buf(
        focussed_gfxbuf_ptr, position, false, true, style_ptr,
        titlebar_button_ptr->draw);
    struct wlr_buffer *focussed_pressed_ptr = create_buf(
        focussed_gfxbuf_ptr, position, true, true, style_ptr,
        titlebar_button_ptr->draw);
    struct wlr_buffer *blurred_ptr = create_buf(
        blurred_gfxbuf_ptr, position, false, false, style_ptr,
        titlebar_button_ptr->draw);

    if (NULL != focussed_released_ptr &&
        NULL != focussed_pressed_ptr &&
        NULL != blurred_ptr) {
        wlr_buffer_drop_nullify(
            &titlebar_button_ptr->focussed_released_wlr_buffer_ptr);
        wlr_buffer_drop_nullify(
            &titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr);
        wlr_buffer_drop_nullify(
            &titlebar_button_ptr->blurred_wlr_buffer_ptr);

        titlebar_button_ptr->focussed_released_wlr_buffer_ptr =
            focussed_released_ptr;
        titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr =
            focussed_pressed_ptr;
        titlebar_button_ptr->blurred_wlr_buffer_ptr = blurred_ptr;

        update_buffers(titlebar_button_ptr);
        return true;
    }

    wlr_buffer_drop_nullify(&focussed_released_ptr);
    wlr_buffer_drop_nullify(&focussed_pressed_ptr);
    wlr_buffer_drop_nullify(&blurred_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_titlebar_button_element(
    wlmtk_titlebar_button_t *titlebar_button_ptr)
{
    return &titlebar_button_ptr->super_button.super_buffer.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, wraps to @ref wlmtk_titlebar_button_destroy. */
void titlebar_button_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_titlebar_button_t *titlebar_button_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_titlebar_button_t,
        super_button.super_buffer.super_element);
    wlmtk_titlebar_button_destroy(titlebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles button clicks: Passes the request to the window. */
void titlebar_button_clicked(wlmtk_button_t *button_ptr)
{
    wlmtk_titlebar_button_t *titlebar_button_ptr = BS_CONTAINER_OF(
        button_ptr, wlmtk_titlebar_button_t, super_button);
    titlebar_button_ptr->click_handler2(titlebar_button_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Updates the button's buffer depending on activation status. */
void update_buffers(wlmtk_titlebar_button_t *titlebar_button_ptr)
{
    // No buffer: Nothing to update.
    if (NULL == titlebar_button_ptr->focussed_released_wlr_buffer_ptr ||
        NULL == titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr ||
        NULL == titlebar_button_ptr->blurred_wlr_buffer_ptr) return;

    if (titlebar_button_ptr->activated) {
        wlmtk_button_set(
            &titlebar_button_ptr->super_button,
            titlebar_button_ptr->focussed_released_wlr_buffer_ptr,
            titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr);
    } else {
        wlmtk_button_set(
            &titlebar_button_ptr->super_button,
            titlebar_button_ptr->blurred_wlr_buffer_ptr,
            titlebar_button_ptr->blurred_wlr_buffer_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Helper: Creates a WLR buffer for the button. */
struct wlr_buffer *create_buf(
    bs_gfxbuf_t *gfxbuf_ptr,
    int position,
    bool pressed,
    bool focussed,
    const wlmtk_titlebar_style_t *style_ptr,
    wlmtk_titlebar_button_draw_t draw)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        style_ptr->height, style_ptr->height);
    if (NULL == wlr_buffer_ptr) return NULL;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr), 0, 0,
        gfxbuf_ptr, position, 0, style_ptr->height, style_ptr->height);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }
    wlmaker_primitives_draw_bezel(
        cairo_ptr, style_ptr->bezel_width, !pressed);
    uint32_t color = style_ptr->focussed_text_color;
    if (!focussed) color = style_ptr->blurred_text_color;
    draw(cairo_ptr, style_ptr->height, color);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* == Unit tests =========================================================== */

static void test_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_titlebar_button_test_cases[] = {
    { 1, "button", test_button },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests button visualization. */
void test_button(bs_test_t *test_ptr)
{
    wlmtk_window_t *w = wlmtk_test_window_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    wlmtk_window_set_properties(w, WLMTK_WINDOW_PROPERTY_CLOSABLE);
    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window_events(w)->request_close, &l);

    wlmtk_titlebar_button_t *button_ptr = wlmtk_titlebar2_button_create(
        wlmtk_window_request_close, w, wlmaker_primitives_draw_close_icon);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, button_ptr);
    wlmtk_titlebar_button_set_activated(button_ptr, true);

    // For improved readability.
    wlmtk_buffer_t *super_buffer_ptr = &button_ptr->super_button.super_buffer;
    wlmtk_element_t *element_ptr = wlmtk_titlebar_button_element(button_ptr);
    wlmtk_element_set_visible(element_ptr, true);

    // Draw contents.
    wlmtk_titlebar_style_t style = {
        .height = 22,
        .focussed_text_color = 0xffffffff,
        .blurred_text_color = 0xffe0c0a0,
        .bezel_width = 1
    };
    bs_gfxbuf_t *f_ptr = bs_gfxbuf_create(100, 22);
    bs_gfxbuf_clear(f_ptr, 0xff4040c0);
    bs_gfxbuf_t *b_ptr = bs_gfxbuf_create(100, 22);
    bs_gfxbuf_clear(b_ptr, 0xff303030);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_button_redraw(button_ptr, f_ptr, b_ptr, 30, &style));
    bs_gfxbuf_destroy(b_ptr);
    bs_gfxbuf_destroy(f_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_button_focussed_released.png");

    // Pointer must be inside the button for accepting DOWN.
    wlmtk_pointer_motion_event_t mev = { .x = 11, .y = 11 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, &mev));

    // Button down: pressed.
    wlmtk_button_event_t button = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr,  &button));
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_button_focussed_pressed.png");

    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr,  &button));
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_button_focussed_released.png");
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    // Click: To be passed along, no change to visual.
    button.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr,  &button));
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_button_focussed_released.png");
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    // De-activate: Show as blurred.
    wlmtk_titlebar_button_set_activated(button_ptr, false);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_button_blurred.png");

    wlmtk_element_destroy(element_ptr);
    wlmtk_window_destroy(w);
}

/* == End of titlebar_button.c ============================================= */
