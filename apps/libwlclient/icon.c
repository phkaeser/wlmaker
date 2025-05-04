/* ========================================================================= */
/**
 * @file icon.c
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

#include "icon.h"

#include "dblbuf.h"
#include "wlmaker-icon-unstable-v1-client-protocol.h"

/* == Declarations ========================================================= */

/** State of the icon. */
typedef struct _wlclient_icon_t {
    /** Back-link to the client. */
    wlclient_t                *wlclient_ptr;

    /** Surface. */
    struct wl_surface         *wl_surface_ptr;
    /** The icon interface. */
    struct zwlmaker_toplevel_icon_v1 *toplevel_icon_ptr;

    /** Width of the icon, once suggested by the server. */
    unsigned                  width;
    /** Height of the icon, once suggested by the server. */
    unsigned                  height;

    /** Callback for when the icon's buffer is ready to be drawn into. */
    wlcl_dblbuf_ready_callback_t ready_callback;
    /** Argument to that callback. */
    void                      *ready_callback_ud_ptr;

    /** Double-buffered state of the surface. */
    wlcl_dblbuf_t             *dblbuf_ptr;

} wlclient_icon_t;

static void handle_toplevel_icon_configure(
    void *data_ptr,
    struct zwlmaker_toplevel_icon_v1 *zwlmaker_toplevel_icon_v1_ptr,
    int32_t width,
    int32_t height,
    uint32_t serial);

/* == Data ================================================================= */

/** Listener implementation for toplevel icon. */
static const struct zwlmaker_toplevel_icon_v1_listener toplevel_icon_listener={
    .configure = handle_toplevel_icon_configure,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_icon_t *wlclient_icon_create(wlclient_t *wlclient_ptr)
{
    if (!wlclient_icon_supported(wlclient_ptr)) {
        bs_log(BS_ERROR, "Icon manager is not supported.");
        return NULL;
    }

    wlclient_icon_t *icon_ptr = logged_calloc(1, sizeof(wlclient_icon_t));
    if (NULL == icon_ptr) return NULL;
    icon_ptr->wlclient_ptr = wlclient_ptr;

    icon_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
    if (NULL == icon_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
        wlclient_icon_destroy(icon_ptr);
        return NULL;
    }

    icon_ptr->toplevel_icon_ptr = zwlmaker_icon_manager_v1_get_toplevel_icon(
        wlclient_attributes(wlclient_ptr)->icon_manager_ptr,
        NULL,
        icon_ptr->wl_surface_ptr);
    if (NULL == icon_ptr->toplevel_icon_ptr) {
        bs_log(BS_ERROR, "Failed  zwlmaker_icon_manager_v1_get_toplevel_icon"
               "(%p, NULL, %p).",
               wlclient_attributes(wlclient_ptr)->icon_manager_ptr,
               icon_ptr->wl_surface_ptr);
        wlclient_icon_destroy(icon_ptr);
        return NULL;
    }

    zwlmaker_toplevel_icon_v1_add_listener(
        icon_ptr->toplevel_icon_ptr,
        &toplevel_icon_listener,
        icon_ptr);
    wl_surface_commit(icon_ptr->wl_surface_ptr);

    return icon_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_icon_destroy(wlclient_icon_t *icon_ptr)
{
    if (NULL != icon_ptr->toplevel_icon_ptr) {
        // TODO(kaeser@gubbe.ch): Destroy the icon!
        icon_ptr->toplevel_icon_ptr = NULL;
    }

    if (NULL != icon_ptr->dblbuf_ptr) {
        wlcl_dblbuf_destroy(icon_ptr->dblbuf_ptr);
        icon_ptr->dblbuf_ptr = NULL;
    }

    if (NULL != icon_ptr->wl_surface_ptr) {
        wl_surface_destroy(icon_ptr->wl_surface_ptr);
        icon_ptr->wl_surface_ptr = NULL;
    }

    free(icon_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlclient_icon_supported(
    wlclient_t *wlclient_ptr)
{
    return (NULL != wlclient_attributes(wlclient_ptr)->icon_manager_ptr);
}

/* ------------------------------------------------------------------------ */
void wlclient_icon_register_ready_callback(
    wlclient_icon_t *icon_ptr,
    bool (*callback)(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr),
    void *ud_ptr)
{
    if (NULL != icon_ptr->dblbuf_ptr) {
        wlcl_dblbuf_register_ready_callback(
            icon_ptr->dblbuf_ptr, callback, ud_ptr);
    } else {
        icon_ptr->ready_callback = callback;
        icon_ptr->ready_callback_ud_ptr = ud_ptr;
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'configure' event: Creates appropriately sized buffer.
 *
 * @param data_ptr
 * @param zwlmaker_toplevel_icon_v1_ptr
 * @param width
 * @param height
 * @param serial
 */
void handle_toplevel_icon_configure(
    void *data_ptr,
    struct zwlmaker_toplevel_icon_v1 *zwlmaker_toplevel_icon_v1_ptr,
    int32_t width,
    int32_t height,
    uint32_t serial)
{
    wlclient_icon_t *icon_ptr = data_ptr;
    icon_ptr->width = width;
    icon_ptr->height = height;
    bs_log(BS_DEBUG, "Configured icon to %"PRId32" x %"PRId32, width, height);
    zwlmaker_toplevel_icon_v1_ack_configure(
        zwlmaker_toplevel_icon_v1_ptr, serial);

    wlclient_t *wlclient_ptr = icon_ptr->wlclient_ptr;

    icon_ptr->dblbuf_ptr = wlcl_dblbuf_create(
        wlclient_attributes(wlclient_ptr)->app_id_ptr,
        icon_ptr->wl_surface_ptr,
        wlclient_attributes(wlclient_ptr)->wl_shm_ptr,
        icon_ptr->width,
        icon_ptr->height);
    if (NULL == icon_ptr->dblbuf_ptr) {
        bs_log(BS_FATAL, "Failed wlcl_dblbuf_create(%p, %p, %u, %u)",
               icon_ptr->wl_surface_ptr,
               wlclient_attributes(wlclient_ptr)->wl_shm_ptr,
               icon_ptr->width,
               icon_ptr->height);
        // TODO(kaeser@gubbe.ch): Error handling.
    }

    wlcl_dblbuf_ready_callback_t callback = icon_ptr->ready_callback;
    if (NULL != callback) {
        icon_ptr->ready_callback = NULL;
        wlcl_dblbuf_register_ready_callback(
            icon_ptr->dblbuf_ptr,
            callback,
            icon_ptr->ready_callback_ud_ptr);
    }
}

/* == End of icon.c ======================================================== */
