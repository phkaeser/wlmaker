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

#include <cairo.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdlib.h>
#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

#include "box.h"
#include "primitives.h"
#include "style.h"
#include "titlebar_button.h"
#include "titlebar_title.h"

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
    /** Width of the title element. */
    int                       title_width;
    /** Whether the title bar is currently displayed as activated. */
    bool                      activated;

    /** Properties of the title bar. */
    uint32_t                  properties;

    /** Title bar style. */
    const struct wlmtk_titlebar_style *style_ptr;
};

static void _wlmtk_titlebar_element_destroy(wlmtk_element_t *element_ptr);
static void _wlmtk_titlebar_compute_positions(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr);
static bool _wlmtk_titlebar_redraw_buffers(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr,
    unsigned width);
static bool _wlmtk_titlebar_redraw(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr);

/* == Data ================================================================= */

const bspl_desc_t wlmtk_titlebar_style_desc[] = {
    BSPL_DESC_CUSTOM(
        "FocussedFill", true, struct wlmtk_titlebar_style, focussed_fill,
        focussed_fill, wlmtk_style_decode_fill, NULL, NULL, NULL),
    BSPL_DESC_ARGB32(
        "FocussedTextColor", true, struct wlmtk_titlebar_style,
        focussed_text_color, focussed_text_color, 0),
    BSPL_DESC_CUSTOM(
        "BlurredFill", true, struct wlmtk_titlebar_style, blurred_fill,
        blurred_fill, wlmtk_style_decode_fill, NULL, NULL, NULL),
    BSPL_DESC_ARGB32(
        "BlurredTextColor", true, struct wlmtk_titlebar_style,
        blurred_text_color, blurred_text_color, 0),
    BSPL_DESC_UINT64(
        "Height", true, struct wlmtk_titlebar_style, height, height, 22),
    BSPL_DESC_UINT64(
        "BezelWidth", true, struct wlmtk_titlebar_style,
        bezel_width, bezel_width, 1),
    BSPL_DESC_DICT(
        "Margin", true, struct wlmtk_titlebar_style, margin, margin,
        wlmtk_style_margin_desc),
    BSPL_DESC_DICT(
        "Font", true, struct wlmtk_titlebar_style, font, font,
        wlmtk_style_font_desc),
    BSPL_DESC_SENTINEL()
};

/** Virtual method table extension for the titlebar's element superclass. */
static const wlmtk_element_vmt_t titlebar_element_vmt = {
    .destroy = _wlmtk_titlebar_element_destroy
};

