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

#include "button.h"
#include "gfxbuf.h"
#include "primitives.h"

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of a titlebar button. */
struct _wlmtk_titlebar_button_t {
    /** Superclass: Button. */
    wlmtk_button_t            super_button;

    /** For drawing the button contents. */
    wlmtk_titlebar_button_draw_t draw;

    /** WLR buffer of the button when focussed & released. */
    struct wlr_buffer         *focussed_released_wlr_buffer_ptr;
    /** WLR buffer of the button when focussed & pressed. */
    struct wlr_buffer         *focussed_pressed_wlr_buffer_ptr;
    /** WLR buffer of the button when blurred. */
    struct wlr_buffer         *blurred_wlr_buffer_ptr;
};

static void titlebar_button_destroy(wlmtk_button_t *button_ptr);
static struct wlr_buffer *create_buf(
    bs_gfxbuf_t *gfxbuf_ptr,
    int position,
    bool pressed,
    const wlmtk_titlebar_style_t *style_ptr,
    wlmtk_titlebar_button_draw_t draw);

/* == Data ================================================================= */

/** Buffer implementation for title of the title bar. */
static const wlmtk_button_impl_t titlebar_button_impl = {
    .destroy = titlebar_button_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_button_t *wlmtk_titlebar_button_create(
    wlmtk_titlebar_button_draw_t draw)
{
    wlmtk_titlebar_button_t *titlebar_button_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_button_t));
    if (NULL == titlebar_button_ptr) return NULL;
    titlebar_button_ptr->draw = draw;

    if (!wlmtk_button_init(
            &titlebar_button_ptr->super_button,
            &titlebar_button_impl)) {
        wlmtk_titlebar_button_destroy(titlebar_button_ptr);
        return NULL;
    }

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
        focussed_gfxbuf_ptr, position, false, style_ptr,
        titlebar_button_ptr->draw);
    struct wlr_buffer *focussed_pressed_ptr = create_buf(
        focussed_gfxbuf_ptr, position, true, style_ptr,
        titlebar_button_ptr->draw);
    struct wlr_buffer *blurred_ptr = create_buf(
        blurred_gfxbuf_ptr, position, false, style_ptr,
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

        // FIXME: Depend on focus/blur.
        wlmtk_button_set(
            &titlebar_button_ptr->super_button,
            titlebar_button_ptr->focussed_released_wlr_buffer_ptr,
            titlebar_button_ptr->focussed_pressed_wlr_buffer_ptr);

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
void titlebar_button_destroy(wlmtk_button_t *button_ptr)
{
    wlmtk_titlebar_button_t *titlebar_button_ptr = BS_CONTAINER_OF(
        button_ptr, wlmtk_titlebar_button_t, super_button);
    wlmtk_titlebar_button_destroy(titlebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
/** Helper: Creates a WLR buffer for the button. */
struct wlr_buffer *create_buf(
    bs_gfxbuf_t *gfxbuf_ptr,
    int position,
    bool pressed,
    const wlmtk_titlebar_style_t *style_ptr,
    void (*draw)(cairo_t *cairo_ptr, uint32_t color))
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
    draw(cairo_ptr, style_ptr->focussed_text_color);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* == End of titlebar_button.c ============================================= */
