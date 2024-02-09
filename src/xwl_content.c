/* ========================================================================= */
/**
 * @file xwl_content.c
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

#include "xwl_content.h"

#include <libbase/libbase.h>

#include "xwl_popup.h"
#include "xwl_toplevel.h"
#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/xwayland.h>
#include <wlr/types/wlr_compositor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the XWayland window content. */
struct _wlmaker_xwl_content_t {
    /** Toolkit content state. */
    wlmtk_content_t           content;

    /** Corresponding wlroots XWayland surface. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
    /** Back-link to the XWayland server. */
    wlmaker_xwl_t             *xwl_ptr;
    /** wlmtk environment. */
    wlmtk_env_t               *env_ptr;

    /** A fake configure serial, tracked here. */
    uint32_t                  serial;

    /** Listener for the `destroy` signal of `wlr_xwayland_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_configure` signal of `wlr_xwayland_surface`. */
    struct wl_listener        request_configure_listener;

    /** Listener for the `associate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        associate_listener;
    /** Listener for the `dissociate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        dissociate_listener;

    /** Listener for the `set_title` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_title_listener;
    /** Listener for the `set_parent` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_parent_listener;
    /** Listener for the `set_decorations` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_decorations_listener;
    /** Listener for the `set_geometry` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_geometry_listener;

    /** The toolkit surface. Only available once 'associated'. */
    wlmtk_surface_t           *surface_ptr;

    /** The XWayland toplevel window, in case this content has no parent. */
    wlmaker_xwl_toplevel_t    *xwl_toplevel_ptr;
    /** The XWayland popup, in case this content has a parent. */
    wlmaker_xwl_popup_t       *xwl_popup_ptr;

