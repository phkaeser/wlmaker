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

/** State of the title bar. */
struct _wlmtk_titlebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Titlebar background, when focussed. */
    bs_gfxbuf_t               *focussed_gfxbuf_ptr;
    /** Titlebar background, when blurred. */
    bs_gfxbuf_t               *blurred_gfxbuf_ptr;
};

/** State of the title bar's title. */
typedef struct {
    /** Superclass; Buffer. */
    wlmtk_buffer_t            super_buffer;

    /** The drawn title, when focussed. */
    struct wlr_buffer         *focussed_wlr_buffer_ptr;
    /** The drawn title, when blurred. */
    struct wlr_buffer         *blurred_wlr_buffer_ptr;
} wlmtk_titlebar_title_t;

wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated);
void wlmtk_titlebar_title_destroy(wlmtk_titlebar_title_t *title_ptr);

static void titlebar_box_destroy(wlmtk_box_t *box_ptr);
static bool redraw_buffers(wlmtk_titlebar_t *titlebar_ptr);

/* == Data ================================================================= */

/** Hardcoded: Height of the titlebar, in pixels. */
static const unsigned         titlebar_height = 22;

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t titlebar_box_impl = {
    .destroy = titlebar_box_destroy
};


/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_t *wlmtk_titlebar_create(void)
{
    wlmtk_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;

    if (!wlmtk_box_init(&titlebar_ptr->super_box,
                        &titlebar_box_impl,
                        WLMTK_BOX_HORIZONTAL)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }

    if (!redraw_buffers(titlebar_ptr)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }

    return titlebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_destroy(wlmtk_titlebar_t *titlebar_ptr)
{
    wlmtk_box_fini(&titlebar_ptr->super_box);

    if (NULL != titlebar_ptr->blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->blurred_gfxbuf_ptr);
        titlebar_ptr->blurred_gfxbuf_ptr = NULL;
    }
    if (NULL != titlebar_ptr->focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->focussed_gfxbuf_ptr);
        titlebar_ptr->focussed_gfxbuf_ptr = NULL;
    }

    free(titlebar_ptr);
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
bool redraw_buffers(wlmtk_titlebar_t *titlebar_ptr)
{
    cairo_t *cairo_ptr;
    int width = 120;

    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(
        width, titlebar_height);
    if (NULL == focussed_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(focussed_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(
        width, titlebar_height);
    if (NULL == blurred_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(focussed_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    cairo_destroy(cairo_ptr);

    if (NULL != titlebar_ptr->focussed_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->focussed_gfxbuf_ptr);
    }
    titlebar_ptr->focussed_gfxbuf_ptr = focussed_gfxbuf_ptr;
    if (NULL != titlebar_ptr->blurred_gfxbuf_ptr) {
        bs_gfxbuf_destroy(titlebar_ptr->blurred_gfxbuf_ptr);
    }
    titlebar_ptr->blurred_gfxbuf_ptr = blurred_gfxbuf_ptr;
    return true;
}

/* == Title buffer methods ================================================= */

static void title_buffer_destroy(wlmtk_buffer_t *buffer_ptr);
bool title_redraw_buffers(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    unsigned position,
    unsigned width);

/** Buffer implementation for title of the title bar. */
static const wlmtk_buffer_impl_t title_buffer_impl = {
    .destroy = title_buffer_destroy
};

/* ------------------------------------------------------------------------- */
/**
 * Creates a title bar title.
 *
 * @param focussed_gfxbuf_ptr Titlebar background when focussed.
 * @param blurred_gfxbuf_ptr  Titlebar background when blurred.
 * @param position            Position of title telative to titlebar.
 * @param width               Width of title.
 * @param activated           Whether the title bar should start focussed.
 *
 * @return Title handle.
 */
wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated)
{
    wlmtk_titlebar_title_t *title_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_title_t));
    if (NULL == title_ptr) return NULL;

    if (!title_redraw_buffers(
            title_ptr,
            focussed_gfxbuf_ptr,
            blurred_gfxbuf_ptr,
            position, width)) {
        wlmtk_titlebar_title_destroy(title_ptr);
        return NULL;
    }

    if (!wlmtk_buffer_init(
            &title_ptr->super_buffer,
            &title_buffer_impl,
            title_ptr->focussed_wlr_buffer_ptr)) {
        wlmtk_titlebar_title_destroy(title_ptr);
        return NULL;
    }

    wlmtk_buffer_set(
        &title_ptr->super_buffer,
        activated ? title_ptr->focussed_wlr_buffer_ptr : title_ptr->blurred_wlr_buffer_ptr);
    return title_ptr;
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
/** Redraws the title buffers. */
bool title_redraw_buffers(
    wlmtk_titlebar_title_t *title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    unsigned position,
    unsigned width)
{
    cairo_t *cairo_ptr;

    BS_ASSERT(focussed_gfxbuf_ptr->width == blurred_gfxbuf_ptr->width);
    BS_ASSERT(titlebar_height == focussed_gfxbuf_ptr->height);
    BS_ASSERT(titlebar_height == blurred_gfxbuf_ptr->height);
    BS_ASSERT(position < focussed_gfxbuf_ptr->width);
    BS_ASSERT(position + width < focussed_gfxbuf_ptr->width);

    struct wlr_buffer *focussed_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, titlebar_height);
    if (NULL == focussed_wlr_buffer_ptr) return false;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(focussed_wlr_buffer_ptr),
        0, 0,
        focussed_gfxbuf_ptr,
        position, 0,
        width, titlebar_height);

    cairo_ptr = cairo_create_from_wlr_buffer(focussed_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(focussed_wlr_buffer_ptr);
        return false;
    }

    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, titlebar_height, 1.0, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr, "Title", 0xffc0c0c0);

    cairo_destroy(cairo_ptr);

    struct wlr_buffer *blurred_wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, titlebar_height);
    if (NULL == blurred_wlr_buffer_ptr) {
        wlr_buffer_drop(focussed_wlr_buffer_ptr);
        return false;
    }

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(blurred_wlr_buffer_ptr),
        0, 0,
        blurred_gfxbuf_ptr,
        position, 0,
        width, titlebar_height);

    cairo_ptr = cairo_create_from_wlr_buffer(blurred_wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(blurred_wlr_buffer_ptr);
        return false;
    }

    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, titlebar_height, 1.0, true);
    wlmaker_primitives_draw_window_title(
        cairo_ptr, "Title", 0xff808080);

    cairo_destroy(cairo_ptr);

    if (NULL == title_ptr->focussed_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->focussed_wlr_buffer_ptr);
    }
    title_ptr->focussed_wlr_buffer_ptr = focussed_wlr_buffer_ptr;
    if (NULL == title_ptr->blurred_wlr_buffer_ptr) {
        wlr_buffer_drop(title_ptr->blurred_wlr_buffer_ptr);
    }
    title_ptr->blurred_wlr_buffer_ptr = blurred_wlr_buffer_ptr;
    return true;
}


/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_title(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_titlebar_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "title", test_title },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_ptr);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Tests title drawing. */
void test_title(bs_test_t *test_ptr)
{
    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(120, titlebar_height);
    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(120, titlebar_height);
    bs_gfxbuf_clear(focussed_gfxbuf_ptr, 0xff2020c0);
    bs_gfxbuf_clear(blurred_gfxbuf_ptr, 0xff404040);

    wlmtk_titlebar_title_t *title_ptr = wlmtk_titlebar_title_create(
        focussed_gfxbuf_ptr, blurred_gfxbuf_ptr, 10, 90, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, title_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->focussed_wlr_buffer_ptr),
        "toolkit/title_focussed.png");
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(title_ptr->blurred_wlr_buffer_ptr),
        "toolkit/title_blurred.png");

    wlmtk_element_destroy(&title_ptr->super_buffer.super_element);

    bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
    bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
}

/* == End of titlebar.c ==================================================== */
