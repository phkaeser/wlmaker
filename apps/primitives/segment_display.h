/* ========================================================================= */
/**
 * @file segment_display.h
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
#ifndef __WLM_SEGMENT_DISPLAY_H__
#define __WLM_SEGMENT_DISPLAY_H__

#include <libbase/libbase.h>

#include <cairo.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Parameters to describe segment properties. */
typedef struct {
    /** Offset distance, from origin to start of segment. */
    double offset;
    /** Full width of the segment, along the lateral direction. */
    double width;
    /** Length of a horizontal segment, along longitudinal direction. */
    double hlength;
    /** Length of a vertical segment, along longitudinal direction. */
    double vlength;
} wlm_cairo_7segment_param_t;

/** Parameters for a 6x8-pixel-sized 7-segment digit display. */
extern const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_6x8;
/** Parameters for a 7x10-pixel-sized 7-segment digit display. */
extern const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_7x10;
/** Parameters for a 8x12-pixel-sized 7-segment digit display. */
extern const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_8x12;
/** Parameters for a 16x24-pixel-sized 7-segment digit display. */
extern const wlm_cairo_7segment_param_t wlm_cairo_7segment_param_16x24;

/**
 * Draws a digit using 7-segment visualization.
 *
 * @param cairo_ptr           The `cairo_t` target to draw the digits to.
 * @param param_ptr           Visualization parameters for the segments.
 * @param x                   X coordinate of lower left corner.
 * @param y                   Y coordinate of lower left corner.
 * @param color_on            An ARGB32 value for an illuminated segment.
 * @param color_off           An ARGB32 value for a non-illuminated segment.
 * @param digit               Digit to draw. Must be 0 <= digit < 10.
 */
void wlm_cairo_7segment_display_digit(
    cairo_t *cairo_ptr,
    const wlm_cairo_7segment_param_t *param_ptr,
    uint32_t x,
    uint32_t y,
    uint32_t color_on,
    uint32_t color_off,
    uint8_t digit);

/** Unit test cases. */
extern const bs_test_case_t wlm_cairo_segment_display_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLM_SEGMENT_DISPLAY_H__ */
/* == End of segment_display.h ============================================= */
