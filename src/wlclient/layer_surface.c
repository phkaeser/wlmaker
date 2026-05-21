/* ========================================================================= */
/**
 * @file layer_surface.c
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

#include "layer_surface.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include "wlclient.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

/* == Declarations ========================================================= */

struct zwlr_layer_surface_v1;

/** State of the layer surface. */
struct _wlmcl_layer_surface_t {
    /** Back-link to the client. */
    wlmcl_client_t                *wlclient_ptr;

    /** Surface. */
    struct wl_surface             *wl_surface_ptr;
    /** Layer-shell surface wrapping `wl_surface_ptr`. */
    struct zwlr_layer_surface_v1  *layer_surface_ptr;

    /** Width of the layer surface. */
    uint32_t                      width;
    /** Height of the layer surface. */
    uint32_t                      height;

    /** Whether the surface has been configured. */
    bool                          configured;

    /** Callback for input configure. */
    void                          (*configure_callback)(void *ud_ptr, uint32_t width, uint32_t height);
    /** Client-provided argument to configure_callback. */
    void                          *configure_callback_ud_ptr;
};

static void _wlmcl_layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *layer_surface_ptr,
    uint32_t serial,
    uint32_t width,
    uint32_t height);

static void _wlmcl_layer_surface_handle_closed(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *layer_surface_ptr);

/* == Data ================================================================= */

