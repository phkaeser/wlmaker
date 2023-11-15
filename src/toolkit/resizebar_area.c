/* ========================================================================= */
/**
 * @file resizebar_area.c
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

#include "resizebar_area.h"

#include "box.h"
#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of an element of the resize bar. */
struct _wlmtk_resizebar_area_t {
    /** Superclass: Buffer. */
    wlmtk_buffer_t            super_buffer;

    /** WLR buffer holding the buffer in released state. */
    struct wlr_buffer         *released_wlr_buffer_ptr;
    /** WLR buffer holding the buffer in pressed state. */
    struct wlr_buffer         *pressed_wlr_buffer_ptr;

    /** Whether the area is currently pressed or not. */
    bool                      pressed;

    /** Window to which the resize bar area belongs. To initiate resizing. */
    wlmtk_window_t            *window_ptr;
    /** Edges that the resizebar area controls. */
    uint32_t                  edges;

    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;
    /** Name of the cursor to show when having pointer focus. */
    const char                 *xcursor_name_ptr;
};

static void buffer_destroy(wlmtk_buffer_t *buffer_ptr);
static bool buffer_pointer_motion(
    wlmtk_buffer_t *buffer_ptr,
    double x, double y,
    uint32_t time_msec);
static bool buffer_pointer_button(
    wlmtk_buffer_t *buffer_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void draw_state(wlmtk_resizebar_area_t *resizebar_area_ptr);
static struct wlr_buffer *create_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr,
    bool pressed);

/* == Data ================================================================= */

