/* ========================================================================= */
/**
 * @file image.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "image.h"

#include "buffer.h"
#include "gfxbuf.h"

/* == Declarations ========================================================= */

/** State of the image. */
struct _wlmtk_image_t {
    /** The image's superclass: A buffer. */
    wlmtk_buffer_t            super_buffer;
};

struct wlr_buffer *_wlmtk_image_create_wlr_buffer_from_image(
    const char *path_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_image_t *wlmtk_image_create(
    const char *image_path_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_image_t *image_ptr = logged_calloc(1, sizeof(wlmtk_image_t));
    if (NULL == image_ptr) return NULL;

    if (!wlmtk_buffer_init(&image_ptr->super_buffer, env_ptr)) {
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }

    struct wlr_buffer *wlr_buffer_ptr =
        _wlmtk_image_create_wlr_buffer_from_image(image_path_ptr);
    if (NULL == wlr_buffer_ptr) {
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }
    wlmtk_buffer_set(&image_ptr->super_buffer, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);

#if 0
    // FIXME == Resolution should be done by caller.
    // Resolve to a full path, and verify the file exists.
    char full_path[PATH_MAX];
    char *path_ptr = bs_file_resolve_and_lookup_from_paths(
        image_path_ptr, lookup_paths_ptr, 0, full_path);
    if (NULL == path_ptr) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed bs_file_resolve_and_lookup_from_paths(%s, ...).",
               icon_path_ptr);
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }
#endif


    return image_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_image_destroy(wlmtk_image_t *image_ptr)
{
    wlmtk_buffer_fini(&image_ptr->super_buffer);
    free(image_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates a wlr_buffer that holds the image loaded from path, at that image's
 * size.
 *
 * @param path_ptr
 *
 * @return the wlr_buffer or NULL on error.
 */
struct wlr_buffer *_wlmtk_image_create_wlr_buffer_from_image(
    const char *path_ptr)
{
    cairo_surface_t *icon_surface_ptr = cairo_image_surface_create_from_png(
        path_ptr);
    if (NULL == icon_surface_ptr) {
        bs_log(BS_ERROR, "Failed cairo_image_surface_create_from_png(%s).",
               path_ptr);
        return false;
    }
    if (CAIRO_STATUS_SUCCESS != cairo_surface_status(icon_surface_ptr)) {
        bs_log(BS_ERROR,
               "Bad surface after cairo_image_surface_create_from_png(%s): %s",
               path_ptr,
               cairo_status_to_string(cairo_surface_status(icon_surface_ptr)));
        cairo_surface_destroy(icon_surface_ptr);
        return NULL;
    }

    int w = cairo_image_surface_get_width(icon_surface_ptr);
    int h = cairo_image_surface_get_height(icon_surface_ptr);

    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(w, h);
    if (NULL == wlr_buffer_ptr) {
        cairo_surface_destroy(icon_surface_ptr);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        cairo_surface_destroy(icon_surface_ptr);
        return NULL;
    }

    cairo_set_source_surface(cairo_ptr, icon_surface_ptr, 0, 0);
    cairo_rectangle(cairo_ptr, 0, 0, w, h);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);

    cairo_destroy(cairo_ptr);
    cairo_surface_destroy(icon_surface_ptr);
    return wlr_buffer_ptr;
}

/* == Unit tests =========================================================== */
static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_image_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_image_t *image_ptr = wlmtk_image_create(
        bs_test_resolve_path("toolkit/test_icon.png"), NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, image_ptr);

    wlmtk_image_destroy(image_ptr);
}

/* == End of image.c ======================================================= */
