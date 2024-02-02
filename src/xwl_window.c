/* ========================================================================= */
/**
 * @file xwl_window.c
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

#include "xwl_window.h"

#include <libbase/libbase.h>

#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/xwayland.h>
#include <wlr/types/wlr_compositor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the XWayland window content. */
struct _wlmaker_xwl_window_t {
    /** Corresponding wlroots XWayland surface. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
    /** Back-link to the XWayland server. */
    wlmaker_xwl_t             *xwl_ptr;
    /** wlmtk environment. */
    wlmtk_env_t               *env_ptr;

    /** Toolkit content state. */
    wlmtk_content_t           content;
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
    /** Listener for the `set_decorations` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_decorations_listener;

    /** The toolkit surface. Only available once 'associated'. */
    wlmtk_surface_t           *surface_ptr;
    /** The toolkit window. Only available once 'associated'. */
    wlmtk_window_t            *window_ptr;

    /** Listener for `surface_map` of the `wlr_surface`. */
    struct wl_listener        surface_commit_listener;
    /** Listener for `surface_map` of the `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for `surface_unmap` of the `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;
};

static void _xwl_window_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_window_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_window_handle_associate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_window_handle_dissociate(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_window_handle_set_title(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_window_handle_set_decorations(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_window_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_window_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_window_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static uint32_t _xwl_window_content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized);
static uint32_t _xwl_window_content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen);
static uint32_t _xwl_window_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height);
static void _xwl_window_content_request_close(
    wlmtk_content_t *content_ptr);
static void _xwl_window_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

static void _xwl_window_apply_decorations(wlmaker_xwl_window_t *xwl_window_ptr);

/* == Data ================================================================= */

/** Virtual methods for XDG toplevel surface, for the Content superclass. */
const wlmtk_content_vmt_t     _xwl_window_content_vmt = {
    .request_maximized = _xwl_window_content_request_maximized,
    .request_fullscreen = _xwl_window_content_request_fullscreen,
    .request_size = _xwl_window_content_request_size,
    .request_close = _xwl_window_content_request_close,
    .set_activated = _xwl_window_content_set_activated,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_window_t *wlmaker_xwl_window_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_xwl_t *xwl_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_window_t));
    if (NULL == xwl_window_ptr) return NULL;
    xwl_window_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;
    wlr_xwayland_surface_ptr->data = xwl_window_ptr;
    xwl_window_ptr->server_ptr = server_ptr;
    xwl_window_ptr->xwl_ptr = xwl_ptr;
    xwl_window_ptr->env_ptr = server_ptr->env_ptr;

    if (!wlmtk_content_init(&xwl_window_ptr->content,
                            NULL,
                            xwl_window_ptr->env_ptr)) {
        wlmaker_xwl_window_destroy(xwl_window_ptr);
        return NULL;
    }
    wlmtk_content_extend(&xwl_window_ptr->content, &_xwl_window_content_vmt);

    xwl_window_ptr->window_ptr = wlmtk_window_create(
        &xwl_window_ptr->content,
        xwl_window_ptr->env_ptr);
    if (NULL == xwl_window_ptr->window_ptr) {
        wlmaker_xwl_window_destroy(xwl_window_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.destroy,
        &xwl_window_ptr->destroy_listener,
        _xwl_window_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.request_configure,
        &xwl_window_ptr->request_configure_listener,
        _xwl_window_handle_request_configure);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.associate,
        &xwl_window_ptr->associate_listener,
        _xwl_window_handle_associate);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.dissociate,
        &xwl_window_ptr->dissociate_listener,
        _xwl_window_handle_dissociate);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_title,
        &xwl_window_ptr->set_title_listener,
        _xwl_window_handle_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_decorations,
        &xwl_window_ptr->set_decorations_listener,
        _xwl_window_handle_set_decorations);

    bs_log(BS_INFO, "Created XWL window %p for wlr_xwayland_surface %p",
           xwl_window_ptr, wlr_xwayland_surface_ptr);

    return xwl_window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_window_destroy(wlmaker_xwl_window_t *xwl_window_ptr)
{
    bs_log(BS_INFO, "Destroy XWL window %p", xwl_window_ptr);

    wl_list_remove(&xwl_window_ptr->set_decorations_listener.link);
     wl_list_remove(&xwl_window_ptr->set_title_listener.link);
   wl_list_remove(&xwl_window_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_window_ptr->associate_listener.link);
    wl_list_remove(&xwl_window_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_window_ptr->destroy_listener.link);

    if (NULL != xwl_window_ptr->window_ptr) {
        wlmtk_window_destroy(xwl_window_ptr->window_ptr);
        xwl_window_ptr->window_ptr = NULL;
    }

    wlmtk_content_fini(&xwl_window_ptr->content);
    if (NULL != xwl_window_ptr->wlr_xwayland_surface_ptr) {
        xwl_window_ptr->wlr_xwayland_surface_ptr->data = NULL;
    }
    free(xwl_window_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_window_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, destroy_listener);

    wlmaker_xwl_window_destroy(xwl_window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_configure` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_window_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, request_configure_listener);
    struct wlr_xwayland_surface_configure_event *cfg_event_ptr = data_ptr;

    bs_log(BS_INFO, "Reqeust configure for %p: "
           "%"PRId16" x %"PRId16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_window_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);

    // FIXME:
    // -> if we have content/surface: check what that means, with respect to
    //    the surface::commit handler.

    // It appears this needs to be ACKed with a surface_configure.
    wlr_xwayland_surface_configure(
        xwl_window_ptr->wlr_xwayland_surface_ptr,
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
void _xwl_window_handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, associate_listener);
    bs_log(BS_INFO, "Associate XWL window %p with wlr_surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);

    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.commit,
        &xwl_window_ptr->surface_commit_listener,
        _xwl_window_handle_surface_commit);
    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.map,
        &xwl_window_ptr->surface_map_listener,
        _xwl_window_handle_surface_map);
    wlmtk_util_connect_listener_signal(
        &xwl_window_ptr->wlr_xwayland_surface_ptr->surface->events.unmap,
        &xwl_window_ptr->surface_unmap_listener,
        _xwl_window_handle_surface_unmap);

    BS_ASSERT(NULL == xwl_window_ptr->surface_ptr);
    xwl_window_ptr->surface_ptr = wlmtk_surface_create(
        xwl_window_ptr->wlr_xwayland_surface_ptr->surface,
        xwl_window_ptr->env_ptr);
    if (NULL == xwl_window_ptr->surface_ptr) {
        bs_log(BS_ERROR, "FIXME: Error.");
    }

    wlmtk_content_set_surface(
        &xwl_window_ptr->content,
        xwl_window_ptr->surface_ptr);

    _xwl_window_apply_decorations(xwl_window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_window_handle_dissociate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, dissociate_listener);

    wlmtk_content_set_surface(&xwl_window_ptr->content, NULL);
    if (NULL != xwl_window_ptr->surface_ptr) {
        wlmtk_surface_destroy(xwl_window_ptr->surface_ptr);
        xwl_window_ptr->surface_ptr = NULL;
    }

    wl_list_remove(&xwl_window_ptr->surface_unmap_listener.link);
    wl_list_remove(&xwl_window_ptr->surface_map_listener.link);
    wl_list_remove(&xwl_window_ptr->surface_commit_listener.link);

    bs_log(BS_INFO, "Dissociate XWL window %p from wlr_surface %p",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_title` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_window_handle_set_title(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, set_title_listener);
    wlmtk_window_set_title(
        xwl_window_ptr->window_ptr,
        xwl_window_ptr->wlr_xwayland_surface_ptr->title);
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
void _xwl_window_handle_set_decorations(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, set_decorations_listener);
    _xwl_window_apply_decorations(xwl_window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Surface commit handler: Pass on the current serial. */
void _xwl_window_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_commit_listener);

    bs_log(BS_DEBUG, "XWL window %p commit surface %p, current %d x %d",
           xwl_window_ptr, xwl_window_ptr->wlr_xwayland_surface_ptr->surface,
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.width,
           xwl_window_ptr->wlr_xwayland_surface_ptr->surface->current.height);

    wlmtk_content_commit_serial(
        &xwl_window_ptr->content,
        xwl_window_ptr->serial);
}

/* ------------------------------------------------------------------------- */
/** Surface map handler: also indicates the window can be mapped. */
void _xwl_window_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_map_listener);

    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        xwl_window_ptr->server_ptr);
    wlmtk_workspace_map_window(
        wlmaker_workspace_wlmtk(workspace_ptr),
        xwl_window_ptr->window_ptr);
    wlmtk_window_set_position(xwl_window_ptr->window_ptr, 40, 30);
}