/** Buffer implementation for title of the title bar. */
static const wlmtk_buffer_impl_t area_buffer_impl = {
    .destroy = buffer_destroy,
    .pointer_motion = buffer_pointer_motion,
    .pointer_button = buffer_pointer_button,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_resizebar_area_t *wlmtk_resizebar_area_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr,
    wlmtk_window_t *window_ptr,
    uint32_t edges)
{
    wlmtk_resizebar_area_t *resizebar_area_ptr = logged_calloc(
        1, sizeof(wlmtk_resizebar_area_t));
    if (NULL == resizebar_area_ptr) return NULL;
    resizebar_area_ptr->wlr_cursor_ptr = wlr_cursor_ptr;
    resizebar_area_ptr->wlr_xcursor_manager_ptr = wlr_xcursor_manager_ptr;
    resizebar_area_ptr->window_ptr = window_ptr;
    resizebar_area_ptr->edges = edges;

    resizebar_area_ptr->xcursor_name_ptr = "default";  // Fail-safe value.
    switch (resizebar_area_ptr->edges) {
    case WLR_EDGE_BOTTOM:
        resizebar_area_ptr->xcursor_name_ptr = "s-resize";
        break;
    case WLR_EDGE_BOTTOM | WLR_EDGE_LEFT:
        resizebar_area_ptr->xcursor_name_ptr = "sw-resize";
        break;
    case WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT:
        resizebar_area_ptr->xcursor_name_ptr = "se-resize";
        break;
    default:
        bs_log(BS_ERROR, "Unsupported edge %"PRIx32, edges);
    }

    if (!wlmtk_buffer_init(
            &resizebar_area_ptr->super_buffer,
            &area_buffer_impl)) {
        wlmtk_resizebar_area_destroy(resizebar_area_ptr);
        return NULL;
    }

    draw_state(resizebar_area_ptr);
    return resizebar_area_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_area_destroy(
    wlmtk_resizebar_area_t *resizebar_area_ptr)
{
    wlr_buffer_drop_nullify(
        &resizebar_area_ptr->released_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(
        &resizebar_area_ptr->pressed_wlr_buffer_ptr);

    wlmtk_buffer_fini(&resizebar_area_ptr->super_buffer);
    free(resizebar_area_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_resizebar_area_redraw(
    wlmtk_resizebar_area_t *resizebar_area_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr)
{
    struct wlr_buffer *released_wlr_buffer_ptr = create_buffer(
        gfxbuf_ptr, position, width, style_ptr, false);
    struct wlr_buffer *pressed_wlr_buffer_ptr = create_buffer(
        gfxbuf_ptr, position, width, style_ptr, true);

    if (NULL == released_wlr_buffer_ptr ||
        NULL == pressed_wlr_buffer_ptr) {
        wlr_buffer_drop_nullify(&released_wlr_buffer_ptr);
        wlr_buffer_drop_nullify(&pressed_wlr_buffer_ptr);
        return false;
    }

    wlr_buffer_drop_nullify(
        &resizebar_area_ptr->released_wlr_buffer_ptr);
    resizebar_area_ptr->released_wlr_buffer_ptr = released_wlr_buffer_ptr;
    wlr_buffer_drop_nullify(
        &resizebar_area_ptr->pressed_wlr_buffer_ptr);
    resizebar_area_ptr->pressed_wlr_buffer_ptr = pressed_wlr_buffer_ptr;

    draw_state(resizebar_area_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_resizebar_area_element(
    wlmtk_resizebar_area_t *resizebar_area_ptr)
{
    return &resizebar_area_ptr->super_buffer.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Dtor. Forwards to @ref wlmtk_resizebar_area_destroy. */
void buffer_destroy(wlmtk_buffer_t *buffer_ptr)
{
    wlmtk_resizebar_area_t *resizebar_area_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_resizebar_area_t, super_buffer);
    wlmtk_resizebar_area_destroy(resizebar_area_ptr);
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_buffer_impl_t::pointer_motion. */
bool buffer_pointer_motion(
    wlmtk_buffer_t *buffer_ptr,
    __UNUSED__ double x, __UNUSED__ double y,
    __UNUSED__ uint32_t time_msec)
{
    wlmtk_resizebar_area_t *resizebar_area_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_resizebar_area_t, super_buffer);

    // TODO(kaeser@gubbe.ch): Inject something testable here.
    if (NULL != resizebar_area_ptr->wlr_cursor_ptr &&
        NULL != resizebar_area_ptr->wlr_xcursor_manager_ptr) {
        wlr_cursor_set_xcursor(
            resizebar_area_ptr->wlr_cursor_ptr,
            resizebar_area_ptr->wlr_xcursor_manager_ptr,
            resizebar_area_ptr->xcursor_name_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/** See @ref wlmtk_buffer_impl_t::pointer_button. */
bool buffer_pointer_button(
    wlmtk_buffer_t *buffer_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_resizebar_area_t *resizebar_area_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_resizebar_area_t, super_buffer);

    if (button_event_ptr->button != BTN_LEFT) return false;

    switch (button_event_ptr->type) {
    case WLMTK_BUTTON_DOWN:
        resizebar_area_ptr->pressed = true;

        if (NULL != resizebar_area_ptr->window_ptr) {
            wlmtk_window_request_resize(
                resizebar_area_ptr->window_ptr,
                resizebar_area_ptr->edges);
        }
        draw_state(resizebar_area_ptr);
        break;

    case WLMTK_BUTTON_UP:
        resizebar_area_ptr->pressed = false;
        draw_state(resizebar_area_ptr);
        break;

    default:
        break;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Draws the buffer in current state (released or pressed).
 *
 * @param resizebar_area_ptr
 */
void draw_state(wlmtk_resizebar_area_t *resizebar_area_ptr)
{
    if (!resizebar_area_ptr->pressed) {
        wlmtk_buffer_set(
            &resizebar_area_ptr->super_buffer,
            resizebar_area_ptr->released_wlr_buffer_ptr);
    } else {
        wlmtk_buffer_set(
            &resizebar_area_ptr->super_buffer,
            resizebar_area_ptr->pressed_wlr_buffer_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a resizebar area texture.
 *
 * @param gfxbuf_ptr
 * @param position
 * @param width
 * @param style_ptr
 * @param pressed
 *
 * @return A pointer to a newly allocated `struct wlr_buffer`.
 */
struct wlr_buffer *create_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr,
    bool pressed)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, style_ptr->height);
    if (NULL == wlr_buffer_ptr) return NULL;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr), 0, 0,
        gfxbuf_ptr, position, 0, width, style_ptr->height);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return false;
    }
    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width,
        style_ptr->height, style_ptr->bezel_width, !pressed);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* == Unit tests =========================================================== */

static void test_area(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_resizebar_area_test_cases[] = {
    { 1, "area", test_area },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests the area behaviour. */
void test_area(bs_test_t *test_ptr)
{
    wlmtk_resizebar_area_t *area_ptr = wlmtk_resizebar_area_create(
        NULL, NULL, NULL, WLR_EDGE_BOTTOM);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, area_ptr);
    wlmtk_element_t *element_ptr = wlmtk_resizebar_area_element(area_ptr);

    // Draw and verify release state.
    wlmtk_resizebar_style_t style = { .height = 7, .bezel_width = 1.0 };
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(30, 7);
    bs_gfxbuf_clear(gfxbuf_ptr, 0xff604020);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_resizebar_area_redraw(area_ptr, gfxbuf_ptr, 10, 12, &style));
    bs_gfxbuf_destroy(gfxbuf_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(area_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/resizebar_area_released.png");

    // Pointer must be inside the button for accepting DOWN.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(element_ptr, 1, 1, 0));
    // Button down: pressed.
    wlmtk_button_event_t button = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(element_ptr,  &button));
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(area_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/resizebar_area_pressed.png");

    // TODO(kaeser@gubbe.ch): Should verify call to wlmtk_window_request_resize
    // and for setting the cursor.

    wlmtk_element_destroy(element_ptr);
}

/* == End of resizebar_area.c ============================================== */
