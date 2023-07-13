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

#include "buffer.h"
#include "wlmaker-icon-unstable-v1-client-protocol.h"

#include <wayland-server-core.h>

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
    wlclient_icon_gfxbuf_callback_t buffer_ready_callback;
    /** Argument to that callback. */
    void                      *buffer_ready_callback_ud_ptr;

    /** The buffer backing the icon. */
    wlclient_buffer_t        *buffer_ptr;

    /** Outstanding frames to display. Considered ready to draw when zero. */
    int                       pending_frames;
    /** Whether the buffer was reported as ready. */
    bool                      buffer_ready;
} wlclient_icon_t;

static void handle_toplevel_icon_configure(
    void *data_ptr,
    struct zwlmaker_toplevel_icon_v1 *zwlmaker_toplevel_icon_v1_ptr,
    int32_t width,
    int32_t height,
    uint32_t serial);
static void handle_frame_done(
    void *data_ptr,
    struct wl_callback *callback,
    uint32_t time);
static void handle_buffer_ready(void *data_ptr);
static void state(wlclient_icon_t *icon_ptr);

/* == Data ================================================================= */

/** Listener implementation for toplevel icon. */
static const struct zwlmaker_toplevel_icon_v1_listener toplevel_icon_listener={
    .configure = handle_toplevel_icon_configure,
};

/** Listener implementation for the frame. */
static const struct wl_callback_listener frame_listener = {
    .done = handle_frame_done
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
void wlclient_icon_callback_when_ready(
    wlclient_icon_t *icon_ptr,
    wlclient_icon_gfxbuf_callback_t callback,
    void *ud_ptr)
{
    icon_ptr->buffer_ready_callback = callback;
    icon_ptr->buffer_ready_callback_ud_ptr = ud_ptr;

    state(icon_ptr);
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

    icon_ptr->buffer_ptr = wlclient_buffer_create(
        wlclient_ptr, icon_ptr->width, icon_ptr->height,
        handle_buffer_ready, icon_ptr);
    if (NULL == icon_ptr->buffer_ptr) {
        bs_log(BS_FATAL, "Failed wlclient_buffer_create(%p, %u, %u)",
               wlclient_ptr, icon_ptr->width, icon_ptr->height);
        // TODO(kaeser@gubbe.ch): Error handling.
        return;
    }

    state(icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the information that there is a buffer ready to be drawn into.
 *
 * @param data_ptr
 */
void handle_buffer_ready(void *data_ptr)
{
    wlclient_icon_t *icon_ptr = data_ptr;
    icon_ptr->buffer_ready = true;
    state(icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Registers the frame got displayed, potentially triggers the callback.
 *
 * @param data_ptr
 * @param callback
 * @param time
 */
void handle_frame_done(
    void *data_ptr,
    struct wl_callback *callback,
    __UNUSED__ uint32_t time)
{
    wl_callback_destroy(callback);

    wlclient_icon_t *icon_ptr = data_ptr;
    icon_ptr->pending_frames--;
    state(icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Runs the ready callback, if due.
 *
 * @param icon_ptr
 */
void state(wlclient_icon_t *icon_ptr)
{
    // Not fully initialized, skip this attempt.
    if (NULL == icon_ptr->buffer_ptr) return;
    // ... or, no callback...
    if (NULL == icon_ptr->buffer_ready_callback) return;
    // ... or, actually not ready.
    if (0 < icon_ptr->pending_frames || !icon_ptr->buffer_ready) return;

    bool rv = icon_ptr->buffer_ready_callback(
        icon_ptr->wlclient_ptr,
        bs_gfxbuf_from_wlclient_buffer(icon_ptr->buffer_ptr),
        icon_ptr->buffer_ready_callback_ud_ptr);
    if (!rv) return;

    struct wl_callback *callback = wl_surface_frame(
        icon_ptr->wl_surface_ptr);
    wl_callback_add_listener(callback, &frame_listener, icon_ptr);

    wl_surface_damage_buffer(
        icon_ptr->wl_surface_ptr,
        0, 0, INT32_MAX, INT32_MAX);

    icon_ptr->pending_frames++;
    icon_ptr->buffer_ready = false;
    wlclient_buffer_attach_to_surface_and_commit(
        icon_ptr->buffer_ptr,
        icon_ptr->wl_surface_ptr);
}

/* == End of icon.c ======================================================== */
