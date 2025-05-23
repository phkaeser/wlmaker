/* ========================================================================= */
/**
 * @file style.c
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

#include <libbase/libbase.h>
#include <toolkit/style.h>

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
cairo_font_weight_t wlmtk_style_font_weight_cairo_from_wlmtk(
    wlmtk_style_font_weight_t weight)
{
    switch (weight) {
    case WLMTK_FONT_WEIGHT_NORMAL: return CAIRO_FONT_WEIGHT_NORMAL;
    case WLMTK_FONT_WEIGHT_BOLD: return CAIRO_FONT_WEIGHT_BOLD;
    default:
        bs_log(BS_FATAL, "Unhandled font weight %d", weight);
        BS_ABORT();
    }
    return CAIRO_FONT_WEIGHT_NORMAL;
}

/* == End of style.c ======================================================= */
