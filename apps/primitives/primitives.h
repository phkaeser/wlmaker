/* ========================================================================= */
/**
 * @file primitives.h
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
#ifndef __WLM_PRIMITIVES_H__
#define __WLM_PRIMITIVES_H__

#include <cairo.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Sets the bezel color.
 *
 * Note: Window Maker draws the bezel by adding 80 (0x50) to each R, G, B of
 * the underlying title for the illuminated side; respectively by subtracting
 * 40 (0x28) on the non-illuminated side.
 * We are using cairo's overlaying with the respective "alpha" values below,
 * which leads to different results.
 *
 * @param cairo_ptr
 * @param illuminated
 */
void wlm_primitives_set_bezel_color(
    cairo_t *cairo_ptr,
    bool illuminated);

/**
 * Draws a bezel into the cairo, at specified position and width/height.
 *
 * TODO(kaeser@gubbe.ch): Share this code with the server.
 *
 * @param cairo_ptr           A cairo, backed by an image surface.
 * @param x
 * @param y
 * @param width
 * @param height
 * @param bezel_width
 * @param raised              Whether the bezel is to highlight a raised (true)
 *                            or pressed (false) state.
 */
void wlm_primitives_draw_bezel_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    double bezel_width,
    bool raised);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLM_PRIMITIVES_H__ */
/* == End of primitives.h ================================================== */
