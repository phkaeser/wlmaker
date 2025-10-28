/* ========================================================================= */
/**
 * @file xwl_surface.c
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

#if defined(WLMAKER_HAVE_XWAYLAND)

#include "xwl_surface.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <xcb/xproto.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/xwayland/xwayland.h>
#undef WLR_USE_UNSTABLE

#include "xwl_popup.h"
#include "toolkit/toolkit.h"


/* == Declarations ========================================================= */

/** State of the XWayland window surface. */
struct _wlmaker_xwl_surface_t {
    /** Corresponding wlroots XWayland surface. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;
    /** Back-link to the XWayland server. */
    wlmaker_xwl_t             *xwl_ptr;

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

    struct wl_listener        surface_map_listener;
    struct wl_listener        surface_unmap_listener;

    /** The toolkit surface. Only available once 'associated'. */
    wlmtk_surface_t           *surface_ptr;

    /** The toolkit window, in case the surface does not have a parent. */
    wlmtk_window2_t           *window_ptr;
    /** The XWayland popup, in case this surface has a parent. */
    wlmaker_xwl_popup_t       *xwl_popup_ptr;
};

static void _xwl_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_surface_handle_associate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_dissociate(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_surface_handle_set_title(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_set_parent(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_set_decorations(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_set_geometry(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_surface_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _xwl_surface_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _xwl_surface_apply_decorations(
    wlmaker_xwl_surface_t *xwl_surface_ptr);

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_surface_t *wlmaker_xwl_surface_create(
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    wlmaker_xwl_t *xwl_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_surface_t));
    if (NULL == xwl_surface_ptr) return NULL;
    xwl_surface_ptr->wlr_xwayland_surface_ptr = wlr_xwayland_surface_ptr;
    wlr_xwayland_surface_ptr->data = xwl_surface_ptr;
    xwl_surface_ptr->xwl_ptr = xwl_ptr;
    xwl_surface_ptr->server_ptr = server_ptr;

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.destroy,
        &xwl_surface_ptr->destroy_listener,
        _xwl_surface_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.request_configure,
        &xwl_surface_ptr->request_configure_listener,
        _xwl_surface_handle_request_configure);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.associate,
        &xwl_surface_ptr->associate_listener,
        _xwl_surface_handle_associate);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.dissociate,
        &xwl_surface_ptr->dissociate_listener,
        _xwl_surface_handle_dissociate);

    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_title,
        &xwl_surface_ptr->set_title_listener,
        _xwl_surface_handle_set_title);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_parent,
        &xwl_surface_ptr->set_parent_listener,
        _xwl_surface_handle_set_parent);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_decorations,
        &xwl_surface_ptr->set_decorations_listener,
        _xwl_surface_handle_set_decorations);
    wlmtk_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_geometry,
        &xwl_surface_ptr->set_geometry_listener,
        _xwl_surface_handle_set_geometry);

    bs_log(BS_INFO, "Created XWL surface %p for wlr_xwayland_surface %p",
           xwl_surface_ptr, wlr_xwayland_surface_ptr);

    return xwl_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_surface_destroy(wlmaker_xwl_surface_t *xwl_surface_ptr)
{
    bs_log(BS_INFO, "Destroy XWL surface %p", xwl_surface_ptr);

    _xwl_surface_handle_dissociate(&xwl_surface_ptr->dissociate_listener, 0);

    wl_list_remove(&xwl_surface_ptr->set_geometry_listener.link);
    wl_list_remove(&xwl_surface_ptr->set_decorations_listener.link);
    wl_list_remove(&xwl_surface_ptr->set_parent_listener.link);
    wl_list_remove(&xwl_surface_ptr->set_title_listener.link);
    wl_list_remove(&xwl_surface_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_surface_ptr->associate_listener.link);
    wl_list_remove(&xwl_surface_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_surface_ptr->destroy_listener.link);

    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr) {
        xwl_surface_ptr->wlr_xwayland_surface_ptr->data = NULL;
    }
    free(xwl_surface_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, destroy_listener);
    wlmaker_xwl_surface_destroy(xwl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `request_configure` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_surface_handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, request_configure_listener);
    struct wlr_xwayland_surface_configure_event *cfg_event_ptr = data_ptr;

    bs_log(BS_INFO, "Request configure for %p: "
           "%"PRId16" x %"PRId16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_surface_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);

    // FIXME:
    // -> if we have surface: check what that means, with respect to
    //    the surface::commit handler.

    // It appears this needs to be ACKed with a surface_configure.
    wlr_xwayland_surface_configure(
        xwl_surface_ptr->wlr_xwayland_surface_ptr,
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
void _xwl_surface_handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, associate_listener);
    wlmaker_xwl_surface_t *parent_xwl_surface_ptr = NULL;
    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        parent_xwl_surface_ptr =
            xwl_surface_ptr->wlr_xwayland_surface_ptr->parent->data;
    }
    bs_log(BS_INFO,
           "Associate XWL surface %p with wlr_surface %p, parent %p at %d, %d",
           xwl_surface_ptr, xwl_surface_ptr->wlr_xwayland_surface_ptr->surface,
           parent_xwl_surface_ptr,
           xwl_surface_ptr->wlr_xwayland_surface_ptr->x,
           xwl_surface_ptr->wlr_xwayland_surface_ptr->y);
    for (size_t i = 0;
         i < xwl_surface_ptr->wlr_xwayland_surface_ptr->window_type_len;
         ++i) {
        const char *name_ptr = xwl_atom_name(
            xwl_surface_ptr->xwl_ptr,
            xwl_surface_ptr->wlr_xwayland_surface_ptr->window_type[i]);
        if (NULL != name_ptr) {
            bs_log(BS_INFO, "  XWL surface %p has window type %s",
                   xwl_surface_ptr, name_ptr);
        }
    }

    BS_ASSERT(NULL == xwl_surface_ptr->surface_ptr);

    xwl_surface_ptr->surface_ptr = wlmtk_surface_create(
        xwl_surface_ptr->wlr_xwayland_surface_ptr->surface,
        xwl_surface_ptr->server_ptr->wlr_seat_ptr);
    if (NULL == xwl_surface_ptr->surface_ptr) {
        // TODO(kaeser@gubbe.ch): Relay error to client, instead of crash.
        bs_log(BS_FATAL, "Failed wlmtk_surface_create.");
        return;
    }

    wlmtk_surface_connect_map_listener_signal(
        xwl_surface_ptr->surface_ptr,
        &xwl_surface_ptr->surface_map_listener,
        _xwl_surface_handle_surface_map);
    wlmtk_surface_connect_unmap_listener_signal(
        xwl_surface_ptr->surface_ptr,
        &xwl_surface_ptr->surface_unmap_listener,
        _xwl_surface_handle_surface_unmap);

    // Currently we treat parent-less windows AND modal windows as toplevel.
    // Modal windows should actually be child wlmtk_window_t, but that isn't
    // supported yet.
    if (NULL == xwl_surface_ptr->wlr_xwayland_surface_ptr->parent ||
        xwl_surface_ptr->wlr_xwayland_surface_ptr->modal) {

        BS_ASSERT(NULL == xwl_surface_ptr->window_ptr);

        xwl_surface_ptr->window_ptr = wlmtk_window2_create(
            wlmtk_surface_element(xwl_surface_ptr->surface_ptr),
            &xwl_surface_ptr->server_ptr->style.window,
            &xwl_surface_ptr->server_ptr->style.menu);
        if (NULL == xwl_surface_ptr->window_ptr) {
            // TODO(kaeser@gubbe.ch): Relay error to client, instead of crash.
            bs_log(BS_FATAL, "Failed wlmtk_window2_create.");
            return;
        }
        _xwl_surface_apply_decorations(xwl_surface_ptr);
        // FIXME: Set properties.

    } else {

        BS_ASSERT(NULL == xwl_surface_ptr->xwl_popup_ptr);
        xwl_surface_ptr->xwl_popup_ptr = wlmaker_xwl_popup_create(
            xwl_surface_ptr);
        if (NULL == xwl_surface_ptr->xwl_popup_ptr) {
            // TODO(kaeser@gubbe.ch): Relay error to client, instead of crash.
            bs_log(BS_FATAL, "Failed wlmaker_xwl_popup_create.");
            return;
        }
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_surface_handle_dissociate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, dissociate_listener);

    if (NULL != xwl_surface_ptr->window_ptr) {
        wlmtk_window2_destroy(xwl_surface_ptr->window_ptr);
        xwl_surface_ptr->window_ptr = NULL;
    }
    if (NULL != xwl_surface_ptr->xwl_popup_ptr) {
        wlmaker_xwl_popup_destroy(xwl_surface_ptr->xwl_popup_ptr);
        xwl_surface_ptr->xwl_popup_ptr = NULL;
    }

    if (NULL != xwl_surface_ptr->surface_ptr) {
        wlmtk_util_disconnect_listener(&xwl_surface_ptr->surface_map_listener);
        wlmtk_util_disconnect_listener(&xwl_surface_ptr->surface_unmap_listener);
        wlmtk_surface_destroy(xwl_surface_ptr->surface_ptr);
        xwl_surface_ptr->surface_ptr = NULL;
    }

    bs_log(BS_INFO, "Dissociate XWL surface %p from wlr_surface %p",
           xwl_surface_ptr, xwl_surface_ptr->wlr_xwayland_surface_ptr->surface);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_title` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_surface_handle_set_title(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_title_listener);

    if (NULL != xwl_surface_ptr->window_ptr) {
        wlmtk_window2_set_title(
            xwl_surface_ptr->window_ptr,
            xwl_surface_ptr->wlr_xwayland_surface_ptr->title);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_parent` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _xwl_surface_handle_set_parent(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_parent_listener);

    BS_ASSERT(NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent);
    wlmaker_xwl_surface_t *parent_xwl_surface_ptr =
        xwl_surface_ptr->wlr_xwayland_surface_ptr->parent->data;

    bs_log(BS_ERROR, "TODO: Set parent for XWL surface %p to XWL surface %p",
           xwl_surface_ptr, parent_xwl_surface_ptr);
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
void _xwl_surface_handle_set_decorations(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_decorations_listener);
    _xwl_surface_apply_decorations(xwl_surface_ptr);
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
void _xwl_surface_handle_set_geometry(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_geometry_listener);

    // For XWayland, the surface's position is given relative to the "root"
    // of the specified windows. For @ref wlmtk_element_t, the position is
    // just relative to the pareent @ref wlmtk_container_t. So we need to
    // subtract each parent popup's position.
    int x = xwl_surface_ptr->wlr_xwayland_surface_ptr->x;
    int y = xwl_surface_ptr->wlr_xwayland_surface_ptr->y;

    bs_log(BS_ERROR, "TODO: set_geometry %d,%d", x, y);
}

