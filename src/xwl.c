/* ========================================================================= */
/**
 * @file xwl.c
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "xwl.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/xwayland.h>
#undef WLR_USE_UNSTABLE

#include "util.h"

/* == Declarations ========================================================= */

/** XWayland interface state. */
struct _wlmaker_xwl_t {
    /** XWayland server and XWM. */
    struct wlr_xwayland       *wlr_xwayland_ptr;

    /** Listener for the `new_surface` signal raised by `wlr_xwayland`. */
    struct wl_listener        new_surface_listener;

};

static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_t *wlmaker_xwl_create(wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_t *xwl_ptr = logged_calloc(1, sizeof(wlmaker_xwl_t));
    if (NULL == xwl_ptr) return NULL;

    xwl_ptr->wlr_xwayland_ptr = wlr_xwayland_create(
        server_ptr->wl_display_ptr,
        server_ptr->wlr_compositor_ptr,
        false);
    if (NULL == xwl_ptr->wlr_xwayland_ptr) {
        bs_log(BS_ERROR, "Failed wlr_xwayland_create(%p, %p, false).",
               server_ptr->wl_display_ptr,
               server_ptr->wlr_compositor_ptr);
        wlmaker_xwl_destroy(xwl_ptr);
        return NULL;
    }

    wlm_util_connect_listener_signal(
        &xwl_ptr->wlr_xwayland_ptr->events.new_surface,
        &xwl_ptr->new_surface_listener,
        handle_new_surface);

    return xwl_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_destroy(wlmaker_xwl_t *xwl_ptr)
{
    if (NULL != xwl_ptr->wlr_xwayland_ptr) {
        wlr_xwayland_destroy(xwl_ptr->wlr_xwayland_ptr);
        xwl_ptr->wlr_xwayland_ptr = NULL;
    }

    free(xwl_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_surface` signal raised by `wlr_xwayland`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_surface(struct wl_listener *listener_ptr,
                        void *data_ptr)
{
    __UNUSED__ struct wlr_xwayland_surface *wlr_xwayland_surface_ptr = data_ptr;
    __UNUSED__ wlmaker_xwl_t *xwl_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_t, new_surface_listener);

    // TODO(kaeser@gubbe.ch): Implement the handlers.
}

/* == End of xwl.c ========================================================= */
