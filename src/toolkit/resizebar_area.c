/* ========================================================================= */
/**
 * @file resizebar_area.c
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "resizebar_area.h"

#include "box.h"
#include "buffer.h"
#include "button.h"
#include "gfxbuf.h"
#include "primitives.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of an element of the resize bar. */
struct _wlmtk_resizebar_button_t {
    /** Superclass: Buffer. */
    wlmtk_button_t            super_button;
};

static void button_destroy(wlmtk_button_t *button_ptr);
static struct wlr_buffer *create_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr,
    bool pressed);

/* == Data ================================================================= */

/** Buffer implementation for title of the title bar. */
static const wlmtk_button_impl_t element_button_impl = {
    .destroy = button_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_resizebar_button_t *wlmtk_resizebar_button_create()
{
    wlmtk_resizebar_button_t *resizebar_button = logged_calloc(
        1, sizeof(wlmtk_resizebar_button_t));
    if (NULL == resizebar_button) return NULL;

    if (!wlmtk_button_init(
            &resizebar_button->super_button,
            &element_button_impl)) {
        wlmtk_resizebar_button_destroy(resizebar_button);
        return NULL;
    }

    return resizebar_button;
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_button_destroy(
    wlmtk_resizebar_button_t *resizebar_button_ptr)
{
    wlmtk_button_fini(&resizebar_button_ptr->super_button);
    free(resizebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_resizebar_button_redraw(
    wlmtk_resizebar_button_t *resizebar_button_ptr,
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

    // Will take ownershp of the buffers.
    wlmtk_button_set(
        &resizebar_button_ptr->super_button,
        released_wlr_buffer_ptr,
        pressed_wlr_buffer_ptr);

    wlr_buffer_drop(released_wlr_buffer_ptr);
    wlr_buffer_drop(pressed_wlr_buffer_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_resizebar_button_element(
    wlmtk_resizebar_button_t *resizebar_button_ptr)
{
    return &resizebar_button_ptr->super_button.super_buffer.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Dtor. Forwards to @ref wlmtk_resizebar_button_destroy. */
void button_destroy(wlmtk_button_t *button_ptr)
{
    wlmtk_resizebar_button_t *resizebar_button_ptr = BS_CONTAINER_OF(
        button_ptr, wlmtk_resizebar_button_t, super_button);
    wlmtk_resizebar_button_destroy(resizebar_button_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a resizebar button texture.
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
