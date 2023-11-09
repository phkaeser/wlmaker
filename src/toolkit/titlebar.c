/* ========================================================================= */
/**
 * @file titlebar.c
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

#include "titlebar.h"

#include "box.h"
#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Forward declaration. */
typedef struct _wlmtk_titlebar_title_t wlmtk_titlebar_title_t;

/** State of the title bar. */
struct _wlmtk_titlebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Title element of the title bar. */
    wlmtk_titlebar_title_t    *title_ptr;

    /** Titlebar background, when focussed. */
    bs_gfxbuf_t               *focussed_gfxbuf_ptr;
    /** Titlebar background, when blurred. */
    bs_gfxbuf_t               *blurred_gfxbuf_ptr;

    /** Current width of the title bar. */
    unsigned                  width;
    /** Whether the title bar is currently displayed as activated. */
    bool                      activated;

    /** Title bar style. */
    wlmtk_titlebar_style_t    style;
};

/** State of the title bar's title. */
struct _wlmtk_titlebar_title_t {
    /** Superclass: Buffer. */
    wlmtk_buffer_t            super_buffer;

    /** The drawn title, when focussed. */
    struct wlr_buffer         *focussed_wlr_buffer_ptr;
    /** The drawn title, when blurred. */
    struct wlr_buffer         *blurred_wlr_buffer_ptr;
};

static wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const wlmtk_titlebar_style_t *style_ptr);
static void wlmtk_titlebar_title_destroy(wlmtk_titlebar_title_t *title_ptr);
static bool wlmtk_titlebar_title_redraw(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const wlmtk_titlebar_style_t *style_ptr);

static void titlebar_box_destroy(wlmtk_box_t *box_ptr);
static bool redraw_buffers(
    wlmtk_titlebar_t *titlebar_ptr,
    unsigned width);

static void title_buffer_destroy(wlmtk_buffer_t *buffer_ptr);
static void title_set_activated(
    wlmtk_titlebar_title_t *title_ptr,
    bool activated);
