/* ========================================================================= */
/**
 * @file xdg_toplevel.c
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

#include "xdg_toplevel.h"

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include "buffer.h"

/* == Declarations ========================================================= */

/** State of the XDG toplevel. */
struct _wlclient_xdg_toplevel_t {
    /** Back-link to the client. */
    wlclient_t                *wlclient_ptr;

    /** Surface. */
    struct wl_surface         *wl_surface_ptr;
    /** Wrapped as XDG surface. */
    struct xdg_surface        *xdg_surface_ptr;
    /** The XDG toplevel. */
    struct xdg_toplevel       *xdg_toplevel_ptr;
};

static void _wlclient_xdg_surface_configure(
    void *data,
    struct xdg_surface *xdg_surface,
    uint32_t serial);

/* == Data ================================================================= */

/** Listeners for the XDG surface. */
static const struct xdg_surface_listener _wlclient_xdg_surface_listener = {
    .configure = _wlclient_xdg_surface_configure,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_xdg_toplevel_t *wlclient_xdg_toplevel_create(
    wlclient_t *wlclient_ptr)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = logged_calloc(
        1, sizeof(wlclient_xdg_toplevel_t));
    if (NULL == toplevel_ptr) return NULL;
    toplevel_ptr->wlclient_ptr = wlclient_ptr;

    toplevel_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
    if (NULL == toplevel_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    toplevel_ptr->xdg_surface_ptr = xdg_wm_base_get_xdg_surface(
        wlclient_attributes(wlclient_ptr)->xdg_wm_base_ptr,
        toplevel_ptr->wl_surface_ptr);
    if (NULL == toplevel_ptr->xdg_surface_ptr) {
        bs_log(BS_ERROR, "Failed xdg_wm_base_get_xdg_surface(%p, %p)",
               wlclient_attributes(wlclient_ptr)->xdg_wm_base_ptr,
               toplevel_ptr->wl_surface_ptr);
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }
    xdg_surface_add_listener(
        toplevel_ptr->xdg_surface_ptr,
        &_wlclient_xdg_surface_listener,
        toplevel_ptr);
    toplevel_ptr->xdg_toplevel_ptr = xdg_surface_get_toplevel(
        toplevel_ptr->xdg_surface_ptr);
    if (NULL == toplevel_ptr->xdg_toplevel_ptr) {
        bs_log(BS_ERROR, "Failed xdg_surface_get_toplevel(%p)",
               toplevel_ptr->wl_surface_ptr);
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    wl_surface_commit(toplevel_ptr->wl_surface_ptr);
    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_xdg_toplevel_destroy(wlclient_xdg_toplevel_t *toplevel_ptr)
{
    if (NULL != toplevel_ptr->wl_surface_ptr) {
        wl_surface_destroy(toplevel_ptr->wl_surface_ptr);
        toplevel_ptr->wl_surface_ptr = NULL;
    }

    free(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlclient_xdg_supported(wlclient_t *wlclient_ptr)
{
    return (NULL != wlclient_attributes(wlclient_ptr)->xdg_wm_base_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `configure` event of the XDG surface.
 *
 * @param data_ptr            Untyped pointer to @ref wlclient_xdg_toplevel_t.
 * @param xdg_surface_ptr
 * @param serial
 */
void _wlclient_xdg_surface_configure(
    void *data_ptr,
    struct xdg_surface *xdg_surface_ptr,
    uint32_t serial)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = data_ptr;
    xdg_surface_ack_configure(xdg_surface_ptr, serial);

    wlclient_buffer_t *buffer_ptr = wlclient_buffer_create(
        toplevel_ptr->wlclient_ptr, 640, 480, NULL, NULL);
    if (NULL == buffer_ptr) {
        bs_log(BS_FATAL, "Failed wlclient_buffer_create(%p, %u, %u)",
               toplevel_ptr->wlclient_ptr, 640, 480);
        // TODO(kaeser@gubbe.ch): Error handling.
        return;
    }

    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_from_wlclient_buffer(buffer_ptr);
    bs_gfxbuf_clear(gfxbuf_ptr, 0xff4080c0);

    wl_surface_damage_buffer(
        toplevel_ptr->wl_surface_ptr, 0, 0, INT32_MAX, INT32_MAX);
    wlclient_buffer_attach_to_surface_and_commit(
        buffer_ptr,
        toplevel_ptr->wl_surface_ptr);
}

/* == End of xdg_toplevel.c ================================================== */