    /** Listener for `surface_commit` of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;
};

static void _xwl_content_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_content_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_content_handle_associate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_content_handle_dissociate(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_content_handle_set_title(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_content_handle_set_parent(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_content_handle_set_decorations(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_content_handle_set_geometry(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_content_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static uint32_t _xwl_content_content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized);
static uint32_t _xwl_content_content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen);
static uint32_t _xwl_content_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void _xwl_content_content_request_close(
    wlmtk_content_t *content_ptr);
static void _xwl_content_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

static void _xwl_content_apply_decorations(
    wlmaker_xwl_content_t *xwl_content_ptr);

/* == Data ================================================================= */

/** Virtual methods for XDG toplevel surface, for the Content superclass. */
static const wlmtk_content_vmt_t _xwl_content_content_vmt = {
    .request_maximized = _xwl_content_content_request_maximized,
    .request_fullscreen = _xwl_content_content_request_fullscreen,
    .request_size = _xwl_content_content_request_size,
    .request_close = _xwl_content_content_request_close,
    .set_activated = _xwl_content_content_set_activated,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_content_t *wlmaker_xwl_content_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_xwl_t *xwl_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_content_t));
    if (NULL == xwl_content_ptr) return NULL;
    xwl_content_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;
    wlr_xwayland_surface_ptr->data = xwl_content_ptr;
    xwl_content_ptr->xwl_ptr = xwl_ptr;
    xwl_content_ptr->env_ptr = server_ptr->env_ptr;
    xwl_content_ptr->server_ptr = server_ptr;

    if (!wlmtk_content_init(&xwl_content_ptr->content,
                            NULL,
                            xwl_content_ptr->env_ptr)) {
        wlmaker_xwl_content_destroy(xwl_content_ptr);
        return NULL;
    }
    wlmtk_content_extend(&xwl_content_ptr->content, &_xwl_content_content_vmt);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.destroy,
        &xwl_content_ptr->destroy_listener,
        _xwl_content_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.request_configure,
        &xwl_content_ptr->request_configure_listener,
        _xwl_content_handle_request_configure);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.associate,
        &xwl_content_ptr->associate_listener,
        _xwl_content_handle_associate);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.dissociate,
        &xwl_content_ptr->dissociate_listener,
        _xwl_content_handle_dissociate);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_title,
        &xwl_content_ptr->set_title_listener,
        _xwl_content_handle_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_parent,
        &xwl_content_ptr->set_parent_listener,
        _xwl_content_handle_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_decorations,
        &xwl_content_ptr->set_decorations_listener,
        _xwl_content_handle_set_decorations);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_geometry,
        &xwl_content_ptr->set_geometry_listener,
        _xwl_content_handle_set_geometry);

    bs_log(BS_INFO, "Created XWL content %p for wlr_xwayland_surface %p",
           xwl_content_ptr, wlr_xwayland_surface_ptr);

    return xwl_content_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_content_destroy(wlmaker_xwl_content_t *xwl_content_ptr)
{
    bs_log(BS_INFO, "Destroy XWL content %p", xwl_content_ptr);

    if (NULL != wlmtk_content_get_parent_content(&xwl_content_ptr->content)) {
        wlmtk_content_remove_popup(
            wlmtk_content_get_parent_content(&xwl_content_ptr->content),
            &xwl_content_ptr->content);
    }

    wl_list_remove(&xwl_content_ptr->set_geometry_listener.link);
    wl_list_remove(&xwl_content_ptr->set_decorations_listener.link);
    wl_list_remove(&xwl_content_ptr->set_parent_listener.link);
    wl_list_remove(&xwl_content_ptr->set_title_listener.link);
    wl_list_remove(&xwl_content_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_content_ptr->associate_listener.link);
    wl_list_remove(&xwl_content_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_content_ptr->destroy_listener.link);

    if (NULL != xwl_content_ptr->xwl_toplevel_ptr) {
        wlmaker_xwl_toplevel_destroy(xwl_content_ptr->xwl_toplevel_ptr);
        xwl_content_ptr->xwl_toplevel_ptr = NULL;
    }
    if (NULL != xwl_content_ptr->xwl_popup_ptr) {
        wlmaker_xwl_popup_destroy(xwl_content_ptr->xwl_popup_ptr);
        xwl_content_ptr->xwl_popup_ptr = NULL;
    }

    wlmtk_content_fini(&xwl_content_ptr->content);
    if (NULL != xwl_content_ptr->wlr_xwayland_surface_ptr) {
        xwl_content_ptr->wlr_xwayland_surface_ptr->data = NULL;
    }
    free(xwl_content_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_content_t *wlmtk_content_from_xwl_content(
    wlmaker_xwl_content_t *xwl_content_ptr)
{
    return &xwl_content_ptr->content;
}

/* ------------------------------------------------------------------------- */
wlmtk_surface_t *wlmtk_surface_from_xwl_content(
    wlmaker_xwl_content_t *xwl_content_ptr)
{
    return xwl_content_ptr->surface_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, destroy_listener);
    wlmaker_xwl_content_destroy(xwl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_configure` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, request_configure_listener);
    struct wlr_xwayland_surface_configure_event *cfg_event_ptr = data_ptr;

    bs_log(BS_INFO, "Request configure for %p: "
           "%"PRId16" x %"PRId16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_content_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);

    // FIXME:
    // -> if we have content/surface: check what that means, with respect to
    //    the surface::commit handler.

    // It appears this needs to be ACKed with a surface_configure.
    wlr_xwayland_surface_configure(
        xwl_content_ptr->wlr_xwayland_surface_ptr,
        cfg_event_ptr->x, cfg_event_ptr->y,
        cfg_event_ptr->width, cfg_event_ptr->height);
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
void _xwl_content_handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, associate_listener);
    bs_log(BS_INFO, "Associate XWL content %p with wlr_surface %p at %d, %d",
           xwl_content_ptr, xwl_content_ptr->wlr_xwayland_surface_ptr->surface,
           xwl_content_ptr->wlr_xwayland_surface_ptr->x,
           xwl_content_ptr->wlr_xwayland_surface_ptr->y);

    BS_ASSERT(NULL == xwl_content_ptr->surface_ptr);
    xwl_content_ptr->surface_ptr = wlmtk_surface_create(
        xwl_content_ptr->wlr_xwayland_surface_ptr->surface,
        xwl_content_ptr->env_ptr);
    if (NULL == xwl_content_ptr->surface_ptr) {
        // TODO(kaeser@gubbe.ch): Relay error to client, instead of crash.
        bs_log(BS_FATAL, "Failed wlmtk_surface_create.");
        return;
    }
    wlmtk_content_set_surface(
        &xwl_content_ptr->content,
        xwl_content_ptr->surface_ptr);
    wlmtk_surface_connect_commit_listener_signal(
        xwl_content_ptr->surface_ptr,
        &xwl_content_ptr->surface_commit_listener,
        _xwl_content_handle_surface_commit);

    if (NULL != xwl_content_ptr->wlr_xwayland_surface_ptr->parent) {
        BS_ASSERT(NULL == xwl_content_ptr->xwl_popup_ptr);
        xwl_content_ptr->xwl_popup_ptr = wlmaker_xwl_popup_create(
            xwl_content_ptr);

        wlmtk_element_set_visible(
            wlmtk_content_element(wlmtk_content_from_xwl_content(xwl_content_ptr)),
            true);

        // FIXME: Well... yeah. In that case, we'll want to create a popup
        // and add it to the parent. It may already have been added to the
        // parent, though?
        return;
    }

    xwl_content_ptr->xwl_toplevel_ptr = wlmaker_xwl_toplevel_create(
        xwl_content_ptr,
        xwl_content_ptr->server_ptr,
        xwl_content_ptr->env_ptr);
    if (NULL == xwl_content_ptr->xwl_toplevel_ptr) {
        // TODO(kaeser@gubbe.ch): Relay error to client, instead of crash.
        bs_log(BS_FATAL, "Failed wlmaker_xwl_toplevel_create.");
        return;
    }

    _xwl_content_apply_decorations(xwl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_dissociate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, dissociate_listener);

    if (NULL != xwl_content_ptr->xwl_toplevel_ptr) {
        wlmaker_xwl_toplevel_destroy(xwl_content_ptr->xwl_toplevel_ptr);
        xwl_content_ptr->xwl_toplevel_ptr = NULL;
    }
    if (NULL != xwl_content_ptr->xwl_popup_ptr) {
        wlmaker_xwl_popup_destroy(xwl_content_ptr->xwl_popup_ptr);
        xwl_content_ptr->xwl_popup_ptr = NULL;
    }

    wl_list_remove(&xwl_content_ptr->surface_commit_listener.link);
    wlmtk_content_set_surface(&xwl_content_ptr->content, NULL);
    if (NULL != xwl_content_ptr->surface_ptr) {
        wlmtk_surface_destroy(xwl_content_ptr->surface_ptr);
        xwl_content_ptr->surface_ptr = NULL;
    }

    bs_log(BS_INFO, "Dissociate XWL content %p from wlr_surface %p",
           xwl_content_ptr, xwl_content_ptr->wlr_xwayland_surface_ptr->surface);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_title` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_set_title(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, set_title_listener);

    if (false) {
        wlmtk_window_set_title(
            NULL,  // FIXME; xwl_content_ptr->window_ptr,
            xwl_content_ptr->wlr_xwayland_surface_ptr->title);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_parent` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_set_parent(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, set_parent_listener);
    wlmtk_content_t *content_ptr = wlmtk_content_from_xwl_content(
        xwl_content_ptr);

    BS_ASSERT(NULL != xwl_content_ptr->wlr_xwayland_surface_ptr->parent);
    wlmaker_xwl_content_t *parent_xwl_content_ptr =
        xwl_content_ptr->wlr_xwayland_surface_ptr->parent->data;
    wlmtk_content_t *parent_content_ptr = wlmtk_content_from_xwl_content(
        parent_xwl_content_ptr);

    // The parent didn't change? Return right away.
    if (parent_content_ptr == wlmtk_content_get_parent_content(content_ptr)) {
        return;
    }

    // There already is a parent, and it does change: Un-parent first.
    if (NULL != wlmtk_content_get_parent_content(content_ptr)) {
        wlmtk_content_remove_popup(
            wlmtk_content_get_parent_content(content_ptr),
            content_ptr);
    }

    wlmtk_content_add_popup(parent_content_ptr, content_ptr);
    bs_log(BS_INFO, "Set parent for XWL content %p to XWL content %p",
           xwl_content_ptr, parent_xwl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_decorations` event of `struct wlr_xwayland_surface`.
 *
 * Applies server-side decoration, if the X11 window is supposed to have
 * decorations.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_set_decorations(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, set_decorations_listener);
    _xwl_content_apply_decorations(xwl_content_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_geometry` event of `struct wlr_xwayland_surface`.
 *
 * Called from wlroots/xwayland/xwm.c, whenever the geometry (position or
 * dimensions) of the window (precisely: the xwayland_surface) changes.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_content_handle_set_geometry(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, set_geometry_listener);

    // FIXME: we'll have x, y, width and height holding current values.
    bs_log(BS_WARNING, "FIXME: geo %d, %d for %d x %d",
           xwl_content_ptr->wlr_xwayland_surface_ptr->x,
           xwl_content_ptr->wlr_xwayland_surface_ptr->y,
           xwl_content_ptr->wlr_xwayland_surface_ptr->width,
           xwl_content_ptr->wlr_xwayland_surface_ptr->height);

    // Tricky: the position is in absolute terms to the "root" window, ie.
    // to the window.
    wlmtk_element_set_position(
        wlmtk_content_element(&xwl_content_ptr->content),
        xwl_content_ptr->wlr_xwayland_surface_ptr->x,
        xwl_content_ptr->wlr_xwayland_surface_ptr->y);
}

/* ------------------------------------------------------------------------- */
/** Surface commit handler: Pass on the current serial. */
void _xwl_content_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_content_t, surface_commit_listener);

    bs_log(BS_DEBUG, "XWL content %p commit surface %p, current %d x %d",
           xwl_content_ptr, xwl_content_ptr->wlr_xwayland_surface_ptr->surface,
           xwl_content_ptr->wlr_xwayland_surface_ptr->surface->current.width,
           xwl_content_ptr->wlr_xwayland_surface_ptr->surface->current.height);

    wlmtk_content_commit_serial(
        &xwl_content_ptr->content,
        xwl_content_ptr->serial);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_maximized. */
uint32_t _xwl_content_content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized)
{
    __UNUSED__ wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_content_t, content);

    if (false) {
        wlmtk_window_commit_maximized(
            NULL,  // FIXME: xwl_content_ptr->window_ptr,
            maximized);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_fullscreen. */
uint32_t _xwl_content_content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen)
{
    __UNUSED__ wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_content_t, content);

    if (false) {
        wlmtk_window_commit_fullscreen(
            NULL, // FIXME: xwl_content_ptr->window_ptr,
            fullscreen);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_size. */
uint32_t _xwl_content_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_content_t, content);
    wlr_xwayland_surface_configure(
        xwl_content_ptr->wlr_xwayland_surface_ptr,
        0, 0, width, height);
    return xwl_content_ptr->serial++;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_close. */
static void _xwl_content_content_request_close(
    wlmtk_content_t *content_ptr)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_content_t, content);
    wlr_xwayland_surface_close(xwl_content_ptr->wlr_xwayland_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::set_activated. */
void _xwl_content_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmaker_xwl_content_t *xwl_content_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_content_t, content);

    wlr_xwayland_surface_activate(
        xwl_content_ptr->wlr_xwayland_surface_ptr, activated);
    wlmtk_surface_set_activated(xwl_content_ptr->surface_ptr, activated);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets whether this window should be server-side-decorated.
 *
 * TODO(kaeser@gubbe.ch): Move this into xwl_toplevel.
 *
 * @param xwl_content_ptr
 */
void _xwl_content_apply_decorations(wlmaker_xwl_content_t *xwl_content_ptr)
{
    static const xwl_atom_identifier_t borderless_window_types[] = {
        NET_WM_WINDOW_TYPE_TOOLTIP, XWL_MAX_ATOM_ID};

    if (NULL == xwl_content_ptr->xwl_toplevel_ptr) return;

    // TODO(kaeser@gubbe.ch): Adapt whether NO_BORDER or NO_TITLE was set.
    if (xwl_content_ptr->wlr_xwayland_surface_ptr->decorations ==
        WLR_XWAYLAND_SURFACE_DECORATIONS_ALL &&
        !xwl_is_window_type(
            xwl_content_ptr->xwl_ptr,
            xwl_content_ptr->wlr_xwayland_surface_ptr,
            borderless_window_types)) {
        wlmaker_xwl_toplevel_set_decorations(
            xwl_content_ptr->xwl_toplevel_ptr,
            true);
    } else {
        wlmaker_xwl_toplevel_set_decorations(
            xwl_content_ptr->xwl_toplevel_ptr,
            false);
    }
}

/* == End of xwl_content.c ================================================= */
