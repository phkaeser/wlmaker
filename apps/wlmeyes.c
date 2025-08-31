/* ========================================================================= */
/**
 * @file wlmeyes.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "libwlclient/xdg_toplevel.h"
#include "libwlclient/libwlclient.h"

/* == Data ================================================================= */

/** State of the client. */
wlclient_t                    *wlclient_ptr;
/** Listener for key events. */
static struct wl_listener     _key_listener;
/** Most recent X position of the pointer. */
double                        pointer_x;
/** Most recent Y position of the pointer. */
double                        pointer_y;

/* == Local (static) methods =============================================== */

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
/** @return x * x. */
static inline double sqr(double x)
{
    return x * x;
}

void _draw_around(cairo_t *cairo_ptr,
                  double x,
                  double y,
                  int width,
                  int height)
{
    double diag = sqrt(width * width + height * height);

    cairo_save(cairo_ptr);

    cairo_translate(cairo_ptr, x * width, y * height);
    cairo_scale(cairo_ptr, 0.2 * width / diag, 0.4 * height / diag);

    cairo_set_line_width(cairo_ptr, 0);
    cairo_set_source_rgb(cairo_ptr, 1.0, 1.0, 1.0);
    cairo_arc(cairo_ptr,
              0.0, 0.0,
              diag,
              0, 2 * M_PI);
    cairo_fill(cairo_ptr);

    cairo_set_line_width(cairo_ptr, sqrt(width * width + height * height) / 10);
    cairo_set_source_rgb(cairo_ptr, 0.0, 0.0, 0.0);
    cairo_arc(cairo_ptr,
              0.0, 0.0,
              diag,
              0, 2 * M_PI);
    cairo_stroke(cairo_ptr);

    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
/** Draws the eyes' pupil, in relative coordinates. */
void _draw_pupil(cairo_t *cairo_ptr,
                 double px,
                 double py,
                 double w,
                 double h,
                 int width,
                 int height)
{
    double rel_x = pointer_x - px;
    double rel_y = pointer_y - py;

    // Scale the position back to the ellipsis.
    double ratio = sqr(rel_x / w) + sqr(rel_y / h);
    if (ratio > 1.0) {
        rel_x = rel_x / sqrt(ratio);
        rel_y = rel_y / sqrt(ratio);
    }

    int x = width * (rel_x + px);
    int y = height * (rel_y + py);

    cairo_save(cairo_ptr);

    cairo_set_source_rgb(cairo_ptr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cairo_ptr, sqrt(width * width + height * height) / 15);
    cairo_set_line_cap(cairo_ptr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cairo_ptr, x, y);
    cairo_line_to(cairo_ptr, x, y);
    cairo_stroke(cairo_ptr);

    cairo_restore(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
/** Draws something into the buffer. */
static bool _callback(bs_gfxbuf_t *gfxbuf_ptr, __UNUSED__ void *ud_ptr)
{
    bs_gfxbuf_clear(gfxbuf_ptr, 0);

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) return false;

    _draw_around(cairo_ptr, 0.25, 0.5,
                 gfxbuf_ptr->width, gfxbuf_ptr->height);
    _draw_around(cairo_ptr, 0.75, 0.5,
                 gfxbuf_ptr->width, gfxbuf_ptr->height);

    _draw_pupil(cairo_ptr, 0.25, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    _draw_pupil(cairo_ptr, 0.75, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates pointer position. */
static void _position_callback(double x, double y, void *ud_ptr)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = ud_ptr;

    pointer_x = x;
    pointer_y = y;
    wlclient_xdg_toplevel_register_ready_callback(
        toplevel_ptr, _callback, toplevel_ptr);
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_INFO;

    wlclient_ptr = wlclient_create("wlmeyes");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    _key_listener.notify = _handle_key;
    wl_signal_add(&wlclient_events(wlclient_ptr)->key, &_key_listener);

    if (wlclient_xdg_supported(wlclient_ptr)) {
        wlclient_xdg_toplevel_t *toplevel_ptr = wlclient_xdg_toplevel_create(
            wlclient_ptr, "wlmaker Toplevel Example", 512, 384);

        if (NULL != toplevel_ptr) {

            wlclient_xdg_toplevel_register_ready_callback(
                toplevel_ptr, _callback, toplevel_ptr);
            wlclient_xdg_toplevel_register_position_callback(
                toplevel_ptr, _position_callback, toplevel_ptr);

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

/* == End of wlmeyes.c ===================================================== */