/* ------------------------------------------------------------------------- */
/** Surface unmap: indicates the window should be unmapped. */
void _xwl_window_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_window_t, surface_unmap_listener);

    wlmtk_workspace_unmap_window(
        wlmtk_window_get_workspace(xwl_window_ptr->window_ptr),
        xwl_window_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_maximized. */
uint32_t _xwl_window_content_request_maximized(
    wlmtk_content_t *content_ptr,
    bool maximized)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_window_t, content);
    wlmtk_window_commit_maximized(xwl_window_ptr->window_ptr, maximized);
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_fullscreen. */
uint32_t _xwl_window_content_request_fullscreen(
    wlmtk_content_t *content_ptr,
    bool fullscreen)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_window_t, content);
    wlmtk_window_commit_fullscreen(xwl_window_ptr->window_ptr, fullscreen);
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_size. */
uint32_t _xwl_window_content_request_size(
    wlmtk_content_t *content_ptr,
    int width,
    int height)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_window_t, content);
    wlr_xwayland_surface_configure(
        xwl_window_ptr->wlr_xwayland_surface_ptr,
        0, 0, width, height);
    return xwl_window_ptr->serial++;
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_close. */
static void _xwl_window_content_request_close(
    wlmtk_content_t *content_ptr)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_window_t, content);
    wlr_xwayland_surface_close(xwl_window_ptr->wlr_xwayland_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::set_activated. */
void _xwl_window_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmaker_xwl_window_t *xwl_window_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_xwl_window_t, content);

    wlr_xwayland_surface_activate(
        xwl_window_ptr->wlr_xwayland_surface_ptr, activated);
    wlmtk_surface_set_activated(xwl_window_ptr->surface_ptr, activated);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets whether this window should be server-side-decorated.
 *
 * @param xwl_window_ptr
 */
void _xwl_window_apply_decorations(wlmaker_xwl_window_t *xwl_window_ptr)
{
    static const xwl_atom_identifier_t borderless_window_types[] = {
        NET_WM_WINDOW_TYPE_TOOLTIP, XWL_MAX_ATOM_ID};

    // TODO(kaeser@gubbe.ch): Adapt whether NO_BORDER or NO_TITLE was set.
    if (xwl_window_ptr->wlr_xwayland_surface_ptr->decorations ==
        WLR_XWAYLAND_SURFACE_DECORATIONS_ALL &&
        !xwl_is_window_type(
            xwl_window_ptr->xwl_ptr,
            xwl_window_ptr->wlr_xwayland_surface_ptr,
            borderless_window_types)) {
        wlmtk_window_set_server_side_decorated(
            xwl_window_ptr->window_ptr, true);
    } else {
        wlmtk_window_set_server_side_decorated(
            xwl_window_ptr->window_ptr, false);
    }
}

/* == End of xwl_window.c ================================================== */