/* ------------------------------------------------------------------------- */
void _xwl_surface_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, surface_map_listener);

    if (NULL == xwl_surface_ptr->window_ptr) return;

    wlmtk_workspace_t *workspace_ptr =
        wlmtk_root_get_current_workspace(
            xwl_surface_ptr->server_ptr->root_ptr);
    wlmtk_workspace_map_window2(workspace_ptr, xwl_surface_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
void _xwl_surface_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, surface_unmap_listener);

    if (NULL == xwl_surface_ptr->window_ptr) return;

    wlmtk_workspace_unmap_window2(
        wlmtk_window2_get_workspace(xwl_surface_ptr->window_ptr),
        xwl_surface_ptr->window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Sets whether this window should be server-side-decorated.
 *
 * @param xwl_surface_ptr
 */
void _xwl_surface_apply_decorations(wlmaker_xwl_surface_t *xwl_surface_ptr)
{
    static const xwl_atom_identifier_t borderless_window_types[] = {
        NET_WM_WINDOW_TYPE_TOOLTIP, XWL_MAX_ATOM_ID};

    if (NULL == xwl_surface_ptr->window_ptr) return;

    // TODO(kaeser@gubbe.ch): Adapt whether NO_BORDER or NO_TITLE was set.
    if (xwl_surface_ptr->wlr_xwayland_surface_ptr->decorations ==
        WLR_XWAYLAND_SURFACE_DECORATIONS_ALL &&
        !xwl_is_window_type(
            xwl_surface_ptr->xwl_ptr,
            xwl_surface_ptr->wlr_xwayland_surface_ptr,
            borderless_window_types)) {
        wlmtk_window2_set_server_side_decorated(
            xwl_surface_ptr->window_ptr,
            true);
    } else {
        wlmtk_window2_set_server_side_decorated(
            xwl_surface_ptr->window_ptr,
            false);
    }
}

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_nested(bs_test_t *test_ptr);

static void fake_init_wlr_xwayland_surface(
    struct wlr_xwayland_surface* wlr_xwayland_surface_ptr);

const bs_test_case_t wlmaker_xwl_surface_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "nested", test_nested },
    { 0, NULL, NULL },
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmaker_server_t server = {};
    struct wlr_xwayland_surface wlr_xwayland_surface;
    fake_init_wlr_xwayland_surface(&wlr_xwayland_surface);

    wlmaker_xwl_surface_t *xwl_surface_ptr = wlmaker_xwl_surface_create(
        &wlr_xwayland_surface, NULL, &server);

    BS_TEST_VERIFY_NEQ(test_ptr, NULL, xwl_surface_ptr);
    wlmaker_xwl_surface_destroy(xwl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests nesting of XWayland surfaces, ie. parenting. */
void test_nested(bs_test_t *test_ptr)
{
    wlmaker_server_t server = {};

    // FIXME: Make this test something.
    struct wlr_xwayland_surface surface0;
    fake_init_wlr_xwayland_surface(&surface0);
    wlmaker_xwl_surface_t *surface0_ptr = wlmaker_xwl_surface_create(
        &surface0, NULL, &server);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface0_ptr);

    struct wlr_xwayland_surface surface1;
    fake_init_wlr_xwayland_surface(&surface1);
    wlmaker_xwl_surface_t *surface1_ptr = wlmaker_xwl_surface_create(
        &surface1, NULL, &server);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface1_ptr);

    struct wlr_xwayland_surface surface2;
    fake_init_wlr_xwayland_surface(&surface2);
    wlmaker_xwl_surface_t *surface2_ptr = wlmaker_xwl_surface_create(
        &surface2, NULL, &server);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface2_ptr);

    surface2.parent = &surface1;
    wl_signal_emit_mutable(&surface2.events.set_parent, NULL);

    surface2.x = 120;
    surface2.y = 12;
    wl_signal_emit_mutable(&surface2.events.set_geometry, NULL);

    wlmaker_xwl_surface_destroy(surface2_ptr);
    wlmaker_xwl_surface_destroy(surface1_ptr);
    wlmaker_xwl_surface_destroy(surface0_ptr);
}

/* ------------------------------------------------------------------------- */
/** Fake-initializes the `wlr_xwayland_surface_ptr`. */
void fake_init_wlr_xwayland_surface(
    struct wlr_xwayland_surface* wlr_xwayland_surface_ptr)
{
    *wlr_xwayland_surface_ptr = (struct wlr_xwayland_surface){};
    wl_signal_init(&wlr_xwayland_surface_ptr->events.destroy);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.request_configure);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.associate);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.dissociate);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.set_title);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.set_parent);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.set_decorations);
    wl_signal_init(&wlr_xwayland_surface_ptr->events.set_geometry);
}

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
/* == End of xwl_surface.c ================================================= */
