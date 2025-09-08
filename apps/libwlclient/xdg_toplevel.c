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

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include "dblbuf.h"
#include "ext-input-observation-v1-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct ext_input_position_observer_v1;
struct wl_array;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct zxdg_toplevel_decoration_v1;

/* == Declarations ========================================================= */

/** State of the XDG toplevel. */
struct _wlclient_xdg_toplevel_t {
    /** Back-link to the client. */
    wlclient_t                *wlclient_ptr;

    /** Window title of the toplevel. */
    char                      *title_ptr;

    /** Surface. */
    struct wl_surface         *wl_surface_ptr;
    /** Wrapped as XDG surface. */
    struct xdg_surface        *xdg_surface_ptr;
    /** The XDG toplevel. */
    struct xdg_toplevel       *xdg_toplevel_ptr;

    /** The XDG toplevel'ss decoration handle. */
    struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration_v1_ptr;
    /** Whether to request decoration on the server side. */
    bool                      decorate_server_side;

    /** The double-buffer wrapper for the surface. */
    wlcl_dblbuf_t             *dblbuf_ptr;

    /** Whether the surface had been configured. Can only use after that. */
    bool                      configured;
    /** Whether the decoration has gotten configured. */
    bool                      decoration_configured;
    /** Callback for when the buffer is ready to draw into. */
    wlcl_dblbuf_ready_callback_t callback;
    /** Client-provided argument to @ref wlclient_xdg_toplevel_t::callback. */
    void                      *callback_ud_ptr;

    /** Callback for input position observation. */
    void (*position_callback)(double x, double y, void *ud_ptr);
    /** Client-provided argument to @ref wlclient_xdg_toplevel_t::position_callback. */
    void                      *position_callback_ud_ptr;
    /** Whether any position update had been received already. */
    bool                      position_received;
    /** Last known reported input X position. */
    int32_t                   last_position_x;
    /** Last known reported input Y position. */
    int32_t                   last_position_y;

    /** Input observer. */
    struct ext_input_position_observer_v1 *input_position_observer_ptr;
};

static void _wlclient_xdg_configure_decoration(
    wlclient_xdg_toplevel_t *toplevel_ptr);
static void _wlclient_xdg_surface_configure(
    void *data,
    struct xdg_surface *xdg_surface,
    uint32_t serial);
static void _wlc_xdg_toplevel_decoration_v1_configure(
    void *data_ptr,
    struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1_ptr,
    uint32_t mode);

static void _xdg_toplevel_handle_configure(
    void *data_ptr,
    struct xdg_toplevel *xdg_toplevel_ptr,
    int32_t width,
    int32_t height,
    struct wl_array *states);
static void _xdg_toplevel_handle_close(
    void *data_ptr,
    struct xdg_toplevel *xdg_toplevel_ptr);
static void _xdg_toplevel_handle_configure_bounds(
    void *data_ptr,
    struct xdg_toplevel *xdg_toplevel_ptr,
    int32_t width,
    int32_t height);
static void _xdg_toplevel_handle_wm_capabilities(
    void *data_ptr,
    struct xdg_toplevel *xdg_toplevel_ptr,
    struct wl_array *capabilities);

static void _wlclient_input_position_observer_position(
    void *data_ptr,
    struct ext_input_position_observer_v1 *input_position_observer_ptr,
    struct wl_surface *wl_surface_ptr,
    uint32_t instance,
    int32_t relative_x,
    int32_t relative_y);

/* == Data ================================================================= */

/** Listeners for the XDG toplevel. */
static const struct xdg_toplevel_listener _wlc_xdg_toplevel_listener = {
    .configure = _xdg_toplevel_handle_configure,
    .close = _xdg_toplevel_handle_close,
    .configure_bounds = _xdg_toplevel_handle_configure_bounds,
    .wm_capabilities = _xdg_toplevel_handle_wm_capabilities
};

/** Listeners for the XDG surface. */
static const struct xdg_surface_listener _wlclient_xdg_surface_listener = {
    .configure = _wlclient_xdg_surface_configure,
};

