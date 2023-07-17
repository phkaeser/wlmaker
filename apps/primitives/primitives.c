/* ========================================================================= */
/**
 * @file primitives.c
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

#include "primitives.h"

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlm_primitives_set_bezel_color(
    cairo_t *cairo_ptr,
    bool illuminated)
{
    if (illuminated) {
        cairo_set_source_rgba(cairo_ptr, 1.0, 1.0, 1.0, 0.6);
    } else {
        cairo_set_source_rgba(cairo_ptr, 0.0, 0.0, 0.0, 0.4);
    }
}

/* ------------------------------------------------------------------------- */
void wlm_primitives_draw_bezel_at(
    cairo_t *cairo_ptr,
    int x,
    int y,
    unsigned width,
    unsigned height,
    double bezel_width,
    bool raised)
{
    cairo_save(cairo_ptr);
    cairo_set_line_width(cairo_ptr, 0);

    // Northwestern corner is illuminted when raised.
    wlm_primitives_set_bezel_color(cairo_ptr, raised);
    cairo_move_to(cairo_ptr, x, y);
    cairo_line_to(cairo_ptr, x + width, y + 0);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + 0, y + height);
    cairo_line_to(cairo_ptr, x + 0, y + 0);
    cairo_fill(cairo_ptr);

    // Southeastern corner is illuminated when sunken.
    wlm_primitives_set_bezel_color(cairo_ptr, !raised);
    cairo_move_to(cairo_ptr, x + width, y + height);
    cairo_line_to(cairo_ptr, x + 0, y + height);
    cairo_line_to(cairo_ptr, x + bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + height - bezel_width);
    cairo_line_to(cairo_ptr, x + width - bezel_width, y + bezel_width);
    cairo_line_to(cairo_ptr, x + width, y + 0);
    cairo_line_to(cairo_ptr, x + width, y + height);
    cairo_fill(cairo_ptr);

    cairo_restore(cairo_ptr);
}

/* == End of primitives.c ================================================== */
