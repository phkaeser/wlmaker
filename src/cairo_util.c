/* ========================================================================= */
/**
 * @file cairo_util.c
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

#include "cairo_util.h"

#include <libbase/libbase.h>
#include <math.h>

/* == Declarations ========================================================= */

/** Image format we want to use throughout. */
static const cairo_format_t   wlmaker_cairo_image_format = CAIRO_FORMAT_ARGB32;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
cairo_t *wlmaker_cairo_util_create_with_surface(
    uint32_t width,
    uint32_t height)
{
    cairo_surface_t *cairo_surface_ptr = cairo_image_surface_create(
        wlmaker_cairo_image_format, width, height);
    if (NULL == cairo_surface_ptr) {
        bs_log(BS_ERROR,
               "Failed cairo_image_surface_create(%d, %"PRIu32", %"PRIu32")",
               wlmaker_cairo_image_format, width, height);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create(cairo_surface_ptr);
    // cairo_create() increases the reference count on cairo_surface_ptr. Means
    // we can destroy (decrease reference) here, no matter the outcome.
    cairo_surface_destroy(cairo_surface_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create(%p)", cairo_surface_ptr);
        return NULL;
    }
    return cairo_ptr;
}

/* == End of cairo_util.c ================================================== */
