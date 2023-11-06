/* ========================================================================= */
/**
 * @file resizebar.c
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "resizebar.h"

#include "box.h"
#include "buffer.h"
#include "gfxbuf.h"
#include "primitives.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Forward declaration: Element of the resizebar. */
typedef struct _wlmtk_resizebar_element_t wlmtk_resizebar_element_t ;

/** State of the title bar. */
struct _wlmtk_resizebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Current width of the resize bar. */
    unsigned                  width;
    /** Style of the resize bar. */
    wlmtk_resizebar_style_t   style;

    /** Background. */
    bs_gfxbuf_t               *gfxbuf_ptr;

    /** Element of the resizebar. */
    wlmtk_resizebar_element_t *element_ptr;
};

/** State of an element of the resize bar. */
struct _wlmtk_resizebar_element_t {
    /** Superclass: Buffer. */
    wlmtk_buffer_t            super_buffer;
    /** The buffer. */
    struct wlr_buffer         *wlr_buffer_ptr;
};

static wlmtk_resizebar_element_t *wlmtk_resizebar_element_create(
    bs_gfxbuf_t *gfxbuf_ptr,
    int width,
    const wlmtk_resizebar_style_t *style_ptr);
static void wlmtk_resizebar_element_destroy(
    wlmtk_resizebar_element_t *element_ptr);
static bool wlmtk_resizebar_element_redraw(
    wlmtk_resizebar_element_t *element_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr);

static void resizebar_box_destroy(wlmtk_box_t *box_ptr);
static bool redraw_buffers(wlmtk_resizebar_t *resizebar_ptr, unsigned width);

