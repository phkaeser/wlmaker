/* ========================================================================= */
/**
 * @file image.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "image.h"

#include <cairo.h>
#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>

#include "buffer.h"
#include "gfxbuf.h"  // IWYU pragma: keep

/* == Declarations ========================================================= */

/** State of the image. */
struct _wlmtk_image_t {
    /** The image's superclass: A buffer. */
    wlmtk_buffer_t            super_buffer;
    /** The superclass' virtual method table. */
    wlmtk_element_vmt_t       orig_element_vmt;
};

struct wlr_buffer *_wlmtk_image_create_wlr_buffer_from_image(
    const char *path_ptr,
    int width,
    int height);

static void _wlmtk_image_element_destroy(wlmtk_element_t *element_ptr);

/* == Data ================================================================= */

/** The imag'es virtual method table for @ref wlmtk_element_t superclass. */
static const wlmtk_element_vmt_t _wlmtk_image_element_vmt = {
    .destroy = _wlmtk_image_element_destroy,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_image_t *wlmtk_image_create(const char *image_path_ptr)
{
    return wlmtk_image_create_scaled(image_path_ptr, 0, 0);
}

/* ------------------------------------------------------------------------- */
wlmtk_image_t *wlmtk_image_create_scaled(
    const char *image_path_ptr,
    int width,
    int height)
{
    wlmtk_image_t *image_ptr = logged_calloc(1, sizeof(wlmtk_image_t));
    if (NULL == image_ptr) return NULL;

    if (!wlmtk_buffer_init(&image_ptr->super_buffer)) {
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }
    image_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_image_element(image_ptr),
        &_wlmtk_image_element_vmt);

    struct wlr_buffer *wlr_buffer_ptr =
        _wlmtk_image_create_wlr_buffer_from_image(
            image_path_ptr, width, height);
    if (NULL == wlr_buffer_ptr) {
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }
    wlmtk_buffer_set(&image_ptr->super_buffer, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);

    return image_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_image_destroy(wlmtk_image_t *image_ptr)
{
    wlmtk_buffer_fini(&image_ptr->super_buffer);
    free(image_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_image_element(wlmtk_image_t *image_ptr)
{
    return wlmtk_buffer_element(&image_ptr->super_buffer);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates a wlr_buffer that holds the image loaded from path, at that image's
 * size.
 *
 * @param path_ptr
 * @param width               Desired width of the image. 0 0r negative to use
 *                            the image's native width.
 * @param height              Desired height of the image. 0 0r negative to use
 *                            the image's native height.
 *
 * @return the wlr_buffer or NULL on error.
 */
struct wlr_buffer *_wlmtk_image_create_wlr_buffer_from_image(
    const char *path_ptr,
    int width,
    int height)
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

    int w = width;
    if (0 >= w) {
        w = cairo_image_surface_get_width(icon_surface_ptr);
    }
    int h = height;
    if (0 >= h) {
        h = cairo_image_surface_get_height(icon_surface_ptr);
    }

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

    cairo_surface_set_device_scale(
        icon_surface_ptr,
        (double)cairo_image_surface_get_width(icon_surface_ptr) / w,
        (double)cairo_image_surface_get_height(icon_surface_ptr) / h);

    cairo_set_source_surface(cairo_ptr, icon_surface_ptr, 0, 0);
    cairo_rectangle(cairo_ptr, 0, 0, w, h);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);

    cairo_destroy(cairo_ptr);
    cairo_surface_destroy(icon_surface_ptr);
    return wlr_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::destroy -- virtual dtor. */
void _wlmtk_image_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_image_t *image_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_image_t, super_buffer.super_element);
    wlmtk_image_destroy(image_ptr);
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
        bs_test_resolve_path("toolkit/test_icon.png"));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, image_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(image_ptr->super_buffer.wlr_buffer_ptr),
        "toolkit/test_icon.png");

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &image_ptr->super_buffer.super_element,
        wlmtk_image_element(image_ptr));

    wlmtk_image_destroy(image_ptr);
}

/* == End of image.c ======================================================= */
