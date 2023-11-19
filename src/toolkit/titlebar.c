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
#include "button.h"
#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"
#include "titlebar_button.h"
#include "titlebar_title.h"
#include "window.h"

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the title bar. */
struct _wlmtk_titlebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;
    /** Link to the titlebar's title. */
    const char                *title_ptr;

    /** Title element of the title bar. */
    wlmtk_titlebar_title_t    *titlebar_title_ptr;

    /** Minimize button. */
    wlmtk_titlebar_button_t  *minimize_button_ptr;
    /** Close button. */
    wlmtk_titlebar_button_t  *close_button_ptr;

    /** Titlebar background, when focussed. */
    bs_gfxbuf_t               *focussed_gfxbuf_ptr;
    /** Titlebar background, when blurred. */
    bs_gfxbuf_t               *blurred_gfxbuf_ptr;

    /** Current width of the title bar. */
    unsigned                  width;
    /** Position of the close button. */
    int                       close_position;
    /** Position of the title element. */
    int                       title_position;
    /** Whether the title bar is currently displayed as activated. */
    bool                      activated;

    /** Title bar style. */
    wlmtk_titlebar_style_t    style;
};

static void titlebar_box_destroy(wlmtk_box_t *box_ptr);
static bool redraw_buffers(
    wlmtk_titlebar_t *titlebar_ptr,
    unsigned width);