/** Default properties: All buttons shown. */
static const uint32_t _wlmtk_titlebar_default_properties =
    WLMTK_TITLEBAR_PROPERTY_ICONIFY |
    WLMTK_TITLEBAR_PROPERTY_CLOSE;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_titlebar_t *wlmtk_titlebar_create(
    wlmtk_window_t *window_ptr,
    const struct wlmtk_titlebar_style *style_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = logged_calloc(
        1, sizeof(wlmtk_titlebar_t));
    if (NULL == titlebar_ptr) return NULL;
    titlebar_ptr->style_ptr = style_ptr;
    titlebar_ptr->title_ptr = wlmtk_window_get_title(window_ptr);

    if (!wlmtk_box_init(&titlebar_ptr->super_box,
                        WLMTK_BOX_HORIZONTAL,
                        &titlebar_ptr->style_ptr->margin)) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        &titlebar_ptr->super_box.super_container.super_element,
        &titlebar_element_vmt);

    titlebar_ptr->titlebar_title_ptr = wlmtk_titlebar_title_create(window_ptr);
    if (NULL == titlebar_ptr->titlebar_title_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_box_add_element_front(
        &titlebar_ptr->super_box,
        wlmtk_titlebar_title_element(titlebar_ptr->titlebar_title_ptr));

    titlebar_ptr->minimize_button_ptr = wlmtk_titlebar_button_create(
        wlmtk_window_request_minimize,
        window_ptr,
        wlmaker_primitives_draw_minimize_icon);
    if (NULL == titlebar_ptr->minimize_button_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_box_add_element_front(
        &titlebar_ptr->super_box,
        wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr));

    titlebar_ptr->close_button_ptr = wlmtk_titlebar_button_create(
        wlmtk_window_request_close,
        window_ptr,
        wlmaker_primitives_draw_close_icon);
    if (NULL == titlebar_ptr->close_button_ptr) {
        wlmtk_titlebar_destroy(titlebar_ptr);
        return NULL;
    }
    wlmtk_box_add_element_back(
        &titlebar_ptr->super_box,
        wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr));

    wlmtk_titlebar_set_properties(
        titlebar_ptr,
        _wlmtk_titlebar_default_properties);
    return titlebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_destroy(wlmtk_titlebar_t *titlebar_ptr)
{
    if (NULL != titlebar_ptr->close_button_ptr) {
        wlmtk_box_remove_element(
            &titlebar_ptr->super_box,
            wlmtk_titlebar_button_element(titlebar_ptr->close_button_ptr));
        wlmtk_titlebar_button_destroy(titlebar_ptr->close_button_ptr);
        titlebar_ptr->close_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->minimize_button_ptr) {
        wlmtk_box_remove_element(
            &titlebar_ptr->super_box,
            wlmtk_titlebar_button_element(titlebar_ptr->minimize_button_ptr));
        wlmtk_titlebar_button_destroy(titlebar_ptr->minimize_button_ptr);
        titlebar_ptr->minimize_button_ptr = NULL;
    }

    if (NULL != titlebar_ptr->titlebar_title_ptr) {
        wlmtk_box_remove_element(
            &titlebar_ptr->super_box,
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
    wlmtk_titlebar_t *tb,
    unsigned width)
{
    if (tb->width == width) return true;
    if (!_wlmtk_titlebar_redraw_buffers(tb, tb->style_ptr, width)) return false;
    BS_ASSERT(width == tb->width);
    return _wlmtk_titlebar_redraw(tb, tb->style_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_titlebar_set_style(
    wlmtk_titlebar_t *tb,
    const struct wlmtk_titlebar_style *style_ptr)
{
    wlmtk_box_set_style(&tb->super_box, &style_ptr->margin);

    if (!_wlmtk_titlebar_redraw_buffers(tb, style_ptr, tb->width) ||
        !_wlmtk_titlebar_redraw(tb, style_ptr)) {
        return false;
    }

    tb->style_ptr = style_ptr;
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_set_properties(
    wlmtk_titlebar_t *titlebar_ptr,
    uint32_t properties)
{
    if (titlebar_ptr->properties == properties) return;
    titlebar_ptr->properties = properties;

    (void)_wlmtk_titlebar_redraw(titlebar_ptr, titlebar_ptr->style_ptr);
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
bool wlmtk_titlebar_is_activated(wlmtk_titlebar_t *titlebar_ptr)
{
    return titlebar_ptr->activated;
}

/* ------------------------------------------------------------------------- */
void wlmtk_titlebar_set_title(
    wlmtk_titlebar_t *titlebar_ptr,
    const char *title_ptr)
{
    if (titlebar_ptr->title_ptr == title_ptr) return;

    titlebar_ptr->title_ptr = title_ptr;
    _wlmtk_titlebar_redraw(titlebar_ptr, titlebar_ptr->style_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_titlebar_element(wlmtk_titlebar_t *titlebar_ptr)
{
    return &titlebar_ptr->super_box.super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, wraps to our dtor. */
void _wlmtk_titlebar_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_titlebar_t *titlebar_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_titlebar_t,
        super_box.super_container.super_element);
    wlmtk_titlebar_destroy(titlebar_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Compute positions of the titlebar elements, if configured.
 *
 * This method updates @ref wlmtk_titlebar_t::close_position, @ref
 * wlmtk_titlebar_t::title_position and @ref wlmtk_titlebar_t::title_width.
 *
 * @param titlebar_ptr
 * @param style_ptr
 */
void _wlmtk_titlebar_compute_positions(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr)
{
    titlebar_ptr->title_width = titlebar_ptr->width;

    // Room for a close button?
    titlebar_ptr->close_position = titlebar_ptr->width;
    if (3 * style_ptr->height < titlebar_ptr->width &&
        (titlebar_ptr->properties & WLMTK_TITLEBAR_PROPERTY_CLOSE)) {
        titlebar_ptr->close_position =
            titlebar_ptr->width - style_ptr->height;
        titlebar_ptr->title_width -=
            style_ptr->height + style_ptr->margin.width;
    }

    titlebar_ptr->title_position = 0;
    // Also having room for a minimize button?
    if (4 * style_ptr->height < titlebar_ptr->width &&
        (titlebar_ptr->properties & WLMTK_TITLEBAR_PROPERTY_ICONIFY)) {
        titlebar_ptr->title_position =
            style_ptr->height + style_ptr->margin.width;
        titlebar_ptr->title_width -=
            style_ptr->height + style_ptr->margin.width;
    }
}

/* ------------------------------------------------------------------------- */
/** Redraws the titlebar's background in appropriate size. */
bool _wlmtk_titlebar_redraw_buffers(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr,
    unsigned width)
{
    cairo_t *cairo_ptr;

    bs_gfxbuf_t *focussed_gfxbuf_ptr = bs_gfxbuf_create(
        width, style_ptr->height);
    if (NULL == focussed_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(focussed_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(cairo_ptr, &style_ptr->focussed_fill);
    cairo_destroy(cairo_ptr);

    bs_gfxbuf_t *blurred_gfxbuf_ptr = bs_gfxbuf_create(
        width, style_ptr->height);
    if (NULL == blurred_gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(blurred_gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(blurred_gfxbuf_ptr);
        bs_gfxbuf_destroy(focussed_gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(cairo_ptr, &style_ptr->blurred_fill);
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
bool _wlmtk_titlebar_redraw(
    wlmtk_titlebar_t *titlebar_ptr,
    const struct wlmtk_titlebar_style *style_ptr)
{
    // Guard clause: Nothing to do... yet.
    if (0 >= titlebar_ptr->width) return true;

    _wlmtk_titlebar_compute_positions(titlebar_ptr, style_ptr);

    if (!wlmtk_titlebar_title_redraw(
            titlebar_ptr->titlebar_title_ptr,
            titlebar_ptr->focussed_gfxbuf_ptr,
            titlebar_ptr->blurred_gfxbuf_ptr,
            titlebar_ptr->title_position,
            titlebar_ptr->title_width,
            titlebar_ptr->activated,
            titlebar_ptr->title_ptr,
            style_ptr)) {
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
                style_ptr)) {
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
                style_ptr)) {
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


    // Don't forget to re-position the elements.
    wlmtk_element_layout(wlmtk_box_element(&titlebar_ptr->super_box));
    wlmtk_element_invalidate_parent_layout(
        wlmtk_box_element(&titlebar_ptr->super_box));
    return true;
}

/* == Unit tests =========================================================== */

static void test_variable_width(bs_test_t *test_ptr);
static void test_properties(bs_test_t *test_ptr);

/** Test cases */
static const bs_test_case_t _wlmtk_titlebar_test_cases[] = {
    { 1, "variable_width", test_variable_width },
    { 1, "properties", test_properties },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmtk_titlebar_test_set = BS_TEST_SET(
    true, "titlebar", _wlmtk_titlebar_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests titlebar with variable width. */
void test_variable_width(bs_test_t *test_ptr)
{
    wlmtk_window_t *w = wlmtk_test_window_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    struct wlmtk_titlebar_style style = { .height = 22, .margin = { .width = 2 } };
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(w, &style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, titlebar_ptr);

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
    BS_TEST_VERIFY_EQ(test_ptr, 24, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 41, width);
    BS_TEST_VERIFY_EQ(test_ptr, 67, close_elem_ptr->x);

    // Width sufficient only for 1 button.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 67));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 43, width);

    // Width doesn't permit any button.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 66));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 66, width);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
    wlmtk_window_destroy(w);
}

/* ------------------------------------------------------------------------- */
/** Tests titlebar with configured properties. */
void test_properties(bs_test_t *test_ptr)
{
    wlmtk_window_t *w = wlmtk_test_window_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    struct wlmtk_titlebar_style style = { .height = 22, .margin = { .width = 2 } };
    wlmtk_titlebar_t *titlebar_ptr = wlmtk_titlebar_create(w, &style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, titlebar_ptr);

    // Short names, for improved readability.
    wlmtk_element_t *title_elem_ptr = wlmtk_titlebar_title_element(
        titlebar_ptr->titlebar_title_ptr);
    wlmtk_element_t *minimize_elem_ptr = wlmtk_titlebar_button_element(
        titlebar_ptr->minimize_button_ptr);
    wlmtk_element_t *close_elem_ptr = wlmtk_titlebar_button_element(
        titlebar_ptr->close_button_ptr);
    int width;

    // Width sufficient for all: All elements visible and placed.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_titlebar_set_width(titlebar_ptr, 89));
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 24, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 41, width);
    BS_TEST_VERIFY_EQ(test_ptr, 67, close_elem_ptr->x);

    // Properties disabling the close button.
    wlmtk_titlebar_set_properties(
        titlebar_ptr, WLMTK_TITLEBAR_PROPERTY_ICONIFY);
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 24, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 65, width);

    // Properties disabling the iconify button.
    wlmtk_titlebar_set_properties(
        titlebar_ptr, WLMTK_TITLEBAR_PROPERTY_CLOSE);
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 65, width);
    BS_TEST_VERIFY_EQ(test_ptr, 67, close_elem_ptr->x);

    // Disable all of them.
    wlmtk_titlebar_set_properties(titlebar_ptr, 0);
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_FALSE(test_ptr, close_elem_ptr->visible);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 89, width);

    // Re-enable all of them.
    wlmtk_titlebar_set_properties(
        titlebar_ptr, _wlmtk_titlebar_default_properties);
    BS_TEST_VERIFY_TRUE(test_ptr, title_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, minimize_elem_ptr->visible);
    BS_TEST_VERIFY_TRUE(test_ptr, close_elem_ptr->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 24, title_elem_ptr->x);
    wlmtk_element_get_dimensions(title_elem_ptr, NULL, NULL, &width, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 41, width);
    BS_TEST_VERIFY_EQ(test_ptr, 67, close_elem_ptr->x);

    wlmtk_element_destroy(wlmtk_titlebar_element(titlebar_ptr));
    wlmtk_window_destroy(w);
}

/* == End of titlebar.c ==================================================== */
