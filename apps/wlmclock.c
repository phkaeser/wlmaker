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
/**
 * Draws contents into the icon buffer.
 *
 * @param wlclient_ptr
 * @param gfxbuf_ptr
 * @param ud_ptr
 */
bool icon_callback(
    __UNUSED__ wlclient_t *wlclient_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    __UNUSED__ void *ud_ptr)
{
    static uint32_t pos = 0;

    bs_log(BS_INFO, "Icon callback.");

    for (unsigned y = 0; y < gfxbuf_ptr->height; ++y) {
        for (unsigned x = 0; x < gfxbuf_ptr->width; ++x) {
            if ((x + pos) & 0x10) {
                bs_gfxbuf_set_pixel(gfxbuf_ptr, x, y, 0xffc08080);
            } else {
                bs_gfxbuf_set_pixel(gfxbuf_ptr, x, y, 0xff2040c0);
            }
        }
    }
    pos++;
    return true;
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

            // Alternative: timer, on the client itself.

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