static void element_buffer_destroy(wlmtk_buffer_t *buffer_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t resizebar_box_impl = {
    .destroy = resizebar_box_destroy
};

/** Buffer implementation for title of the title bar. */
static const wlmtk_buffer_impl_t element_buffer_impl = {
    .destroy = element_buffer_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_resizebar_t *wlmtk_resizebar_create(
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr)
{
    wlmtk_resizebar_t *resizebar_ptr = logged_calloc(
        1, sizeof(wlmtk_resizebar_t));
    if (NULL == resizebar_ptr) return NULL;
    memcpy(&resizebar_ptr->style, style_ptr, sizeof(wlmtk_resizebar_style_t));

    if (!wlmtk_box_init(&resizebar_ptr->super_box,
                        &resizebar_box_impl,
                        WLMTK_BOX_HORIZONTAL)) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }

    if (!redraw_buffers(resizebar_ptr, width)) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }

    resizebar_ptr->element_ptr = wlmtk_resizebar_element_create(
        resizebar_ptr->gfxbuf_ptr, width, &resizebar_ptr->style);
    if (NULL == resizebar_ptr->element_ptr) {
        wlmtk_resizebar_destroy(resizebar_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &resizebar_ptr->element_ptr->super_buffer.super_element, true);
    wlmtk_container_add_element(
        &resizebar_ptr->super_box.super_container,
        &resizebar_ptr->element_ptr->super_buffer.super_element);

    return resizebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_destroy(wlmtk_resizebar_t *resizebar_ptr)
{
    if (NULL != resizebar_ptr->element_ptr) {
        wlmtk_container_remove_element(
            &resizebar_ptr->super_box.super_container,
            &resizebar_ptr->element_ptr->super_buffer.super_element);
        wlmtk_resizebar_element_destroy(resizebar_ptr->element_ptr);
        resizebar_ptr->element_ptr = NULL;
    }

    if (NULL != resizebar_ptr->gfxbuf_ptr) {
        bs_gfxbuf_destroy(resizebar_ptr->gfxbuf_ptr);
        resizebar_ptr->gfxbuf_ptr = NULL;
    }

    wlmtk_box_fini(&resizebar_ptr->super_box);
    free(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_resizebar_set_width(
    wlmtk_resizebar_t * resizebar_ptr,
    unsigned width)
{
    if (resizebar_ptr->width == width) return true;
    if (!redraw_buffers(resizebar_ptr, width)) return false;
    BS_ASSERT(width == resizebar_ptr->width);

    if (!wlmtk_resizebar_element_redraw(
            resizebar_ptr->element_ptr,
            resizebar_ptr->gfxbuf_ptr,
            0, width,
            &resizebar_ptr->style)) {
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_resizebar_element(wlmtk_resizebar_t *resizebar_ptr)
{
    return &resizebar_ptr->super_box.super_container.super_element;
}

/* == Resizebar element methods ============================================ */

/* ------------------------------------------------------------------------- */
/**
 * Creates a resizebar element.
 *
 * @param gfxbuf_ptr
 * @param width
 * @param style_ptr
 *
 * @return Pointer to the element.
 */
wlmtk_resizebar_element_t *wlmtk_resizebar_element_create(
    bs_gfxbuf_t *gfxbuf_ptr,
    int width,
    const wlmtk_resizebar_style_t *style_ptr)
{
    wlmtk_resizebar_element_t *element_ptr = logged_calloc(
        1, sizeof(wlmtk_resizebar_element_t));
    if (NULL == element_ptr) return NULL;

    if (!wlmtk_resizebar_element_redraw(
            element_ptr,
            gfxbuf_ptr,
            0,
            width,
            style_ptr)) {
        wlmtk_resizebar_element_destroy(element_ptr);
        return NULL;
   }

    if (!wlmtk_buffer_init(
            &element_ptr->super_buffer,
            &element_buffer_impl,
            element_ptr->wlr_buffer_ptr)) {
        wlmtk_resizebar_element_destroy(element_ptr);
        return NULL;
    }

    return element_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the resizebar element.
 *
 * @param element_ptr
 */
void wlmtk_resizebar_element_destroy(
    wlmtk_resizebar_element_t *element_ptr)
{
    if (NULL != element_ptr->wlr_buffer_ptr) {
        wlr_buffer_drop(element_ptr->wlr_buffer_ptr);
        element_ptr->wlr_buffer_ptr = NULL;
    }

    wlmtk_buffer_fini(&element_ptr->super_buffer);
    free(element_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Redraws the element, with updated position and width.
 *
 * @param element_ptr
 * @param gfxbuf_ptr
 * @param position
 * @param width
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_resizebar_element_redraw(
    wlmtk_resizebar_element_t *element_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        width, style_ptr->height);
    if (NULL == wlr_buffer_ptr) return false;

    bs_gfxbuf_copy_area(
        bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr),
        0, 0,
        gfxbuf_ptr,
        position, 0,
        width, style_ptr->height);

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return false;
    }
    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, 0, 0, width, style_ptr->height, 1.0, true);
    cairo_destroy(cairo_ptr);

    if (NULL != element_ptr->wlr_buffer_ptr) {
        wlr_buffer_drop(element_ptr->wlr_buffer_ptr);
    }
    element_ptr->wlr_buffer_ptr = wlr_buffer_ptr;
    wlmtk_buffer_set(&element_ptr->super_buffer, element_ptr->wlr_buffer_ptr);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from box. Wraps to our dtor. */
void resizebar_box_destroy(wlmtk_box_t *box_ptr)
{
    wlmtk_resizebar_t *resizebar_ptr = BS_CONTAINER_OF(
        box_ptr, wlmtk_resizebar_t, super_box);
    wlmtk_resizebar_destroy(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
/** Redraws the resizebar's background in appropriate size. */
bool redraw_buffers(wlmtk_resizebar_t *resizebar_ptr, unsigned width)
{
    cairo_t *cairo_ptr;

    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(
        width, resizebar_ptr->style.height);
    if (NULL == gfxbuf_ptr) return false;
    cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_gfxbuf_destroy(gfxbuf_ptr);
        return false;
    }
    wlmaker_primitives_cairo_fill(cairo_ptr, &resizebar_ptr->style.fill);
    cairo_destroy(cairo_ptr);

    if (NULL != resizebar_ptr->gfxbuf_ptr) {
        bs_gfxbuf_destroy(resizebar_ptr->gfxbuf_ptr);
    }
    resizebar_ptr->gfxbuf_ptr = gfxbuf_ptr;
    resizebar_ptr->width = width;
    return true;
}

/* ------------------------------------------------------------------------- */
/** Dtor. Forwards to @ref wlmtk_resizebar_element_destroy. */
void element_buffer_destroy(wlmtk_buffer_t *buffer_ptr)
{
    wlmtk_resizebar_element_t *element_ptr = BS_CONTAINER_OF(
        buffer_ptr, wlmtk_resizebar_element_t, super_buffer);
    wlmtk_resizebar_element_destroy(element_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_resizebar_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises @ref wlmtk_resizebar_create and @ref wlmtk_resizebar_destroy. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_resizebar_style_t style = {};
    wlmtk_resizebar_t *resizebar_ptr = wlmtk_resizebar_create(120, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, resizebar_ptr);

    wlmtk_element_destroy(wlmtk_resizebar_element(resizebar_ptr));
}

/* == End of resizebar.c =================================================== */