static bool redraw(wlmtk_titlebar_t *titlebar_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t titlebar_box_impl = {
    .destroy = titlebar_box_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_t *wlmtk_titlebar_create(
    wlmtk_window_t *window_ptr,
    const wlmtk_titlebar_style_t *style_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;
    memcpy(&titlebar_ptr->style, style_ptr, sizeof(wlmtk_titlebar_style_t));
    titlebar_ptr->title_ptr = wlmtk_window_get_title(window_ptr);

    if (!wlmtk_box_init(&titlebar_ptr->super_box,
                        &titlebar_box_impl,
                        WLMTK_BOX_HORIZONTAL)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }

    titlebar_ptr->titlebar_title_ptr = wlmtk_titlebar_title_create(window_ptr);
    if (NULL == titlebar_ptr->titlebar_title_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &titlebar_ptr->super_box.super_container,
        wlmtk_titlebar_title_element(titlebar_ptr->titlebar_title_ptr));

    titlebar_ptr->minimize_button_ptr = wlmtk_titlebar_button_create(
        wlmtk_window_request_minimize,
        window_ptr,
        wlmaker_primitives_draw_minimize_icon);
    if (NULL == titlebar_ptr->minimize_button_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &titlebar_ptr->super_box.super_container,
        wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr));

    titlebar_ptr->close_button_ptr = wlmtk_titlebar_button_create(
        wlmtk_window_request_close,
        window_ptr,
        wlmaker_primitives_draw_close_icon);
    if (NULL == titlebar_ptr->close_button_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_container_add_element_before(
        &titlebar_ptr->super_box.super_container, NULL,
        wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr));

    return titlebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_destroy(wlmtk_titlebar_t *titlebar_ptr)
{
    if (NULL != titlebar_ptr->close_button_ptr) {
        wlmtk_container_remove_element(
            &titlebar_ptr->super_box.super_container,
            wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr));
        wlmtk_titlebar_button_destroy(titlebar_ptr->close_button_ptr);
        titlebar_ptr->close_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->minimize_button_ptr) {
        wlmtk_container_remove_element(
            &titlebar_ptr->super_box.super_container,
            wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr));
        wlmtk_titlebar_button_destroy(titlebar_ptr->minimize_button_ptr);
        titlebar_ptr->minimize_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->titlebar_title_ptr) {
        wlmtk_container_remove_element(
            &titlebar_ptr->super_box.super_container,
            wlmtk_titlebar_title_element(titlebar_ptr->titlebar_title_ptr));
        wlmtk_titlebar_title_destroy(titlebar_ptr->titlebar_title_ptr);
        titlebar_ptr->titlebar_title_ptr = NULL;
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

    titlebar_ptr->close_position = width;
    if (3 * titlebar_ptr->style.height < width) {
        titlebar_ptr->close_position = width - titlebar_ptr->style.height;
    }
    titlebar_ptr->title_position = 0;
    if (4 * titlebar_ptr->style.height < width) {
        titlebar_ptr->title_position = titlebar_ptr->style.height;
    }

    if (!redraw(titlebar_ptr)) {
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
    wlmtk_titlebar_button_set_activated(
        titlebar_ptr->minimize_button_ptr, titlebar_ptr->activated);
    wlmtk_titlebar_title_set_activated(
        titlebar_ptr->titlebar_title_ptr, titlebar_ptr->activated);
    wlmtk_titlebar_button_set_activated(
        titlebar_ptr->close_button_ptr, titlebar_ptr->activated);
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_set_title(
    wlmtk_titlebar_t *titlebar_ptr,
    const char *title_ptr)
{
    if (titlebar_ptr->title_ptr == title_ptr) return;

    titlebar_ptr->title_ptr = title_ptr;
    redraw(titlebar_ptr);
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

/* ------------------------------------------------------------------------- */
/** Redraws the titlebar elements. */
bool redraw(wlmtk_titlebar_t *titlebar_ptr)
{
    // Guard clause: Nothing to do... yet.
    if (0 >= titlebar_ptr->width) return true;

    if (!wlmtk_titlebar_title_redraw(
            titlebar_ptr->titlebar_title_ptr,
            titlebar_ptr->focussed_gfxbuf_ptr,
            titlebar_ptr->blurred_gfxbuf_ptr,
            titlebar_ptr->title_position,
            titlebar_ptr->close_position - titlebar_ptr->title_position,
            titlebar_ptr->activated,
            titlebar_ptr->title_ptr,
            &titlebar_ptr->style)) {
        return false;
    }
    wlmtk_element_set_visible(
        wlmtk_titlebar_title_element(titlebar_ptr->titlebar_title_ptr), true);

    if (0 < titlebar_ptr->title_position) {
        if (!wlmtk_titlebar_button_redraw(
                titlebar_ptr->minimize_button_ptr,
                titlebar_ptr->focussed_gfxbuf_ptr,
                titlebar_ptr->blurred_gfxbuf_ptr,
                0,
                &titlebar_ptr->style)) {
            return false;
        }
        wlmtk_element_set_visible(
            wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr),
            true);
    } else {
        wlmtk_element_set_visible(
            wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr),
            false);
    }

    if (titlebar_ptr->close_position < (int)titlebar_ptr->width) {
        if (!wlmtk_titlebar_button_redraw(
                titlebar_ptr->close_button_ptr,
                titlebar_ptr->focussed_gfxbuf_ptr,
                titlebar_ptr->blurred_gfxbuf_ptr,
                titlebar_ptr->close_position,
                &titlebar_ptr->style)) {
            return false;
        }
        wlmtk_element_set_visible(
            wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr),
            true);
    } else {
        wlmtk_element_set_visible(
            wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr),
            false);
    }

    return true;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_variable_width(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_titlebar_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "variable_width", test_variable_width },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    wlmtk_titlebar_style_t style = {};
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(
        &fake_window_ptr->window, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_ptr);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests titlebar with variable width. */
void test_variable_width(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    wlmtk_titlebar_style_t style = { .height = 22 };
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(
        &fake_window_ptr->window, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, titlebar_ptr);

    // Short names, for improved readability.
    wlmtk_element_t *title_elem_ptr = wlmtk_titlebar_title_element(
        titlebar_ptr->titlebar_title_ptr);
    wlmtk_element_t *minimize_elem_ptr = wlmtk_titlebar_button_element(
        titlebar_ptr->minimize_button_ptr);
    wlmtk_element_t *close_elem_ptr = wlmtk_titlebar_button_element(
        titlebar_ptr->close_button_ptr);
    int width;

    // Created with zero width: All invisible. */
    BS_TEST_VERIFY_FALSE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, close_elem_ptr->visible);

    // Width sufficient for all: All elements visible and placed.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 89));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 22, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 45, width);
    BS_TEST_VERIFY_EQ(test_ptr, 67, close_elem_ptr->x);

    // Width sufficient only for 1 button.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 67));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 45, width);

    // Width doesn't permit any button.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 66));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 66, width);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* == End of titlebar.c ==================================================== */
