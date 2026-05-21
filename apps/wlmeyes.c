/* ========================================================================= */
/**
 * @file wlmeyes.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "wlclient/xdg_toplevel.h"
#include "wlclient/wlclient.h"
#include "wlclient/icon.h"
#include "wlclient/dblbuf.h"

/* == Data ================================================================= */

/** State of the client. */
wlmcl_client_t                    *wlclient_ptr;
/** Double buffer for toplevel. */
static wlmcl_dblbuf_t             *toplevel_dblbuf_ptr;
/** Double buffer for icon. */
static wlmcl_dblbuf_t             *icon_dblbuf_ptr;
/** Listener for key events. */
static struct wl_listener     _key_listener;
/** Most recent X position of the pointer. */
double                        pointer_x;
/** Most recent Y position of the pointer. */
double                        pointer_y;
/** Most recent X position of the pointer relative to the ivon. */
double                        icon_pointer_x;
/** Most recent Y position of the pointer relative to the ivon. */
double                        icon_pointer_y;

/** Desired width of the toplevel, in pixels. */
uint32_t                      toplevel_width;
/** Desired height of the toplevel, in pixels. */
uint32_t                      toplevel_height;

/** Commandline arguments. */
static const bs_arg_t _wlmeyes_args[] = {
    BS_ARG_UINT32(
        "width",
        "Desired width of the XDG toplevel window, in pixels.",
        512,
        1,
        INT32_MAX,
        &toplevel_width),
    BS_ARG_UINT32(
        "height",
        "Desired height of the XDG toplevel window, in pixels.",
        384,
        1,
        INT32_MAX,
        &toplevel_height),
    BS_ARG_SENTINEL(),
};

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles key events. */
static void _handle_key(__UNUSED__ struct wl_listener *listener_ptr,
                        void *data_ptr)
{
    struct wlmcl_client_key_event *event_ptr = data_ptr;

    if (!event_ptr->pressed) return;
    char name[128];
    if (0 <= xkb_keysym_get_name(event_ptr->keysym, name, sizeof(name))) {
        bs_log(BS_INFO, "Key press received: %s", name);
    }

    if (XKB_KEY_Escape == event_ptr->keysym ||
        XKB_KEY_q == event_ptr->keysym ||
        XKB_KEY_Q == event_ptr->keysym) {
        wlmcl_client_request_terminate(wlclient_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** @return x * x. */
static inline double sqr(double x)
{
    return x * x;
}

/* ------------------------------------------------------------------------- */
/** Draws the white + border of the eye. */
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
                 double pointer_x,
                 double pointer_y,
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

    _draw_pupil(cairo_ptr,
                pointer_x, pointer_y,
                0.25, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    _draw_pupil(cairo_ptr,
                pointer_x, pointer_y,
                0.75, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates pointer position. */
static void _position_callback(double x, double y, __UNUSED__ void *ud_ptr)
{
    pointer_x = x;
    pointer_y = y;
    if (NULL != toplevel_dblbuf_ptr) {
        wlmcl_dblbuf_register_ready_callback(
            toplevel_dblbuf_ptr, _callback, NULL);
    }
}

/* ------------------------------------------------------------------------- */
/** Called when the icon is ready to refresh. */
static bool _icon_callback(bs_gfxbuf_t *gfxbuf_ptr, __UNUSED__ void *ud_ptr)
{
    bs_gfxbuf_clear(gfxbuf_ptr, 0);

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) return false;

    _draw_around(cairo_ptr, 0.25, 0.5,
                 gfxbuf_ptr->width, gfxbuf_ptr->height);
    _draw_around(cairo_ptr, 0.75, 0.5,
                 gfxbuf_ptr->width, gfxbuf_ptr->height);

    _draw_pupil(cairo_ptr,
                icon_pointer_x, icon_pointer_y,
                0.25, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    _draw_pupil(cairo_ptr,
                icon_pointer_x, icon_pointer_y,
                0.75, 0.5, 0.13, 0.3,
                gfxbuf_ptr->width, gfxbuf_ptr->height);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates pointer position for the icon. */
static void _icon_position_callback(double x, double y, __UNUSED__ void *ud_ptr)
{
    icon_pointer_x = x;
    icon_pointer_y = y;
    if (NULL != icon_dblbuf_ptr) {
        wlmcl_dblbuf_register_ready_callback(
            icon_dblbuf_ptr, _icon_callback, NULL);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles configure events for the toplevel. */
static void _handle_toplevel_configure(void *ud_ptr, uint32_t width, uint32_t height)
{
    wlmcl_xdg_toplevel_t *toplevel_ptr = ud_ptr;
    if (NULL != toplevel_dblbuf_ptr) {
        wlmcl_dblbuf_destroy(toplevel_dblbuf_ptr);
    }
    toplevel_dblbuf_ptr = wlmcl_dblbuf_create(
        wlmcl_client_attributes(wlclient_ptr)->app_id_ptr,
        wlmcl_xdg_toplevel_wl_surface(toplevel_ptr),
        wlmcl_client_attributes(wlclient_ptr)->wl_shm_ptr,
        width,
        height);
    if (NULL == toplevel_dblbuf_ptr) {
        bs_log(BS_FATAL, "Failed wlmcl_dblbuf_create for toplevel.");
        return;
    }
    wlmcl_dblbuf_register_ready_callback(
        toplevel_dblbuf_ptr, _callback, NULL);
}

/* ------------------------------------------------------------------------- */
/** Handles configure events for the icon. */
static void _handle_icon_configure(void *ud_ptr, uint32_t width, uint32_t height)
{
    wlmcl_icon_t *icon_ptr = ud_ptr;
    if (NULL != icon_dblbuf_ptr) {
        wlmcl_dblbuf_destroy(icon_dblbuf_ptr);
    }
    icon_dblbuf_ptr = wlmcl_dblbuf_create(
        wlmcl_client_attributes(wlclient_ptr)->app_id_ptr,
        wlmcl_icon_wl_surface(icon_ptr),
        wlmcl_client_attributes(wlclient_ptr)->wl_shm_ptr,
        width,
        height);
    if (NULL == icon_dblbuf_ptr) {
        bs_log(BS_FATAL, "Failed wlmcl_dblbuf_create for icon.");
        return;
    }
    wlmcl_dblbuf_register_ready_callback(
        icon_dblbuf_ptr, _icon_callback, NULL);
}

/* == Main program ========================================================= */
/** Main program. */
int main(int argc, const char **argv)
{
    bs_log_severity = BS_INFO;

    if (!bs_arg_parse(_wlmeyes_args, BS_ARG_MODE_NO_EXTRA, &argc, argv)) {
        bs_arg_print_usage(stderr, _wlmeyes_args);
        return EXIT_FAILURE;
    }

    wlclient_ptr = wlmcl_client_create("wlmaker.wlmeyes");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    _key_listener.notify = _handle_key;
    wl_signal_add(&wlmcl_client_events(wlclient_ptr)->key, &_key_listener);

    if (wlmcl_xdg_supported(wlclient_ptr)) {
        wlmcl_xdg_toplevel_t *toplevel_ptr = wlmcl_xdg_toplevel_create(
            wlclient_ptr,
            "wlmaker Toplevel Example",
            toplevel_width,
            toplevel_height);
        if (NULL == toplevel_ptr) {
            bs_log(BS_ERROR, "Failed wlmcl_xdg_toplevel_create(%p)",
                   wlclient_ptr);
        } else {

            wlmcl_xdg_decoration_set_server_side(toplevel_ptr, false);
            wlmcl_xdg_toplevel_register_configure_callback(
                toplevel_ptr, _handle_toplevel_configure, toplevel_ptr);
            wlmcl_xdg_toplevel_register_position_callback(
                toplevel_ptr, _position_callback, toplevel_ptr);

            wlmcl_icon_t *icon_ptr = wlmcl_icon_create(wlclient_ptr);
            if (NULL != icon_ptr) {
                wlmcl_icon_register_configure_callback(
                    icon_ptr, _handle_icon_configure, icon_ptr);
                wlmcl_icon_register_position_callback(
                    icon_ptr, _icon_position_callback, icon_ptr);
            }

            wlmcl_client_run(wlclient_ptr);
            wlmcl_xdg_toplevel_destroy(toplevel_ptr);
            if (NULL != icon_ptr) {
                wlmcl_icon_destroy(icon_ptr);
            }
            if (NULL != toplevel_dblbuf_ptr) {
                wlmcl_dblbuf_destroy(toplevel_dblbuf_ptr);
                toplevel_dblbuf_ptr = NULL;
            }
            if (NULL != icon_dblbuf_ptr) {
                wlmcl_dblbuf_destroy(icon_dblbuf_ptr);
                icon_dblbuf_ptr = NULL;
            }
        }
    } else {
        bs_log(BS_ERROR, "XDG shell is not supported.");
    }

    wl_list_remove(&_key_listener.link);
    wlmcl_client_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}

/* == End of wlmeyes.c ===================================================== */
