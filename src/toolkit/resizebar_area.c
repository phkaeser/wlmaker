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

};

static void buffer_destroy(wlmtk_buffer_t *buffer_ptr);
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
    .pointer_button = buffer_pointer_button,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_resizebar_area_t *wlmtk_resizebar_area_create(
    wlmtk_window_t *window_ptr,
    uint32_t edges)
{
    wlmtk_resizebar_area_t *resizebar_area_ptr = logged_calloc(
        1, sizeof(wlmtk_resizebar_area_t));
    if (NULL == resizebar_area_ptr) return NULL;
    resizebar_area_ptr->window_ptr = window_ptr;
    resizebar_area_ptr->edges = edges;

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

        wlmtk_window_request_resize(
            resizebar_area_ptr->window_ptr,
            resizebar_area_ptr->edges);
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

/* == End of resizebar_area.c ============================================== */
