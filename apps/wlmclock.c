/* ========================================================================= */
/**
 * @file wlmclock.c
 *
 * Demonstrator for using the icon protocol.
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

#include <cairo.h>
#include <libbase/libbase.h>
#include <libwlclient/libwlclient.h>
#include <math.h>
#include <primitives/primitives.h>
#include <primitives/segment_display.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "libwlclient/icon.h"

/** Foreground color of a LED in the VFD-style display. */
static const uint32_t color_led = 0xff55ffff;
/** Color of a turned-off element in the VFD-style display. */
static const uint32_t color_off = 0xff114444;
/** Background color in the VFD-style display. */
static const uint32_t color_background = 0xff111111;

/* ------------------------------------------------------------------------- */
/** Returns the next full second for when to draw the clock. */
uint64_t next_draw_time(void)
{
    return (bs_usec() / 1000000 + 1) * 1000000;
}

/* ------------------------------------------------------------------------- */
/**
 * Draws contents into the icon buffer.
 *
 * @param gfxbuf_ptr
 * @param ud_ptr
 */
bool icon_callback(
    bs_gfxbuf_t *gfxbuf_ptr,
    __UNUSED__ void *ud_ptr)
{
    bs_gfxbuf_clear(gfxbuf_ptr, 0);

    if (gfxbuf_ptr->width != gfxbuf_ptr->height) {
        bs_log(BS_ERROR, "Requiring a square buffer, width %u != height %u",
               gfxbuf_ptr->width, gfxbuf_ptr->height);
        return false;
    }

    unsigned width = gfxbuf_ptr->width;
    unsigned outer = 4 * gfxbuf_ptr->width / 64;
    unsigned inner = 5 * gfxbuf_ptr->width / 64;

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create_from_bs_gfxbuf(%p)", gfxbuf_ptr);
        return false;
    }

    float r, g, b, alpha;
    bs_gfxbuf_argb8888_to_floats(color_background, &r, &g, &b, &alpha);
    cairo_pattern_t *pattern_ptr = cairo_pattern_create_rgba(r, g, b, alpha);
    BS_ASSERT(NULL != pattern_ptr);
    cairo_set_source(cairo_ptr, pattern_ptr);
    cairo_pattern_destroy(pattern_ptr);
    cairo_rectangle(
        cairo_ptr,
        outer + 1, width - 18, width - 2 * outer - 2, 14);
    cairo_fill(cairo_ptr);

    wlm_primitives_draw_bezel_at(
        cairo_ptr,
        outer, width - 19, width - 2 * outer, 15, 1.0, false);

    struct timeval tv;
    if (0 != gettimeofday(&tv, NULL)) {
        memset(&tv, 0, sizeof(tv));
    }
    struct tm *tm_ptr = localtime(&tv.tv_sec);
    char time_buf[7];
    snprintf(time_buf, sizeof(time_buf), "%02d%02d%02d",
             tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec);

    for (int i = 0; i < 6; ++i) {
        wlm_cairo_7segment_display_digit(
            cairo_ptr,
            &wlm_cairo_7segment_param_8x12,
            width / 2 - 26 + i * 8 + (i / 2) * 2,
            width - 6,
            color_led,
            color_off,
            time_buf[i] - '0');
    }

    cairo_set_source_argb8888(cairo_ptr, color_led);
    cairo_rectangle(cairo_ptr, width / 2 - 10, width - 14, 1, 1.25);
    cairo_rectangle(cairo_ptr, width / 2 - 10, width - 10, 1, 1.25);
    cairo_rectangle(cairo_ptr, width / 2 + 8, width - 14, 1, 1.25);
    cairo_rectangle(cairo_ptr, width / 2 + 8, width - 10, 1, 1.25);
    cairo_fill(cairo_ptr);


    // Draws a clock face, with small ticks every hour.
    double center_x = 31.5 * width / 64.0;
    double center_y = 24.5 * width / 64.0;
    double radius = 19 * width / 64.0;

    wlm_primitives_draw_bezel_at(
        cairo_ptr,
        outer, outer,
        width - 2 * outer, 41.0 * width / 64.0,
        inner - outer, false);
    cairo_set_source_argb8888(cairo_ptr, color_background);
    cairo_rectangle(
        cairo_ptr,
        inner, inner,
        width - 2 * inner, 39.0 * width / 64.0);
    cairo_fill(cairo_ptr);

    cairo_set_source_argb8888(cairo_ptr, color_led);
    for (int i = 0; i < 12; ++i) {
        // ... and larer ticks every 3 hours.
        double ratio = 0.9;
        if (i % 3 == 0) {
            ratio = 0.85;
            cairo_set_line_width(cairo_ptr, 2.0);
        } else {
            cairo_set_line_width(cairo_ptr, 1.0);
        }

        cairo_move_to(cairo_ptr,
                      center_x + ratio * radius * sin(i * 2*M_PI / 12.0),
                      center_y - ratio * radius * cos(i * 2*M_PI / 12.0));
        cairo_line_to(cairo_ptr,
                      center_x + radius * sin(i * 2*M_PI / 12.0),
                      center_y - radius * cos(i * 2*M_PI / 12.0));
        cairo_stroke(cairo_ptr);
    }

    // Seconds pointer.
    double angle = tm_ptr->tm_sec * 2*M_PI / 60.0;
    cairo_set_line_width(cairo_ptr, 0.5);
    cairo_move_to(cairo_ptr, center_x, center_y);
    cairo_line_to(cairo_ptr,
                  center_x + 0.7 * radius * sin(angle),
                  center_y - 0.7 * radius * cos(angle));
    cairo_stroke(cairo_ptr);

    // Minutes pointer.
    angle = tm_ptr->tm_min * 2*M_PI / 60.0 + (
        tm_ptr->tm_sec / 60.0 * 2*M_PI / 60.0);
    cairo_set_line_width(cairo_ptr, 1.0);
    cairo_move_to(cairo_ptr, center_x, center_y);
    cairo_line_to(cairo_ptr,
                  center_x + 0.7 * radius * sin(angle),
                  center_y - 0.7 * radius * cos(angle));
    cairo_stroke(cairo_ptr);

    // Hours pointer.
    angle = tm_ptr->tm_hour * 2*M_PI / 12.0 + (
        tm_ptr->tm_min / 60.0 * 2*M_PI / 12.0);
    cairo_set_line_width(cairo_ptr, 2.0);
    cairo_move_to(cairo_ptr, center_x, center_y);
    cairo_line_to(cairo_ptr,
                  center_x + 0.5 * radius * sin(angle),
                  center_y - 0.5 * radius * cos(angle));
    cairo_stroke(cairo_ptr);

    cairo_destroy(cairo_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Called once per second. */
void timer_callback(wlclient_t *client_ptr, void *ud_ptr)
{
    wlclient_icon_t *icon_ptr = ud_ptr;

    wlclient_icon_register_ready_callback(icon_ptr, icon_callback, NULL);
    wlclient_register_timer(
        client_ptr, next_draw_time(), timer_callback, icon_ptr);
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_DEBUG;

    wlclient_t *wlclient_ptr = wlclient_create("wlmaker.wlmeyes");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    if (wlclient_icon_supported(wlclient_ptr)) {
        wlclient_icon_t *icon_ptr = wlclient_icon_create(wlclient_ptr);
        if (NULL == icon_ptr) {
            bs_log(BS_ERROR, "Failed wlclient_icon_create(%p)", wlclient_ptr);
        } else {
            wlclient_icon_register_ready_callback(
                icon_ptr, icon_callback, NULL);

            wlclient_register_timer(
                wlclient_ptr, next_draw_time(), timer_callback, icon_ptr);

            wlclient_run(wlclient_ptr);
            wlclient_icon_destroy(icon_ptr);
        }
    } else {
        bs_log(BS_ERROR, "icon protocol is not supported.");
    }

    wlclient_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of wlmclock.c ==================================================== */
