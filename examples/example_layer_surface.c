/* ========================================================================= */
/**
 * @file example_layer_surface.c
 *
 * Example app for libwlclient layer shell support.
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright 2026 Google LLC
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
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include "wlclient/dblbuf.h"
#include "wlclient/layer_shell.h"
#include "wlclient/wlclient.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct zwlr_layer_surface_v1;

static void _layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *layer_surface_ptr,
    uint32_t serial,
    uint32_t width,
    uint32_t height);
static void _layer_surface_handle_closed(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *layer_surface_ptr);

/** State of the client. */
static wlmcl_client_t                *wlclient_ptr;
/** Background buffer. */
static bs_gfxbuf_t                   *background_colors;
/** Double buffer. */
static wlmcl_dblbuf_t                *dblbuf_ptr;
/** Wayland surface. */
struct wl_surface                    *wl_surface_ptr;

/** Listeners for the layer surface. */
static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = _layer_surface_handle_configure,
    .closed = _layer_surface_handle_closed,
};

/* ------------------------------------------------------------------------- */
/** Draws something into the buffer. */
static bool _callback(bs_gfxbuf_t *gfxbuf_ptr, __UNUSED__ void *ud_ptr)
{
    static uint64_t ns_base = 0;
    bs_log(BS_DEBUG, "Callback gfxbuf %p", gfxbuf_ptr);

    // If background dimensions differ (e.g. after resize), recreate background
    if (background_colors == NULL ||
        background_colors->width != gfxbuf_ptr->width ||
        background_colors->height != gfxbuf_ptr->height) {
        if (background_colors != NULL) {
            bs_gfxbuf_destroy(background_colors);
        }
        // Let's create a solid color background.
        background_colors = bs_gfxbuf_create(gfxbuf_ptr->width, gfxbuf_ptr->height);
        if (background_colors != NULL) {
            uint32_t fill_color = 0xd0202040; // Semi-transparent dark blue/purple
            for (unsigned i = 0; i < background_colors->width * background_colors->height; ++i) {
                background_colors->data_ptr[i] = fill_color;
            }
        }
    }

    if (background_colors != NULL) {
        bs_gfxbuf_copy(gfxbuf_ptr, background_colors);
    }

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) return false;

    // Draw some text and a small animation
    cairo_select_font_face(
        cairo_ptr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 18);
    cairo_set_source_argb8888(cairo_ptr, 0xffffffff);

    // Rotate text vertically since it's a side panel
    cairo_save(cairo_ptr);
    cairo_translate(cairo_ptr, 40, gfxbuf_ptr->height / 2.0);
    cairo_rotate(cairo_ptr, -M_PI_2);
    cairo_move_to(cairo_ptr, -100, 0);
    cairo_show_text(cairo_ptr, "wlmdock prototype");
    cairo_restore(cairo_ptr);

    // Animated indicator dot
    if (0 == ns_base) ns_base = bs_mono_nsec();
    double offset = sin(3.0e-9 * bs_mono_nsec() - ns_base);
    cairo_arc(cairo_ptr, gfxbuf_ptr->width / 2.0, gfxbuf_ptr->height / 2.0 + 80.0 * offset, 12.0, 0.0, 2.0 * M_PI);
    cairo_set_source_argb8888(cairo_ptr, 0xff00ff00); // Green dot
    cairo_fill(cairo_ptr);

    cairo_destroy(cairo_ptr);

    wlmcl_dblbuf_register_ready_callback(dblbuf_ptr, _callback, NULL);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles the `configure` callback from the wlr-layer-surface listener. */
void _layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr,
    uint32_t serial,
    __UNUSED__ uint32_t width,
    __UNUSED__ uint32_t height)
{
    wlmcl_client_t *wlclient_ptr = data_ptr;

    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_ptr, serial);

    if (NULL != dblbuf_ptr) {
        wlmcl_dblbuf_destroy(dblbuf_ptr);
    }

    dblbuf_ptr = wlmcl_dblbuf_create(
        wlmcl_client_attributes(wlclient_ptr)->app_id_ptr,
        wl_surface_ptr,
        wlmcl_client_attributes(wlclient_ptr)->wl_shm_ptr,
        100,
        300);
    if (NULL == dblbuf_ptr) {
        bs_log(BS_FATAL, "Failed wlmcl_dblbuf_create.");
        return;
    }
    wlmcl_dblbuf_register_ready_callback(dblbuf_ptr, _callback, NULL);
}

/* ------------------------------------------------------------------------- */
/** Handles the `closed` callback from the wlr-layer-surface listener. */
void _layer_surface_handle_closed(
    void *data_ptr,
    __UNUSED__ struct zwlr_layer_surface_v1 *layer_surface_ptr)
{
    wlmcl_client_t *wlclient_ptr = data_ptr;
    wlmcl_client_request_terminate(wlclient_ptr);
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_INFO;

    wlclient_ptr = wlmcl_client_create("example_layer_surface");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    struct zwlr_layer_shell_v1 *layer_shell_ptr = NULL;
    if (!wlmcl_layer_shell_register(wlclient_ptr, &layer_shell_ptr) ||
        !wlmcl_client_initialize(wlclient_ptr)) {
        return EXIT_FAILURE;
    }

    // Create as TOP layer, anchored to the right edge, spanning top to bottom.
    wl_surface_ptr = wl_compositor_create_surface(
        wlmcl_client_attributes(wlclient_ptr)->wl_compositor_ptr);
    if (NULL == wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               (void*)wlmcl_client_attributes(wlclient_ptr)->wl_compositor_ptr);
        return EXIT_FAILURE;
    }
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr =
        zwlr_layer_shell_v1_get_layer_surface(
            layer_shell_ptr,
            wl_surface_ptr,
            NULL,  // Let compositor choose output.
            ZWLR_LAYER_SHELL_V1_LAYER_TOP,
            "example_layer_surface");
    if (NULL == zwlr_layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed zwlr_layer_shell_v1_get_layer_surface(...)");
        return EXIT_FAILURE;
    }

    zwlr_layer_surface_v1_set_size(zwlr_layer_surface_ptr, 100, 300);
    zwlr_layer_surface_v1_set_anchor(
        zwlr_layer_surface_ptr,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    wl_surface_commit(wl_surface_ptr);

    if (0 != zwlr_layer_surface_v1_add_listener(
            zwlr_layer_surface_ptr,
            &layer_surface_listener,
            wlclient_ptr)) {
        bs_log(BS_ERROR, "Failed zwlr_layer_surface_v1_add_listener.");
        return EXIT_FAILURE;
    }

    // Run main loop
    wlmcl_client_run(wlclient_ptr);

    zwlr_layer_surface_v1_destroy(zwlr_layer_surface_ptr);
    wl_surface_destroy(wl_surface_ptr);
    if (NULL != dblbuf_ptr) {
        wlmcl_dblbuf_destroy(dblbuf_ptr);
        dblbuf_ptr = NULL;
    }

    if (background_colors != NULL) {
        bs_gfxbuf_destroy(background_colors);
    }
    wlmcl_client_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of example_layer_surface.c ======================================= */
