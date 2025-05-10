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

#include <libbase/libbase.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include "dblbuf.h"
#include "xdg-shell-client-protocol.h"

struct xdg_surface;

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

    /** The double-buffer wrapper for the surface. */
    wlcl_dblbuf_t             *dblbuf_ptr;

    /** Whether the surface had been configured. Can only use after that. */
    bool                      configured;
    /** Callback for when the buffer is ready to draw into. */
    wlcl_dblbuf_ready_callback_t callback;
    /** Client-provied argument to @ref wlclient_xdg_toplevel_t::callback. */
    void                      *callback_ud_ptr;
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
    wlclient_t *wlclient_ptr,
    unsigned width, unsigned height)
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

    toplevel_ptr->dblbuf_ptr = wlcl_dblbuf_create(
        toplevel_ptr->wl_surface_ptr,
        wlclient_attributes(wlclient_ptr)->wl_shm_ptr,
        width,
        height);
    if (NULL == toplevel_ptr->dblbuf_ptr) {
        bs_log(BS_ERROR, "Failed wlcl_dblbuf_create(%p, %p, %u, %u)",
        toplevel_ptr->wl_surface_ptr,
        wlclient_attributes(wlclient_ptr)->wl_shm_ptr,
        width,
        height);
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
               toplevel_ptr->xdg_surface_ptr);
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    wl_surface_commit(toplevel_ptr->wl_surface_ptr);
    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_xdg_toplevel_destroy(wlclient_xdg_toplevel_t *toplevel_ptr)
{
    if (NULL != toplevel_ptr->dblbuf_ptr) {
        wlcl_dblbuf_destroy(toplevel_ptr->dblbuf_ptr);
        toplevel_ptr->dblbuf_ptr = NULL;
    }

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

/* ------------------------------------------------------------------------- */
void wlclient_xdg_toplevel_register_ready_callback(
    wlclient_xdg_toplevel_t *toplevel_ptr,
    bool (*callback)(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr),
    void *callback_ud_ptr)
{
    if (toplevel_ptr->configured) {
        wlcl_dblbuf_register_ready_callback(
            toplevel_ptr->dblbuf_ptr,
            callback,
            callback_ud_ptr);
        return;
    }

    toplevel_ptr->callback = callback;
    toplevel_ptr->callback_ud_ptr = callback_ud_ptr;
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
    __UNUSED__ wlclient_xdg_toplevel_t *toplevel_ptr = data_ptr;
    bs_log(BS_ERROR, "Configure + ACK %"PRIu32, serial);
    xdg_surface_ack_configure(xdg_surface_ptr, serial);

    toplevel_ptr->configured = true;
    if (NULL != toplevel_ptr->callback) {
        wlcl_dblbuf_register_ready_callback(
            toplevel_ptr->dblbuf_ptr,
            toplevel_ptr->callback,
            toplevel_ptr->callback_ud_ptr);
        toplevel_ptr->callback = NULL;
    }
}

/* == End of xdg_toplevel.c ================================================== */
