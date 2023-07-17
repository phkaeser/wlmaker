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

#include <libbase/libbase.h>
#include <libwlclient/libwlclient.h>
#include <primitives/primitives.h>
#include <primitives/segment_display.h>

#include <sys/time.h>

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
 * @param icon_ptr
 * @param gfxbuf_ptr
 * @param ud_ptr
 */
bool icon_callback(
    __UNUSED__ wlclient_icon_t *icon_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    __UNUSED__ void *ud_ptr)
{
    bs_gfxbuf_clear(gfxbuf_ptr, 0);

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
    cairo_rectangle(cairo_ptr, 5, 46, 54, 14);
    cairo_fill(cairo_ptr);

    wlm_primitives_draw_bezel_at(cairo_ptr, 4, 45, 56, 15, 1.0, false);

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
            6 + i * 8 + (i / 2) * 2, 58,
            color_led,
            color_off,
            time_buf[i] - '0');
    }

    cairo_set_source_argb8888(cairo_ptr, color_led);
    cairo_rectangle(cairo_ptr, 22, 50, 1, 1.25);
    cairo_rectangle(cairo_ptr, 22, 54, 1, 1.25);
    cairo_rectangle(cairo_ptr, 40, 50, 1, 1.25);
    cairo_rectangle(cairo_ptr, 40, 54, 1, 1.25);
    cairo_fill(cairo_ptr);

    cairo_destroy(cairo_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Called once per second. */
void timer_callback(wlclient_t *client_ptr, void *ud_ptr)
{
    wlclient_icon_t *icon_ptr = ud_ptr;

    wlclient_icon_callback_when_ready(icon_ptr, icon_callback, NULL);
    wlclient_register_timer(
        client_ptr, next_draw_time(), timer_callback, icon_ptr);
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_DEBUG;

    wlclient_t *wlclient_ptr = wlclient_create("wlmclock");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    if (wlclient_icon_supported(wlclient_ptr)) {
        wlclient_icon_t *icon_ptr = wlclient_icon_create(wlclient_ptr);
        if (NULL == icon_ptr) {
            bs_log(BS_ERROR, "Failed wlclient_icon_create(%p)", wlclient_ptr);
        } else {
            wlclient_icon_callback_when_ready(icon_ptr, icon_callback, NULL);

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
