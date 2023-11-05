/* ========================================================================= */
/**
 * @file resizebar.c
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "resizebar.h"

#include "box.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of the title bar. */
struct _wlmtk_resizebar_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Current width of the resize bar. */
    unsigned                  width;
    /** Style of the resize bar. */
    wlmtk_resizebar_style_t   style;
};

static void resizebar_box_destroy(wlmtk_box_t *box_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t resizebar_box_impl = {
    .destroy = resizebar_box_destroy
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

    resizebar_ptr->width = width;

    return resizebar_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_destroy(wlmtk_resizebar_t *resizebar_ptr)
{
    wlmtk_box_fini(&resizebar_ptr->super_box);
    free(resizebar_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_resizebar_set_width(
    wlmtk_resizebar_t * resizebar_ptr,
    unsigned width)
{
    resizebar_ptr->width = width;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_resizebar_element(wlmtk_resizebar_t *resizebar_ptr)
{
    return &resizebar_ptr->super_box.super_container.super_element;
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

/* == End of resizebar.c =================================================== */