static bool title_redraw_buffers(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_titlebar_style_t *style_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t titlebar_box_impl = {
    .destroy = titlebar_box_destroy
};

/** Buffer implementation for title of the title bar. */
static const wlmtk_buffer_impl_t title_buffer_impl = {
    .destroy = title_buffer_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_t *wlmtk_titlebar_create(
    unsigned width,
    const wlmtk_titlebar_style_t *style_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;
    memcpy(&titlebar_ptr->style, style_ptr, sizeof(wlmtk_titlebar_style_t));

    if (!wlmtk_box_init(&titlebar_ptr->super_box,
                        &titlebar_box_impl,
                        WLMTK_BOX_HORIZONTAL)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }

    if (!redraw_buffers(titlebar_ptr, width)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }

    titlebar_ptr->title_ptr = wlmtk_titlebar_title_create(
        titlebar_ptr->focussed_gfxbuf_ptr,
        titlebar_ptr->blurred_gfxbuf_ptr,
        0,
        titlebar_ptr->width,
        titlebar_ptr->activated,
        &titlebar_ptr->style);
    if (NULL == titlebar_ptr->title_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &titlebar_ptr->title_ptr->super_buffer.super_element, true);
    wlmtk_container_add_element(
        &titlebar_ptr->super_box.super_container,
        &titlebar_ptr->title_ptr->super_buffer.super_element);

    return titlebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_destroy(wlmtk_titlebar_t *titlebar_ptr)
{
    if (NULL != titlebar_ptr->title_ptr) {
        wlmtk_container_remove_element(
            &titlebar_ptr->super_box.super_container,
            &titlebar_ptr->title_ptr->super_buffer.super_element);
        wlmtk_titlebar_title_destroy(titlebar_ptr->title_ptr);
        titlebar_ptr->title_ptr = NULL;
    }

    if (NULL != titlebar_ptr->blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->blurred_gfxbuf_ptr);
        titlebar_ptr->blurred_gfxbuf_ptr = NULL;
    }
    if (NULL != titlebar_ptr->focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->focussed_gfxbuf_ptr);
        titlebar_ptr->focussed_gfxbuf_ptr = NULL;
    }

    wlmtk_box_fini(&titlebar_ptr->super_box);

    free(titlebar_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_titlebar_set_width(
    wlmtk_titlebar_t *titlebar_ptr,
    unsigned width)
{
    if (titlebar_ptr->width == width) return true;
    if (!redraw_buffers(titlebar_ptr, width)) return false;
    BS_ASSERT(width == titlebar_ptr->width);

    if (!wlmtk_titlebar_title_redraw(
            titlebar_ptr->title_ptr,
            titlebar_ptr->focussed_gfxbuf_ptr,
            titlebar_ptr->blurred_gfxbuf_ptr,
            0,
            titlebar_ptr->width,
            titlebar_ptr->activated,
            &titlebar_ptr->style)) {
        return false;
    }

    // Don't forget to re-position the elements.
    wlmtk_container_update_layout(&titlebar_ptr->super_box.super_container);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_set_activated(
    wlmtk_titlebar_t *titlebar_ptr,
    bool activated)
{
    if (titlebar_ptr->activated == activated) return;
    titlebar_ptr->activated = activated;
    title_set_activated(titlebar_ptr->title_ptr, titlebar_ptr->activated);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_titlebar_element(wlmtk_titlebar_t *titlebar_ptr)
{
    return &titlebar_ptr->super_box.super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from box. Wraps to our dtor. */
void titlebar_box_destroy(wlmtk_box_t *box_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = BS_CONTAINER_OF(
        box_ptr, wlmtk_titlebar_t, super_box);
    wlmtk_titlebar_destroy(titlebar_ptr);
}

/* ------------------------------------------------------------------------- */
/** Redraws the titlebar's background in appropriate size. */
bool redraw_buffers(wlmtk_titlebar_t *titlebar_ptr, unsigned width)
{
    cairo_t *cairo_ptr;

    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(
        width, titlebar_ptr->style.height);
    if (NULL == focussed_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(focussed_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(
        cairo_ptr, &titlebar_ptr->style.focussed_fill);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(
        width, titlebar_ptr->style.height);
    if (NULL == blurred_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(blurred_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(
        cairo_ptr, &titlebar_ptr->style.blurred_fill);
    cairo_destroy(cairo_ptr);

    if (NULL != titlebar_ptr->focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->focussed_gfxbuf_ptr);
    }
    titlebar_ptr->focussed_gfxbuf_ptr = focussed_gfxbuf_ptr;
    if (NULL != titlebar_ptr->blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->blurred_gfxbuf_ptr);
    }
    titlebar_ptr->blurred_gfxbuf_ptr = blurred_gfxbuf_ptr;
    titlebar_ptr->width = width;
    return true;
}

/* == Title buffer methods ================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Creates a title bar title.
 *
 * @param focussed_gfxbuf_ptr Titlebar background when focussed.
 * @param blurred_gfxbuf_ptr  Titlebar background when blurred.
 * @param position            Position of title telative to titlebar.
 * @param width               Width of title.
 * @param activated           Whether the title bar should start focussed.
 * @param style_ptr
 *
 * @return Title handle.
 */
wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const wlmtk_titlebar_style_t *style_ptr)
{
    wlmtk_titlebar_title_t *title_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_title_t));
    if (NULL == title_ptr) return NULL;

    if (!wlmtk_buffer_init(
            &title_ptr->super_buffer,
            &title_buffer_impl)) {
        wlmtk_titlebar_title_destroy(title_ptr);
        return NULL;
    }

    if (!title_redraw_buffers(
            title_ptr,
            focussed_gfxbuf_ptr,
            blurred_gfxbuf_ptr,
            position, width, style_ptr)) {
        wlmtk_titlebar_title_destroy(title_ptr);
        return NULL;
    }

    title_set_activated(title_ptr, activated);
    return title_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Redraws the title section of the title bar.
 *
 * @param title_ptr
 * @param focussed_gfxbuf_ptr Titlebar background when focussed.
 * @param blurred_gfxbuf_ptr  Titlebar background when blurred.
 * @param position            Position of title telative to titlebar.
 * @param width               Width of title.
 * @param activated           Whether the title bar should start focussed.
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_titlebar_title_redraw(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const wlmtk_titlebar_style_t *style_ptr)
{
    if (!title_redraw_buffers(
            title_ptr,
            focussed_gfxbuf_ptr,
            blurred_gfxbuf_ptr,
            position, width, style_ptr)) {
        return false;
    }
    title_set_activated(title_ptr, activated);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the titlebar title.
 *
 * @param title_ptr
 */
void wlmtk_titlebar_title_destroy(wlmtk_titlebar_title_t *title_ptr)
{
    if (NULL != title_ptr->focussed_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->focussed_wlr_buffer_ptr);
        title_ptr->focussed_wlr_buffer_ptr = NULL;
    }
    if (NULL != title_ptr->blurred_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->blurred_wlr_buffer_ptr);
        title_ptr->blurred_wlr_buffer_ptr = NULL;
    }

    wlmtk_buffer_fini(&title_ptr->super_buffer);
    free(title_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor. Forwards to @ref wlmtk_titlebar_title_destroy. */
void title_buffer_destroy(wlmtk_buffer_t *buffer_ptr)
{
    wlmtk_titlebar_title_t *title_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_titlebar_title_t, super_buffer);
    wlmtk_titlebar_title_destroy(title_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets whether the title is drawn focussed (activated) or blurred.
 *
 * @param title_ptr
 * @param activated
 */
void title_set_activated(wlmtk_titlebar_title_t *title_ptr, bool activated)
{
    wlmtk_buffer_set(
        &title_ptr->super_buffer,
        activated ?
        title_ptr->focussed_wlr_buffer_ptr :
        title_ptr->blurred_wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/** Redraws the title buffers. */
bool title_redraw_buffers(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_titlebar_style_t *style_ptr)
{
    cairo_t *cairo_ptr;

    BS_ASSERT(focussed_gfxbuf_ptr->width == blurred_gfxbuf_ptr->width);
    BS_ASSERT(style_ptr->height == focussed_gfxbuf_ptr->height);
    BS_ASSERT(style_ptr->height == blurred_gfxbuf_ptr->height);
    BS_ASSERT(position <= focussed_gfxbuf_ptr->width);
    BS_ASSERT(position + width <= focussed_gfxbuf_ptr->width);

    struct wlr_buffer *focussed_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, style_ptr->height);
    if (NULL == focussed_wlr_buffer_ptr) return false;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(focussed_wlr_buffer_ptr),
        0, 0,
        focussed_gfxbuf_ptr,
        position, 0,
        width, style_ptr->height);

    cairo_ptr = cairo_create_from_wlr_buffer(focussed_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(focussed_wlr_buffer_ptr);
        return false;
    }

    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, style_ptr->height, 1.0, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr, "Title", style_ptr->focussed_text_color);

    cairo_destroy(cairo_ptr);

    struct wlr_buffer *blurred_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, style_ptr->height);
    if (NULL == blurred_wlr_buffer_ptr) {
        wlr_buffer_drop(focussed_wlr_buffer_ptr);
        return false;
    }

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(blurred_wlr_buffer_ptr),
        0, 0,
        blurred_gfxbuf_ptr,
        position, 0,
        width, style_ptr->height);

    cairo_ptr = cairo_create_from_wlr_buffer(blurred_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(blurred_wlr_buffer_ptr);
        return false;
    }

    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, style_ptr->height, 1.0, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr, "Title", style_ptr->blurred_text_color);

    cairo_destroy(cairo_ptr);

    if (NULL != title_ptr->focussed_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->focussed_wlr_buffer_ptr);
    }
    title_ptr->focussed_wlr_buffer_ptr = focussed_wlr_buffer_ptr;
    if (NULL != title_ptr->blurred_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->blurred_wlr_buffer_ptr);
    }
    title_ptr->blurred_wlr_buffer_ptr = blurred_wlr_buffer_ptr;
    return true;
}


/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_create_empty(bs_test_t *test_ptr);
static void test_title(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_titlebar_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "create_empty", test_create_empty },
    { 1, "title", test_title },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_titlebar_style_t style = {};
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(120, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_ptr);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_empty(bs_test_t *test_ptr)
{
    wlmtk_titlebar_style_t style = {};
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(0, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_ptr);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Tests title drawing. */
void test_title(bs_test_t *test_ptr)
{
    const wlmtk_titlebar_style_t style = {
        .focussed_text_color = 0xffc0c0c0,
        .blurred_text_color = 0xff808080,
        .height = 22,
    };
    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(120, 22);
    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(120, 22);
    bs_gfxbuf_clear(focussed_gfxbuf_ptr, 0xff2020c0);
    bs_gfxbuf_clear(blurred_gfxbuf_ptr, 0xff404040);

    wlmtk_titlebar_title_t *title_ptr = wlmtk_titlebar_title_create(
        focussed_gfxbuf_ptr, blurred_gfxbuf_ptr, 10, 90, true, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, title_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->focussed_wlr_buffer_ptr),
        "toolkit/title_focussed.png");
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->blurred_wlr_buffer_ptr),
        "toolkit/title_blurred.png");

    // We had started as "activated", verify that's correct.
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/title_focussed.png");

    // De-activated the title. Verify that was propagated.
    title_set_activated(title_ptr, false);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/title_blurred.png");

    // Redraw with shorter width. Verify that's still correct.
    wlmtk_titlebar_title_redraw(
        title_ptr, focussed_gfxbuf_ptr, blurred_gfxbuf_ptr,
        10, 70, false, &style);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/title_blurred_short.png");

    wlmtk_element_destroy(&title_ptr->super_buffer.super_element);
    bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
    bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
}

/* == End of titlebar.c ==================================================== */
