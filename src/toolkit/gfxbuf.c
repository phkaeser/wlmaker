/* ========================================================================= */
/**
 * @file gfxbuf.c
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

#include "gfxbuf.h"

#include <drm_fourcc.h>

#define WLR_USE_UNSTABLE
#include <wlr/interfaces/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the wrapped graphics buffer. */
typedef struct {
    /** The wlroots buffer. */
    struct wlr_buffer         wlr_buffer;

    /** The actual graphics buffer. */
    bs_gfxbuf_t               *gfxbuf_ptr;
} wlmaker_gfxbuf_t;

static wlmaker_gfxbuf_t *wlmaker_gfxbuf_from_wlr_buffer(
    struct wlr_buffer *wlr_buffer_ptr);

static void wlmaker_gfxbuf_impl_destroy(
    struct wlr_buffer *wlr_buffer_ptr);
static bool wlmaker_gfxbuf_impl_begin_data_ptr_access(
    struct wlr_buffer *wlr_buffer_ptr,
    uint32_t flags,
    void **data_ptr_ptr,
    uint32_t *format_ptr,
    size_t *stride_ptr);
static void wlmaker_gfxbuf_impl_end_data_ptr_access(
    struct wlr_buffer *wlr_buffer_ptr);



/** Implementation callbacks for wlroots' `struct wlr_buffer`. */
static const struct wlr_buffer_impl wlmaker_gfxbuf_impl = {
    .destroy = wlmaker_gfxbuf_impl_destroy,
    .begin_data_ptr_access = wlmaker_gfxbuf_impl_begin_data_ptr_access,
    .end_data_ptr_access = wlmaker_gfxbuf_impl_end_data_ptr_access
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlr_buffer *bs_gfxbuf_create_wlr_buffer(
    unsigned width,
    unsigned height)
{
    wlmaker_gfxbuf_t *gfxbuf_ptr = logged_calloc(1, sizeof(wlmaker_gfxbuf_t));
    if (NULL == gfxbuf_ptr) return NULL;

    wlr_buffer_init(
        &gfxbuf_ptr->wlr_buffer,
        &wlmaker_gfxbuf_impl,
        width,
        height);

    gfxbuf_ptr->gfxbuf_ptr = bs_gfxbuf_create(width, height);
    if (NULL == gfxbuf_ptr->gfxbuf_ptr) {
        wlmaker_gfxbuf_impl_destroy(&gfxbuf_ptr->wlr_buffer);
        return NULL;
    }

    return &gfxbuf_ptr->wlr_buffer;
}

/* ------------------------------------------------------------------------- */
bs_gfxbuf_t *bs_gfxbuf_from_wlr_buffer(
    struct wlr_buffer *wlr_buffer_ptr)
{
    wlmaker_gfxbuf_t *gfxbuf_ptr = wlmaker_gfxbuf_from_wlr_buffer(
        wlr_buffer_ptr);
    return gfxbuf_ptr->gfxbuf_ptr;
}

/* ------------------------------------------------------------------------- */
cairo_t *cairo_create_from_wlr_buffer(struct wlr_buffer *wlr_buffer_ptr)
{
    return cairo_create_from_bs_gfxbuf(
        bs_gfxbuf_from_wlr_buffer(wlr_buffer_ptr));
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the @ref wlmaker_gfxbuf_t for `wlr_buffer_ptr`.
 *
 * @param wlr_buffer_ptr      Pointer to a `struct wlr_buffer`. Must be a
 *                            `wlr_buffer` that was previously created by
 *                            @ref bs_gfxbuf_create_wlr_buffer.
 *
 * @return A pointer to the @ref wlmaker_gfxbuf_t.
 */
wlmaker_gfxbuf_t *wlmaker_gfxbuf_from_wlr_buffer(
    struct wlr_buffer *wlr_buffer_ptr)
{
    // Verifies this is indeed a graphics-buffer-backed WLR buffer. */
    BS_ASSERT(0 == memcmp(
                  wlr_buffer_ptr->impl,
                  &wlmaker_gfxbuf_impl,
                  sizeof(struct wlr_buffer_impl)));

    return BS_CONTAINER_OF(wlr_buffer_ptr, wlmaker_gfxbuf_t, wlr_buffer);
}

/* ------------------------------------------------------------------------- */
/**
 * `struct wlr_buffer_impl` callback: Destroys the graphics buffer.
 *
 * This function Will be called only once producer and all consumers of the
 * corresponding wlr_buffer have lifted their locks (references).
 *
 * @param wlr_buffer_ptr
 */
void wlmaker_gfxbuf_impl_destroy(
    struct wlr_buffer *wlr_buffer_ptr)
{
    wlmaker_gfxbuf_t *gfxbuf_ptr = wlmaker_gfxbuf_from_wlr_buffer(
        wlr_buffer_ptr);

    if (NULL != gfxbuf_ptr->gfxbuf_ptr) {
        bs_gfxbuf_destroy(gfxbuf_ptr->gfxbuf_ptr);
        gfxbuf_ptr->gfxbuf_ptr = NULL;
    }

    free(gfxbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * `struct wlr_buffer_impl` callback: Set up for data access.
 *
 * @param wlr_buffer_ptr
 * @param flags               unused.
 * @param data_ptr_ptr        Output argument, point to the surface data.
 * @param format_ptr          Output argument, set to the surface's format.
 * @param stride_ptr          Output argument, set to the surface's stride.
 */
bool wlmaker_gfxbuf_impl_begin_data_ptr_access(
    struct wlr_buffer *wlr_buffer_ptr,
    __UNUSED__ uint32_t flags,
    void **data_ptr_ptr,
    uint32_t *format_ptr,
    size_t *stride_ptr)
{
    wlmaker_gfxbuf_t *gfxbuf_ptr = wlmaker_gfxbuf_from_wlr_buffer(
        wlr_buffer_ptr);
    *data_ptr_ptr = gfxbuf_ptr->gfxbuf_ptr->data_ptr;
    *format_ptr = DRM_FORMAT_ARGB8888;  // Equivalent to CAIRO ARGB32.
    *stride_ptr =gfxbuf_ptr->gfxbuf_ptr->pixels_per_line * sizeof(uint32_t);
    return true;
}

/* ------------------------------------------------------------------------- */
/** `struct wlr_buffer_impl` callback: End data access. Here, a no-op. */
void wlmaker_gfxbuf_impl_end_data_ptr_access(
    __UNUSED__ struct wlr_buffer *wlr_buffer_ptr)
{
    // Nothing to do.
}

/* == End of gfxbuf.c ====================================================== */
