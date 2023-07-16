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

    cairo_select_font_face(
        cairo_ptr, "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 15.0);
    cairo_set_source_argb8888(cairo_ptr, 0xffc0c0c0);

    cairo_move_to(cairo_ptr, 8, 32);
    cairo_show_text(cairo_ptr, "time");

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