/** Listeners for the layer surface. */
static const struct zwlr_layer_surface_v1_listener
_wlmcl_layer_surface_listener = {
    .configure = _wlmcl_layer_surface_handle_configure,
    .closed = _wlmcl_layer_surface_handle_closed,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmcl_layer_shell_supported(wlmcl_client_t *wlclient_ptr)
{
    return (NULL != wlmcl_client_attributes(wlclient_ptr)->layer_shell_ptr);
}

/* ------------------------------------------------------------------------- */
wlmcl_layer_surface_t *wlmcl_layer_surface_create(
    wlmcl_client_t *wlclient_ptr,
    uint32_t layer,
    const char *namespace_ptr,
    uint32_t anchor,
    uint32_t width,
    uint32_t height)
{
    if (!wlmcl_layer_shell_supported(wlclient_ptr)) {
        bs_log(BS_ERROR, "Layer shell is not supported.");
        return NULL;
    }

    wlmcl_layer_surface_t *layer_surface_ptr = logged_calloc(
        1, sizeof(wlmcl_layer_surface_t));
    if (NULL == layer_surface_ptr) return NULL;
    layer_surface_ptr->wlclient_ptr = wlclient_ptr;
    layer_surface_ptr->width = width;
    layer_surface_ptr->height = height;

    layer_surface_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlmcl_client_attributes(wlclient_ptr)->wl_compositor_ptr);
    if (NULL == layer_surface_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               (void*)wlmcl_client_attributes(wlclient_ptr)->wl_compositor_ptr);
        wlmcl_layer_surface_destroy(layer_surface_ptr);
        return NULL;
    }

    layer_surface_ptr->layer_surface_ptr = zwlr_layer_shell_v1_get_layer_surface(
        wlmcl_client_attributes(wlclient_ptr)->layer_shell_ptr,
        layer_surface_ptr->wl_surface_ptr,
        NULL,  // Let compositor choose output.
        layer,
        namespace_ptr);
    if (NULL == layer_surface_ptr->layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed zwlr_layer_shell_v1_get_layer_surface.");
        wlmcl_layer_surface_destroy(layer_surface_ptr);
        return NULL;
    }

    zwlr_layer_surface_v1_set_size(
        layer_surface_ptr->layer_surface_ptr, width, height);
    zwlr_layer_surface_v1_set_anchor(
        layer_surface_ptr->layer_surface_ptr, anchor);

    if (0 != zwlr_layer_surface_v1_add_listener(
            layer_surface_ptr->layer_surface_ptr,
            &_wlmcl_layer_surface_listener,
            layer_surface_ptr)) {
        bs_log(BS_ERROR, "Failed zwlr_layer_surface_v1_add_listener.");
        wlmcl_layer_surface_destroy(layer_surface_ptr);
        return NULL;
    }

    wl_surface_commit(layer_surface_ptr->wl_surface_ptr);

    return layer_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmcl_layer_surface_destroy(wlmcl_layer_surface_t *layer_surface_ptr)
{
    if (NULL == layer_surface_ptr) return;

    if (NULL != layer_surface_ptr->layer_surface_ptr) {
        zwlr_layer_surface_v1_destroy(layer_surface_ptr->layer_surface_ptr);
        layer_surface_ptr->layer_surface_ptr = NULL;
    }



    if (NULL != layer_surface_ptr->wl_surface_ptr) {
        wl_surface_destroy(layer_surface_ptr->wl_surface_ptr);
        layer_surface_ptr->wl_surface_ptr = NULL;
    }

    free(layer_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmcl_layer_surface_request_size(
    wlmcl_layer_surface_t *layer_surface_ptr,
    uint32_t width,
    uint32_t height)
{
    zwlr_layer_surface_v1_set_size(
        layer_surface_ptr->layer_surface_ptr, width, height);
    wl_surface_commit(layer_surface_ptr->wl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmcl_layer_surface_set_margin(
    wlmcl_layer_surface_t *layer_surface_ptr,
    int32_t top,
    int32_t right,
    int32_t bottom,
    int32_t left)
{
    zwlr_layer_surface_v1_set_margin(
        layer_surface_ptr->layer_surface_ptr, top, right, bottom, left);
    wl_surface_commit(layer_surface_ptr->wl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmcl_layer_surface_set_exclusive_zone(
    wlmcl_layer_surface_t *layer_surface_ptr,
    int32_t pixels)
{
    zwlr_layer_surface_v1_set_exclusive_zone(
        layer_surface_ptr->layer_surface_ptr, pixels);
    wl_surface_commit(layer_surface_ptr->wl_surface_ptr);
}


/* ------------------------------------------------------------------------- */
struct wl_surface *wlmcl_layer_surface_wl_surface(wlmcl_layer_surface_t *layer_surface_ptr)
{
    return layer_surface_ptr->wl_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmcl_layer_surface_register_configure_callback(
    wlmcl_layer_surface_t *layer_surface_ptr,
    void (*callback)(void *ud_ptr, uint32_t width, uint32_t height),
    void *ud_ptr)
{
    layer_surface_ptr->configure_callback = callback;
    layer_surface_ptr->configure_callback_ud_ptr = ud_ptr;
    if (layer_surface_ptr->configured && NULL != callback) {
        callback(ud_ptr, layer_surface_ptr->width, layer_surface_ptr->height);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `configure` event of the layer surface.
 *
 * @param data_ptr            Untyped pointer to @ref wlmcl_layer_surface_t.
 * @param layer_surface_ptr
 * @param serial
 * @param width
 * @param height
 */
void _wlmcl_layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *layer_surface_ptr,
    uint32_t serial,
    uint32_t width,
    uint32_t height)
{
    wlmcl_layer_surface_t *layer_surface_ptr_ = data_ptr;
    bs_log(BS_DEBUG, "Layer surface configure: size %" PRIu32 "x%" PRIu32 ", serial %" PRIu32,
           width, height, serial);

    zwlr_layer_surface_v1_ack_configure(layer_surface_ptr, serial);

    if (width == 0 || height == 0) {
        // Client should decide. We keep our current width/height.
        width = layer_surface_ptr_->width;
        height = layer_surface_ptr_->height;
    }

    layer_surface_ptr_->width = width;
    layer_surface_ptr_->height = height;
    layer_surface_ptr_->configured = true;

    if (NULL != layer_surface_ptr_->configure_callback) {
        layer_surface_ptr_->configure_callback(
            layer_surface_ptr_->configure_callback_ud_ptr,
            width,
            height);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `closed` event of the layer surface.
 *
 * @param data_ptr            Untyped pointer to @ref wlmcl_layer_surface_t.
 * @param layer_surface_ptr
 */
void _wlmcl_layer_surface_handle_closed(
    void *data_ptr,
    __UNUSED__ struct zwlr_layer_surface_v1 *layer_surface_ptr)
{
    wlmcl_layer_surface_t *layer_surface_ptr_ = data_ptr;
    bs_log(BS_INFO, "Layer surface closed by compositor.");
    wlmcl_client_request_terminate(layer_surface_ptr_->wlclient_ptr);
}

/* == End of layer_surface.c ================================================= */