/** Listeners for the XDG decoration manager. */
static const struct zxdg_toplevel_decoration_v1_listener
_wlc_xdg_toplevel_decoration_v1_listener = {
    .configure = _wlc_xdg_toplevel_decoration_v1_configure,
};

/** Listeners for the Pointer position Tracker. */
static const struct ext_input_position_observer_v1_listener
_wlclient_tracker_listener = {
    .position = _wlclient_input_position_observer_position,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_xdg_toplevel_t *wlclient_xdg_toplevel_create(
    wlclient_t *wlclient_ptr,
    const char *title_ptr,
    unsigned width,
    unsigned height)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = logged_calloc(
        1, sizeof(wlclient_xdg_toplevel_t));
    if (NULL == toplevel_ptr) return NULL;
    toplevel_ptr->wlclient_ptr = wlclient_ptr;
    toplevel_ptr->title_ptr = logged_strdup(title_ptr);
    if (NULL == toplevel_ptr->title_ptr) {
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    toplevel_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
    if (NULL == toplevel_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               wlclient_attributes(wlclient_ptr)->wl_compositor_ptr);
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    toplevel_ptr->dblbuf_ptr = wlcl_dblbuf_create(
        wlclient_attributes(wlclient_ptr)->app_id_ptr,
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

    xdg_surface_set_window_geometry(
        toplevel_ptr->xdg_surface_ptr, 0, 0, width, height);

    if (0 != xdg_toplevel_add_listener(
            toplevel_ptr->xdg_toplevel_ptr,
            &_wlc_xdg_toplevel_listener,
            toplevel_ptr)) {
        wlclient_xdg_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    if (NULL != wlclient_attributes(wlclient_ptr)->xdg_decoration_manager_ptr) {
        toplevel_ptr->xdg_toplevel_decoration_v1_ptr =
            zxdg_decoration_manager_v1_get_toplevel_decoration(
                wlclient_attributes(wlclient_ptr)->xdg_decoration_manager_ptr,
                toplevel_ptr->xdg_toplevel_ptr);
        if (NULL == toplevel_ptr->xdg_toplevel_decoration_v1_ptr) {
            bs_log(BS_ERROR, "Failed "
                   "zxdg_decoration_manager_v1_get_toplevel_decoration()");
            wlclient_xdg_toplevel_destroy(toplevel_ptr);
            return NULL;
        }

        if (0 != zxdg_toplevel_decoration_v1_add_listener(
                toplevel_ptr->xdg_toplevel_decoration_v1_ptr,
                &_wlc_xdg_toplevel_decoration_v1_listener,
                toplevel_ptr)) {
            bs_log(BS_ERROR, "Failed zxdg_toplevel_decoration_v1_add_listener");
            wlclient_xdg_toplevel_destroy(toplevel_ptr);
            return NULL;
        }
    }

    xdg_toplevel_set_title(toplevel_ptr->xdg_toplevel_ptr,
                           toplevel_ptr->title_ptr);
    if (NULL != wlclient_attributes(wlclient_ptr)->app_id_ptr) {
        xdg_toplevel_set_app_id(
            toplevel_ptr->xdg_toplevel_ptr,
            wlclient_attributes(wlclient_ptr)->app_id_ptr);
    }

    if (NULL != wlclient_attributes(wlclient_ptr)->input_observation_manager_ptr) {
        toplevel_ptr->input_position_observer_ptr =
            ext_input_observation_manager_v1_create_pointer_observer(
                wlclient_attributes(wlclient_ptr)->input_observation_manager_ptr,
                wlclient_attributes(wlclient_ptr)->wl_pointer_ptr,
                toplevel_ptr->wl_surface_ptr);
        if (NULL == toplevel_ptr->input_position_observer_ptr) {
            bs_log(BS_ERROR,
                   "Failed ext_input_observation_v1_pointer_position(%p, %p)",
                   wlclient_attributes(wlclient_ptr)->input_observation_manager_ptr,
                   toplevel_ptr->wl_surface_ptr);
            wlclient_xdg_toplevel_destroy(toplevel_ptr);
            return NULL;
        }
        ext_input_position_observer_v1_add_listener(
            toplevel_ptr->input_position_observer_ptr,
            &_wlclient_tracker_listener,
            toplevel_ptr);
        bs_log(BS_INFO, "Created pointer tracker %p for wl_surface %p",
               toplevel_ptr->input_position_observer_ptr, toplevel_ptr->wl_surface_ptr);
    }

    wl_surface_commit(toplevel_ptr->wl_surface_ptr);
    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_xdg_toplevel_destroy(wlclient_xdg_toplevel_t *toplevel_ptr)
{
    if (NULL != toplevel_ptr->xdg_toplevel_decoration_v1_ptr) {
        zxdg_toplevel_decoration_v1_destroy(
            toplevel_ptr->xdg_toplevel_decoration_v1_ptr);
        toplevel_ptr->xdg_toplevel_decoration_v1_ptr = NULL;
    }

    if (NULL != toplevel_ptr->xdg_toplevel_ptr) {
        xdg_toplevel_destroy(toplevel_ptr->xdg_toplevel_ptr);
        toplevel_ptr->xdg_toplevel_ptr = NULL;
    }

    if (NULL != toplevel_ptr->dblbuf_ptr) {
        wlcl_dblbuf_destroy(toplevel_ptr->dblbuf_ptr);
        toplevel_ptr->dblbuf_ptr = NULL;
    }

    if (NULL != toplevel_ptr->input_position_observer_ptr) {
        ext_input_position_observer_v1_destroy(
            toplevel_ptr->input_position_observer_ptr);
        toplevel_ptr->input_position_observer_ptr = NULL;
    }

    if (NULL != toplevel_ptr->wl_surface_ptr) {
        wl_surface_destroy(toplevel_ptr->wl_surface_ptr);
        toplevel_ptr->wl_surface_ptr = NULL;
    }

    if (NULL != toplevel_ptr->title_ptr) {
        free(toplevel_ptr->title_ptr);
        toplevel_ptr->title_ptr = NULL;
    }

    free(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlclient_xdg_supported(wlclient_t *wlclient_ptr)
{
    return (NULL != wlclient_attributes(wlclient_ptr)->xdg_wm_base_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlclient_xdg_decoration_set_server_side(
    wlclient_xdg_toplevel_t *toplevel_ptr,
    bool enabled)
{
    // Guard clause.
    if (NULL == toplevel_ptr->xdg_toplevel_decoration_v1_ptr) return false;

    // Nothing to do.
    if (toplevel_ptr->decorate_server_side == enabled) return true;

    toplevel_ptr->decoration_configured = false;
    toplevel_ptr->decorate_server_side = enabled;
    _wlclient_xdg_configure_decoration(toplevel_ptr);
    return true;
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

/* ------------------------------------------------------------------------- */
void wlclient_xdg_toplevel_register_position_callback(
    wlclient_xdg_toplevel_t *toplevel_ptr,
    void (*callback)(double x, double y, void *ud_ptr),
    void *callback_ud_ptr)
{
    if (toplevel_ptr->position_received) {
        callback(
            toplevel_ptr->last_position_x / 256.0,
            toplevel_ptr->last_position_y / 256.0,
            callback_ud_ptr);
    }

    toplevel_ptr->position_callback = callback;
    toplevel_ptr->position_callback_ud_ptr = callback_ud_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Updates the server-side decoration mode. */
void _wlclient_xdg_configure_decoration(wlclient_xdg_toplevel_t *toplevel_ptr)
{
    // Guard clauses.
    if (NULL == toplevel_ptr->xdg_toplevel_decoration_v1_ptr) return;
    if (toplevel_ptr->decoration_configured) return;

    zxdg_toplevel_decoration_v1_set_mode(
        toplevel_ptr->xdg_toplevel_decoration_v1_ptr,
        (toplevel_ptr->decorate_server_side ?
         ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
         ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE));
    toplevel_ptr->decoration_configured = true;
}

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

    _wlclient_xdg_configure_decoration(toplevel_ptr);

    toplevel_ptr->configured = true;
    if (NULL != toplevel_ptr->callback) {
        wlcl_dblbuf_register_ready_callback(
            toplevel_ptr->dblbuf_ptr,
            toplevel_ptr->callback,
            toplevel_ptr->callback_ud_ptr);
        toplevel_ptr->callback = NULL;
    }

}

/* ------------------------------------------------------------------------- */
/** Handles the decoration mode change listener. */
void _wlc_xdg_toplevel_decoration_v1_configure(
    void *data_ptr,
    __UNUSED__ struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1_ptr,
    uint32_t mode)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = data_ptr;

    static const char* decoration_modes[3] = {
        [0] = "(unknown)",
        [ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE] = (
            "ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE"),
        [ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE] = (
            "ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE")
    };

    if (mode >= sizeof(decoration_modes) / sizeof(char*)) mode = 0;
    bs_log(BS_INFO, "XDG toplevel %p configured decoration mode %s",
           toplevel_ptr->xdg_toplevel_ptr,
           decoration_modes[mode]);

    toplevel_ptr->decoration_configured = false;
    _wlclient_xdg_configure_decoration(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles XDG toplevel's configure event. */
void _xdg_toplevel_handle_configure(
    __UNUSED__ void *data_ptr,
    __UNUSED__ struct xdg_toplevel *xdg_toplevel_ptr,
    __UNUSED__ int32_t width,
    __UNUSED__ int32_t height,
    __UNUSED__ struct wl_array *states)
{
    // Currently unused.
}

/* ------------------------------------------------------------------------- */
/** Handles the action of the 'close' button. */
void _xdg_toplevel_handle_close(
    void *data_ptr,
    __UNUSED__ struct xdg_toplevel *xdg_toplevel_ptr)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = data_ptr;
    wlclient_request_terminate(toplevel_ptr->wlclient_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles 'configure_bounds' of the toplevel. */
void _xdg_toplevel_handle_configure_bounds(
    __UNUSED__ void *data_ptr,
    __UNUSED__ struct xdg_toplevel *xdg_toplevel_ptr,
    __UNUSED__ int32_t width,
    __UNUSED__ int32_t height)
{
    // Currently unused.
}

/* ------------------------------------------------------------------------- */
/** Handles the 'wm_capabilities' request. */
void _xdg_toplevel_handle_wm_capabilities(
    __UNUSED__ void *data_ptr,
    __UNUSED__ struct xdg_toplevel *xdg_toplevel_ptr,
    __UNUSED__ struct wl_array *capabilities)
{
    // Currently unused.
}

/* ------------------------------------------------------------------------- */
/** Callback for when a `position` event is received. */
void _wlclient_input_position_observer_position(
    void *data_ptr,
    __UNUSED__ struct ext_input_position_observer_v1 *input_position_observer_ptr,
    __UNUSED__ struct wl_surface *wl_surface_ptr,
    __UNUSED__ uint32_t instance,
    int32_t relative_x,
    int32_t relative_y)
{
    wlclient_xdg_toplevel_t *toplevel_ptr = data_ptr;

    if (!toplevel_ptr->position_received ||
        toplevel_ptr->last_position_x != relative_x ||
        toplevel_ptr->last_position_y != relative_y) {

        toplevel_ptr->position_received = true;
        toplevel_ptr->last_position_x = relative_x;
        toplevel_ptr->last_position_y = relative_y;

        if (NULL != toplevel_ptr->position_callback) {
            toplevel_ptr->position_callback(
                toplevel_ptr->last_position_x / 256.0,
                toplevel_ptr->last_position_y / 256.0,
                toplevel_ptr->position_callback_ud_ptr);
        }
    }
}

/* == End of xdg_toplevel.c ================================================== */
