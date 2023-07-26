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

/** Implementation of an Xwayland user interface component. */
typedef struct {

    /** Link to the corresponding `struct wlr_xwayland_surface`. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    /** Back-link to the interface. */
    wlmaker_xwl_t             *xwl_ptr;

    /** Listener for the `destroy` signal of `wlr_xwayland_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_configure` signal of `wlr_xwayland_surface`. */
    struct wl_listener        request_configure_listener;

    /** Listener for the `associate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        associate_listener;
    /** Listener for the `dissociate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        dissociate_listener;
} wlmaker_xwl_surface_t;

static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_xwl_surface_t *xwl_surface_create(
    wlmaker_xwl_t *xwl_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr);
static void xwl_surface_destroy(
    wlmaker_xwl_surface_t *xwl_surface_ptr);

static void handle_destroy(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_associate(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_dissociate(struct wl_listener *listener_ptr, void *data_ptr);

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

    wlmaker_xwl_surface_t *xwl_surface_ptr = xwl_surface_create(
        xwl_ptr, wlr_xwayland_surface_ptr);
    if (NULL == xwl_surface_ptr) {
        bs_log(BS_ERROR, "Failed xwl_surface_create(%p, %p)",
               xwl_ptr, wlr_xwayland_surface_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates an XWL surface.
 *
 * @param xwl_ptr
 * @param wlr_xwayland_surface_ptr
 *
 * @return The XWL surface or NULL on error.
 */
wlmaker_xwl_surface_t *xwl_surface_create(
    wlmaker_xwl_t *xwl_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_surface_t));
    if (NULL == xwl_surface_ptr) return NULL;
    xwl_surface_ptr->xwl_ptr = xwl_ptr;
    xwl_surface_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;

    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.destroy,
        &xwl_surface_ptr->destroy_listener,
        handle_destroy);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.request_configure,
        &xwl_surface_ptr->request_configure_listener,
        handle_request_configure);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.associate,
        &xwl_surface_ptr->associate_listener,
        handle_associate);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.dissociate,
        &xwl_surface_ptr->dissociate_listener,
        handle_dissociate);

    bs_log(BS_INFO, "Created XWayland surface %p, wlr xwayland surface %p",
           xwl_surface_ptr, wlr_xwayland_surface_ptr);
    return xwl_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the XWL surface.
 *
 * @param xwl_surface_ptr
 */
void xwl_surface_destroy(wlmaker_xwl_surface_t *xwl_surface_ptr)
{
    bs_log(BS_INFO, "Destroying XWayland surface %p", xwl_surface_ptr);

    wl_list_remove(&xwl_surface_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_surface_ptr->associate_listener.link);
    wl_list_remove(&xwl_surface_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_surface_ptr->destroy_listener.link);

    free(xwl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, destroy_listener);

    xwl_surface_destroy(xwl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_configure` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, request_configure_listener);
    struct wlr_xwayland_surface_configure_event *cfg_event_ptr = data_ptr;

    bs_log(BS_WARNING, "Not implemented for %p: configure - "
           "%"PRId16" x %"PRIx16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_surface_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `associate` event of `struct wlr_xwayland_surface`.
 *
 * The `associate` event is triggered once an X11 window becomes associated
 * with the surface. Understanding this is a moment the surface can be mapped.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_associate(struct wl_listener *listener_ptr, void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, associate_listener);

    bs_log(BS_WARNING, "Not implemented: associate %p data %p",
           xwl_surface_ptr, data_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_dissociate(struct wl_listener *listener_ptr, void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, dissociate_listener);

    bs_log(BS_WARNING, "Not implemented: dissociate %p data %p",
           xwl_surface_ptr, data_ptr);
}

/* == End of xwl.c ========================================================= */
