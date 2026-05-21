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
#include "wlclient/layer_surface.h"
#include "wlclient/wlclient.h"
#include "wlclient/dblbuf.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

/** State of the client. */
static wlmcl_client_t                *wlclient_ptr;
/** Background buffer. */
static bs_gfxbuf_t                   *background_colors;
/** Double buffer. */
static wlmcl_dblbuf_t                *dblbuf_ptr;

/* ------------------------------------------------------------------------- */
/** Draws something into the buffer. */
static bool _callback(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr)
{
    static uint64_t ns_base = 0;
    wlmcl_layer_surface_t *layer_surface_ptr = ud_ptr;
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

    wlmcl_dblbuf_register_ready_callback(
        dblbuf_ptr, _callback, layer_surface_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles configure events. */
static void _handle_configure(void *ud_ptr, uint32_t width, uint32_t height)
{
    wlmcl_layer_surface_t *layer_surface_ptr = ud_ptr;
    if (NULL != dblbuf_ptr) {
        wlmcl_dblbuf_destroy(dblbuf_ptr);
    }
    dblbuf_ptr = wlmcl_dblbuf_create(
        wlmcl_client_attributes(wlclient_ptr)->app_id_ptr,
        wlmcl_layer_surface_wl_surface(layer_surface_ptr),
        wlmcl_client_attributes(wlclient_ptr)->wl_shm_ptr,
        width,
        height);
    if (NULL == dblbuf_ptr) {
        bs_log(BS_FATAL, "Failed wlmcl_dblbuf_create.");
        return;
    }
    wlmcl_dblbuf_register_ready_callback(
        dblbuf_ptr, _callback, layer_surface_ptr);
}

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_INFO;

    wlclient_ptr = wlmcl_client_create("example_layer_surface");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    if (wlmcl_layer_shell_supported(wlclient_ptr)) {
        // Create as TOP layer, anchored to the right edge, spanning top to bottom
        wlmcl_layer_surface_t *layer_surface_ptr = wlmcl_layer_surface_create(
            wlclient_ptr,
            ZWLR_LAYER_SHELL_V1_LAYER_TOP,
            "wlmdock-panel",
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
            100,
            300);

        if (NULL != layer_surface_ptr) {
            // Register configure callback
            wlmcl_layer_surface_register_configure_callback(
                layer_surface_ptr, _handle_configure, layer_surface_ptr);

            // Run main loop
            wlmcl_client_run(wlclient_ptr);

            wlmcl_layer_surface_destroy(layer_surface_ptr);
            if (NULL != dblbuf_ptr) {
                wlmcl_dblbuf_destroy(dblbuf_ptr);
                dblbuf_ptr = NULL;
            }
        } else {
            bs_log(BS_ERROR, "Failed wlmcl_layer_surface_create(%p)",
                   wlclient_ptr);
        }
    } else {
        bs_log(BS_ERROR, "Layer shell is not supported.");
    }

    if (background_colors != NULL) {
        bs_gfxbuf_destroy(background_colors);
    }
    wlmcl_client_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of example_layer_surface.c ======================================= */
