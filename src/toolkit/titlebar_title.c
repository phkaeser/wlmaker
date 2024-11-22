/* ========================================================================= */
/**
 * @file titlebar_title.c
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

#include "titlebar_title.h"

#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"
#include "window.h"

#include <wlr/version.h>
#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the title bar's title. */
struct _wlmtk_titlebar_title_t {
    /** Superclass: Buffer. */
    wlmtk_buffer_t            super_buffer;
    /** Pointer to the window the title element belongs to. */
    wlmtk_window_t            *window_ptr;

    /** The drawn title, when focussed. */
    struct wlr_buffer         *focussed_wlr_buffer_ptr;
    /** The drawn title, when blurred. */
    struct wlr_buffer         *blurred_wlr_buffer_ptr;
};

static void _wlmtk_titlebar_title_element_destroy(
    wlmtk_element_t *element_ptr);
static bool _wlmtk_titlebar_title_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_titlebar_title_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);

static void title_set_activated(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bool activated);
struct wlr_buffer *title_create_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    uint32_t text_color,
    const char *title_ptr,
    const wlmtk_titlebar_style_t *style_ptr);

/* == Data ================================================================= */

/** Extension to the superclass elment's virtual method table. */
static const wlmtk_element_vmt_t titlebar_title_element_vmt = {
    .destroy = _wlmtk_titlebar_title_element_destroy,
    .pointer_button = _wlmtk_titlebar_title_element_pointer_button,
    .pointer_axis = _wlmtk_titlebar_title_element_pointer_axis,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    wlmtk_env_t *env_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmtk_titlebar_title_t *titlebar_title_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_title_t));
    if (NULL == titlebar_title_ptr) return NULL;
    titlebar_title_ptr->window_ptr = window_ptr;

    if (!wlmtk_buffer_init(&titlebar_title_ptr->super_buffer, env_ptr)) {
        wlmtk_titlebar_title_destroy(titlebar_title_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &titlebar_title_ptr->super_buffer.super_element,
        &titlebar_title_element_vmt);

    return titlebar_title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_title_destroy(wlmtk_titlebar_title_t *titlebar_title_ptr)
{
    wlr_buffer_drop_nullify(&titlebar_title_ptr->focussed_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(&titlebar_title_ptr->blurred_wlr_buffer_ptr);
    wlmtk_buffer_fini(&titlebar_title_ptr->super_buffer);
    free(titlebar_title_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_titlebar_title_redraw(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const char *title_ptr,
    const wlmtk_titlebar_style_t *style_ptr)
{
    BS_ASSERT(focussed_gfxbuf_ptr->width == blurred_gfxbuf_ptr->width);
    BS_ASSERT(style_ptr->height == focussed_gfxbuf_ptr->height);
    BS_ASSERT(style_ptr->height == blurred_gfxbuf_ptr->height);
    BS_ASSERT(position <= (int)focussed_gfxbuf_ptr->width);
    BS_ASSERT(position + width <= (int)focussed_gfxbuf_ptr->width);

    if (NULL == title_ptr) title_ptr = "";

    struct wlr_buffer *focussed_wlr_buffer_ptr = title_create_buffer(
        focussed_gfxbuf_ptr, position, width,
        style_ptr->focussed_text_color, title_ptr, style_ptr);
    struct wlr_buffer *blurred_wlr_buffer_ptr = title_create_buffer(
        blurred_gfxbuf_ptr, position, width,
        style_ptr->blurred_text_color, title_ptr, style_ptr);

    if (NULL == focussed_wlr_buffer_ptr ||
        NULL == blurred_wlr_buffer_ptr) {
        wlr_buffer_drop_nullify(&focussed_wlr_buffer_ptr);
        wlr_buffer_drop_nullify(&blurred_wlr_buffer_ptr);
        return false;
    }

    wlr_buffer_drop_nullify(&titlebar_title_ptr->focussed_wlr_buffer_ptr);
    titlebar_title_ptr->focussed_wlr_buffer_ptr = focussed_wlr_buffer_ptr;
    wlr_buffer_drop_nullify(&titlebar_title_ptr->blurred_wlr_buffer_ptr);
    titlebar_title_ptr->blurred_wlr_buffer_ptr = blurred_wlr_buffer_ptr;

    title_set_activated(titlebar_title_ptr, activated);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_title_set_activated(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bool activated)
{
    title_set_activated(titlebar_title_ptr, activated);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_titlebar_title_element(
    wlmtk_titlebar_title_t *titlebar_title_ptr)
{
    return &titlebar_title_ptr->super_buffer.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Dtor. */
void _wlmtk_titlebar_title_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmtk_titlebar_title_t *titlebar_title_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_titlebar_title_t, super_buffer.super_element);
    wlmtk_titlebar_title_destroy(titlebar_title_ptr);
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_element_vmt_t::pointer_button. */
bool _wlmtk_titlebar_title_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_titlebar_title_t *titlebar_title_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_titlebar_title_t, super_buffer.super_element);

    if (button_event_ptr->button != BTN_LEFT) return true;

    switch (button_event_ptr->type) {
    case WLMTK_BUTTON_DOWN:
        wlmtk_window_request_move(titlebar_title_ptr->window_ptr);
        break;

    default:  // Can be ignored.
        break;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles pointer axis events: Scroll wheel up will shade, down will unshade.
 *
 * @param element_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return true, if the axis event was consumed. That is the case if it's
 *     source is a scroll wheel, and the orientation is vertical.
 */
bool _wlmtk_titlebar_title_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmtk_titlebar_title_t *titlebar_title_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_titlebar_title_t, super_buffer.super_element);

    // Only consider vertical wheel moves.
    if (
#if WLR_VERSION_NUM >= (18 << 8)
        (WL_POINTER_AXIS_SOURCE_WHEEL != wlr_pointer_axis_event_ptr->source &&
         WL_POINTER_AXIS_SOURCE_FINGER != wlr_pointer_axis_event_ptr->source) ||
        WL_POINTER_AXIS_VERTICAL_SCROLL !=
        wlr_pointer_axis_event_ptr->orientation
#else // WLR_VERSION_NUM >= (18 << 8)
        (WLR_AXIS_SOURCE_WHEEL != wlr_pointer_axis_event_ptr->source &&
         WLR_AXIS_SOURCE_FINGER != wlr_pointer_axis_event_ptr->source) ||
        WLR_AXIS_ORIENTATION_VERTICAL !=wlr_pointer_axis_event_ptr->orientation
#endif // WLR_VERSION_NUM >= (18 << 8)
        ) {
        return false;
    }

    if (wlr_pointer_axis_event_ptr->delta > 0) {
        wlmtk_window_request_shaded(titlebar_title_ptr->window_ptr, false);
    } else if (wlr_pointer_axis_event_ptr->delta < 0) {
        wlmtk_window_request_shaded(titlebar_title_ptr->window_ptr, true);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Sets whether the title is drawn focussed (activated) or blurred.
 *
 * @param titlebar_title_ptr
 * @param activated
 */
void title_set_activated(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bool activated)
{
    wlmtk_buffer_set(
        &titlebar_title_ptr->super_buffer,
        activated ?
        titlebar_title_ptr->focussed_wlr_buffer_ptr :
        titlebar_title_ptr->blurred_wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a WLR buffer with the title's texture, as specified.
 *
 * @param gfxbuf_ptr
 * @param position
 * @param width
 * @param text_color
 * @param title_ptr
 * @param style_ptr
 *
 * @return A pointer to a `struct wlr_buffer` with the texture.
 */
struct wlr_buffer *title_create_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    uint32_t text_color,
    const char *title_ptr,
    const wlmtk_titlebar_style_t *style_ptr)
{
    BS_ASSERT(NULL != title_ptr);
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, style_ptr->height);
    if (NULL == wlr_buffer_ptr) return NULL;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr),
        0, 0,
        gfxbuf_ptr,
        position, 0,
        width, style_ptr->height);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }
    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width,
        style_ptr->height, style_ptr->bezel_width, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr, &style_ptr->font, title_ptr, text_color);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* == Unit tests =========================================================== */

static void test_title(bs_test_t *test_ptr);
static void test_shade(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_titlebar_title_test_cases[] = {
    // TODO(kaeser@gubbe.ch): Re-enable, once figuring out why this fails on
    // Trixie when running as a github action.
    { 0, "title", test_title },
    { 1, "shade", test_shade },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests title drawing. */
void test_title(bs_test_t *test_ptr)
{
    const wlmtk_titlebar_style_t style = {
        .focussed_text_color = 0xffc0c0c0,
        .blurred_text_color = 0xff808080,
        .height = 22,
        .font = {
            .face = "Helvetica",
            .weight = WLMTK_FONT_WEIGHT_BOLD,
            .size = 15,
        },
        .bezel_width = 1
    };

    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(120, 22);
    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(120, 22);
    bs_gfxbuf_clear(focussed_gfxbuf_ptr, 0xff2020c0);
    bs_gfxbuf_clear(blurred_gfxbuf_ptr, 0xff404040);

    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    wlmtk_titlebar_title_t *titlebar_title_ptr = wlmtk_titlebar_title_create(
        NULL, fake_window_ptr->window_ptr);
    wlmtk_element_t *element_ptr = wlmtk_titlebar_title_element(
        titlebar_title_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_title_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_title_redraw(
            titlebar_title_ptr,
            focussed_gfxbuf_ptr, blurred_gfxbuf_ptr,
            10, 90, true, "Title", &style));

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(titlebar_title_ptr->focussed_wlr_buffer_ptr),
        "toolkit/title_focussed.png");
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(titlebar_title_ptr->blurred_wlr_buffer_ptr),
        "toolkit/title_blurred.png");

    // We had started as "activated", verify that's correct.
    wlmtk_buffer_t *super_buffer_ptr = &titlebar_title_ptr->super_buffer;
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_focussed.png");

    // De-activated the title. Verify that was propagated.
    title_set_activated(titlebar_title_ptr, false);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_blurred.png");

    // Redraw with shorter width. Verify that's still correct.
    wlmtk_titlebar_title_redraw(
        titlebar_title_ptr, focussed_gfxbuf_ptr, blurred_gfxbuf_ptr,
        10, 70, false, "Title", &style);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(super_buffer_ptr->wlr_buffer_ptr),
        "toolkit/title_blurred_short.png");

    // Pressing a button should trigger a move.
    BS_TEST_VERIFY_FALSE(test_ptr, fake_window_ptr->request_move_called);
    wlmtk_button_event_t button = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr,  &button));
    BS_TEST_VERIFY_TRUE(test_ptr, fake_window_ptr->request_move_called);

    wlmtk_element_destroy(element_ptr);
    wlmtk_fake_window_destroy(fake_window_ptr);
    bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
    bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that axis actions trigger 'shade'. */
void test_shade(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    wlmtk_titlebar_title_t *titlebar_title_ptr = wlmtk_titlebar_title_create(
        NULL, fake_window_ptr->window_ptr);
    wlmtk_element_t *element_ptr = wlmtk_titlebar_title_element(
        titlebar_title_ptr);

    // Initial state: Not shaded.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    struct wlr_pointer_axis_event axis_event = {
#if WLR_VERSION_NUM >= (18 << 8)
        .source = WL_POINTER_AXIS_SOURCE_WHEEL,
        .orientation = WL_POINTER_AXIS_VERTICAL_SCROLL,
#else // WLR_VERSION_NUM >= (18 << 8)
        .source = WLR_AXIS_SOURCE_WHEEL,
        .orientation = WLR_AXIS_ORIENTATION_VERTICAL,
#endif // WLR_VERSION_NUM >= (18 << 8)
        .delta = -0.01
    };

    // Initial state: Not server-side-decorated, won't shade.
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    // Decorate. Now it shall shade.
    wlmtk_window_set_server_side_decorated(fake_window_ptr->window_ptr, true);
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    // Scroll the other way: Unshade.
    axis_event.delta = 0.01;
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    // Source 'finger from a touchpad' is accepted, too.
#if WLR_VERSION_NUM >= (18 << 8)
    axis_event.source = WL_POINTER_AXIS_SOURCE_FINGER;
#else // WLR_VERSION_NUM >= (18 << 8)
    axis_event.source = WLR_AXIS_SOURCE_FINGER;
#endif // WLR_VERSION_NUM >= (18 << 8)
    axis_event.delta = -0.01;
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    axis_event.delta = 0.01;
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    // Axis from another source: Ignored.
#if WLR_VERSION_NUM >= (18 << 8)
    axis_event.source = WL_POINTER_AXIS_SOURCE_WHEEL_TILT;
#else // WLR_VERSION_NUM >= (18 << 8)
    axis_event.source = WLR_AXIS_SOURCE_WHEEL_TILT;
#endif // WLR_VERSION_NUM >= (18 << 8)
    axis_event.delta = -0.01;
    wlmtk_element_pointer_axis(element_ptr, &axis_event);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_shaded(fake_window_ptr->window_ptr));

    wlmtk_titlebar_title_destroy(titlebar_title_ptr);
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* == End of titlebar_title.c ============================================== */
