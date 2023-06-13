/* ========================================================================= */
/**
 * @file gfxbuf.h
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
#ifndef __GFXBUF_H__
#define __GFXBUF_H__

#include <libbase/libbase.h>
#include <cairo.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_buffer.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a wlroots buffer tied to a libbase graphics buffer.
 *
 * This creates a libbase graphics buffer, and wraps it as `struct wlr_buffer`.
 *
 * @param width
 * @param height
 *
 * @return A struct wlr_buffer. Must be released using wlr_buffer_drop().
 */
struct wlr_buffer *bs_gfxbuf_create_wlr_buffer(
    unsigned width,
    unsigned height);

/**
 * Returns the libbase graphics buffer for the `struct wlr_buffer`.
 *
 * @param wlr_buffer_ptr      Pointer to a `struct wlr_buffer`. Must be a
 *                            `wlr_buffer` that was previously created by
 *                            @ref bs_gfxbuf_create_wlr_buffer.
 *
 * @return Pointer to the libbase graphics buffer.
 */
bs_gfxbuf_t *bs_gfxbuf_from_wlr_buffer(struct wlr_buffer *wlr_buffer_ptr);

/**
 * Returns a `cairo_t` for the WLR buffer backged by a libbase graphics buffer.
 *
 * @param wlr_buffer_ptr      Pointer to a `struct wlr_buffer`. Must be a
 *                            `wlr_buffer` that was previously created by
 *                            @ref bs_gfxbuf_create_wlr_buffer.
 *
 * @return Pointer to the the cairo. The `wlr_buffer_ptr` must outlive the
 *     `cairo_t`. It must be destroyed using `cairo_destroy`.
 */
cairo_t *cairo_create_from_wlr_buffer(struct wlr_buffer *wlr_buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __GFXBUF_H__ */
/* == End of gfxbuf.h ====================================================== */
