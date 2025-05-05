/* ========================================================================= */
/**
 * @file wlmclock.c
 *
 * Example app for libwlclient.
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
#include <stdlib.h>

#include "libwlclient/xdg_toplevel.h"
#include "libwlclient/libwlclient.h"

/** State of the client. */
wlclient_t                    *wlclient_ptr;
/** Listener for key events. */
static struct wl_listener     _key_listener;
/** A colorful background. */
static bs_gfxbuf_t            *background_colors;

/* ------------------------------------------------------------------------- */
/** Handles key events. */
static void _handle_key(__UNUSED__ struct wl_listener *listener_ptr,
                        void *data_ptr)
{
    wlclient_key_event_t *event_ptr = data_ptr;

    if (!event_ptr->pressed) return;
    char name[128];
    if (0 <= xkb_keysym_get_name(event_ptr->keysym, name, sizeof(name))) {
        bs_log(BS_INFO, "Key press received: %s", name);
    }

    if (XKB_KEY_Escape == event_ptr->keysym ||
        XKB_KEY_q == event_ptr->keysym ||
        XKB_KEY_Q == event_ptr->keysym) {
        wlclient_request_terminate(wlclient_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Draws something into the buffer. */
static bool _callback(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = ud_ptr;
    bs_log(BS_DEBUG, "Callback gfxbuf %p", gfxbuf_ptr);

    bs_gfxbuf_copy(gfxbuf_ptr, background_colors);

    wlclient_xdg_toplevel_register_ready_callback(
        toplevel_ptr, _callback, toplevel_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Creates a colorful background. */
bs_gfxbuf_t *_create_background(unsigned w, unsigned h)
{
    bs_gfxbuf_t *b = bs_gfxbuf_create(w, h);
    if (NULL == b) return NULL;

    uint32_t t = 0xc0000000;  // Transparency.
    for (unsigned y = 0; y < h / 2; ++y) {
        for (unsigned x = 0; x < w / 2; ++x) {
            unsigned rel_y = y * 2 * 256 / h;
            unsigned rel_x = x * 2 * 256 / w;
            // Upper left: red (horizontal), green (vertical).
            b->data_ptr[y * b->pixels_per_line + x] =
                (rel_x << 16) + (rel_y << 8) + t;
            // Upper right: green (horizontal), blue (vertical).
            b->data_ptr[y * b->pixels_per_line + x + w / 2] =
                (rel_x << 8) + rel_y + t;
            // Bottom left: blue (horizontal), red (vertical).
            b->data_ptr[(y + h / 2)* b->pixels_per_line + x] =
                (rel_y << 16) + rel_x + t;
            // Bottom left: rgb (both horizontal + vertical)
            b->data_ptr[(y + h / 2) * b->pixels_per_line + x + w / 2] =
                (((rel_x + rel_y) << 15) & 0xff0000) +
                (((rel_x + rel_y) << 7) & 0x00ff00) +
                (((rel_x + rel_y) >> 1) & 0x0000ff) + t;
       }
    }
    return b;
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_INFO;

    background_colors = _create_background(640, 400);
    if (NULL == background_colors) return EXIT_FAILURE;

    wlclient_ptr = wlclient_create("example_toplevel");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    _key_listener.notify = _handle_key;
    wl_signal_add(&wlclient_events(wlclient_ptr)->key, &_key_listener);



    if (wlclient_xdg_supported(wlclient_ptr)) {
        wlclient_xdg_toplevel_t *toplevel_ptr = wlclient_xdg_toplevel_create(
            wlclient_ptr, 640, 400);

        wlclient_xdg_toplevel_register_ready_callback(
            toplevel_ptr, _callback, toplevel_ptr);

        if (NULL != toplevel_ptr) {
            wlclient_run(wlclient_ptr);
            wlclient_xdg_toplevel_destroy(toplevel_ptr);
        } else {
            bs_log(BS_ERROR, "Failed wlclient_xdg_toplevel_create(%p)",
                   wlclient_ptr);
        }
    } else {
        bs_log(BS_ERROR, "XDG shell is not supported.");
    }

    wl_list_remove(&_key_listener.link);
    wlclient_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of wlmclock.c ==================================================== */
