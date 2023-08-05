/* ========================================================================= */
/**
 * @file xwl.c
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
 *
 * @internal
 * The current XWayland implementation is not very cleanly designed, and is
 * more of an advanced prototype than a finished implementation.
 * TODO(kaeser@gubbe.ch): Re-design, once object model is updated.
 *
 * Known issues:
 *
 * * Scene graph API nodes for toplevel windows are created early. This leads
 *   to issues with ownership (cleanup?), stacking order, and when properties
 *   (position) are set. It'd be better to only create them when mapping a
 *   window (and destroying when unmapping).
 *
 * * Windows with parents are created as plain surfaces and don't clearly show
 *   their stacking order. Decorations may not get applied in all cases.
 *
 * * Stacking order is not tackled, eg. popups may appear below. Reproduce:
 *   Open `emacs`, click a menu, and hover over a menu item for the tooltip to
 *   appear. When moving across menus, the tooltip sometimes appears below the
 *   menu window.
 *
 * * Popups or dialogs may not be activated or focussed correctly. Reproduce:
 *   Open `emacs`, open the `File` menu, and `Visit New File...`. The dialogue
 *   does not accept mouse events. Moving the dialogue window moves the entire
 *   emacs window.
 *
 * * `modal` windows are not identified and treated as such.
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
    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

    /** XWayland server and XWM. */
    struct wlr_xwayland       *wlr_xwayland_ptr;

    /** Listener for the `new_surface` signal raised by `wlr_xwayland`. */
    struct wl_listener        new_surface_listener;
};

/** A scene-graph API subtree for a wlroots XWayland surface. */
typedef struct {
    /** The wlroots XWayland surface that we're wrapping here. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;
    /** Holds the wlroots surface. Element of `wlr_scene_tree_ptr`. */
    struct wlr_scene_surface  *wlr_scene_surface_ptr;

    /** Listener: `map` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener: `unmap` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_unmap_listener;
    /** Listener: `destroy` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_destroy_listener;
} wlmaker_scene_xwayland_surface_t;

static struct wlr_scene_surface *wlmaker_scene_xwayland_surface_create(
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr);
static void xwls_destroy(
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr);
static void xwls_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void xwls_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void xwls_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/** Implementation of an Xwayland user interface component. */
typedef struct {
    /** State of the view. */
    wlmaker_view_t            view;
    /** Whether `view` was initialized. */
    bool                      view_initialized;

    /** Link to the corresponding `struct wlr_xwayland_surface`. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;

    /** Back-link to the interface. */
    wlmaker_xwl_t             *xwl_ptr;

    /** The surface's tree. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;
    /**
     * Scene graph API node of the actual surface. */
    struct wlr_scene_surface  *wlr_scene_surface_ptr;

    /** Listener: `destroy` event of `wlr_scene_tree_ptr`. */
    struct wl_listener        tree_destroy_listener;

    /** Listener for the `destroy` signal of `wlr_xwayland_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_configure` signal of `wlr_xwayland_surface`. */
    struct wl_listener        request_configure_listener;

    /** Listener for the `associate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        associate_listener;
    /** Listener for the `dissociate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        dissociate_listener;

    /** Listener for the `set_parent` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_parent_listener;
    /** Listener for the `set_decorations` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_decorations_listener;
    /** Listener for the `set_geometry` signal of `wlr_xwayland_surface`. */
    struct wl_listener        set_geometry_listener;

    /** Listener for the `map` signal of `wlr_xwayland_surface_ptr->surface` */
    struct wl_listener        surface_map_listener;
    /** Listener for `unmap` signal of `wlr_xwayland_surface_ptr->surface` */
    struct wl_listener        surface_unmap_listener;
} wlmaker_xwl_surface_t;

static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_xwl_surface_t *xwl_surface_create(
    wlmaker_xwl_t *xwl_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr);
static void xwl_surface_destroy(
    wlmaker_xwl_surface_t *xwl_surface_ptr);

static void handle_tree_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_associate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_dissociate(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_set_parent(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_set_decorations(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_set_geometry(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static uint32_t wlmaker_xwl_surface_set_activated(
    wlmaker_view_t *view_ptr,
    bool activated);
static void wlmaker_xwl_surface_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr);
static void wlmaker_xwl_surface_set_size(
    wlmaker_view_t *view_ptr,
    int width, int height);
static void wlmaker_xwl_surface_set_maximized(
    wlmaker_view_t *view_ptr,
    bool maximize);
static void wlmaker_xwl_surface_set_fullscreen(
    wlmaker_view_t *view_ptr,
    bool fullscreen);
static void wlmaker_xwl_surface_send_close_callback(wlmaker_view_t *view_ptr);

static void handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);
static void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);

static struct wlr_scene_tree *get_parent_wlr_scene_tree_ptr(
    wlmaker_xwl_surface_t *xwl_surface_ptr);

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     xwl_surface_view_impl = {
    .set_activated = wlmaker_xwl_surface_set_activated,
    .get_size = wlmaker_xwl_surface_get_size,
    .set_size = wlmaker_xwl_surface_set_size,
    .set_maximized = wlmaker_xwl_surface_set_maximized,
    .set_fullscreen = wlmaker_xwl_surface_set_fullscreen
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_t *wlmaker_xwl_create(wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_t *xwl_ptr = logged_calloc(1, sizeof(wlmaker_xwl_t));
    if (NULL == xwl_ptr) return NULL;
    xwl_ptr->server_ptr = server_ptr;

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
 * Creates a scene-graph API node for the XWayland wlroots surface.
 *
 * @param wlr_scene_tree_ptr  Scene graph where the surface node should get
 *     attached to.
 * @param wlr_xwayland_surface_ptr The wlroots XWayland surface to create the
 *     node for.
 *
 * @return A scene surface.
 */
struct wlr_scene_surface *wlmaker_scene_xwayland_surface_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        logged_calloc(1, sizeof(wlmaker_scene_xwayland_surface_t));
    if (NULL == scene_xwayland_surface_ptr) return NULL;
    scene_xwayland_surface_ptr->wlr_xwayland_surface_ptr =
        wlr_xwayland_surface_ptr;

    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->surface->events.map,
        &scene_xwayland_surface_ptr->surface_map_listener,
        xwls_handle_surface_map);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->surface->events.unmap,
        &scene_xwayland_surface_ptr->surface_unmap_listener,
        xwls_handle_surface_unmap);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->surface->events.destroy,
        &scene_xwayland_surface_ptr->surface_destroy_listener,
        xwls_handle_surface_destroy);

    // TODO(kaeser@gubbe.ch): See if this better be implemented with a
    // wlr_scene_subsurface_tree_create(...).
    scene_xwayland_surface_ptr->wlr_scene_surface_ptr =
        wlr_scene_surface_create(
            wlr_scene_tree_ptr,
            wlr_xwayland_surface_ptr->surface);
    if (NULL == scene_xwayland_surface_ptr->wlr_scene_surface_ptr) {
        xwls_destroy(scene_xwayland_surface_ptr);
        return NULL;
    }
    // Set the initial position. @ref handle_set_geometry will take care of
    // further changes.
    wlr_scene_node_set_position(
        &scene_xwayland_surface_ptr->wlr_scene_surface_ptr->buffer->node,
        wlr_xwayland_surface_ptr->x,
        wlr_xwayland_surface_ptr->y);

    return scene_xwayland_surface_ptr->wlr_scene_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor for the xwayland surface scene node.
 *
 * @param scene_xwayland_surface_ptr
 */
void xwls_destroy(wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr)
{
    // wlr_scene_surface_ptr is now tied with wlr_xwayland_surface_ptr, and
    // will get collected via the surface's destroy operation.

    wl_list_remove(&scene_xwayland_surface_ptr->surface_destroy_listener.link);
    wl_list_remove(&scene_xwayland_surface_ptr->surface_unmap_listener.link);
    wl_list_remove(&scene_xwayland_surface_ptr->surface_map_listener.link);

    free(scene_xwayland_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal of the surface. Will enable the node.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void xwls_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        BS_CONTAINER_OF(listener_ptr, wlmaker_scene_xwayland_surface_t,
                        surface_map_listener);
    wlr_scene_node_set_enabled(
        &scene_xwayland_surface_ptr->wlr_scene_surface_ptr->buffer->node, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal of the surface. Will disable the node.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void xwls_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        BS_CONTAINER_OF(listener_ptr, wlmaker_scene_xwayland_surface_t,
                        surface_unmap_listener);
    wlr_scene_node_set_enabled(
        &scene_xwayland_surface_ptr->wlr_scene_surface_ptr->buffer->node, false);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the surface. Will destroy all resources
 * except the node.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void xwls_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        BS_CONTAINER_OF(listener_ptr, wlmaker_scene_xwayland_surface_t,
                        surface_destroy_listener);
    xwls_destroy(scene_xwayland_surface_ptr);
}

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
    wlmaker_xwl_t *xwl_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_t, new_surface_listener);
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr = data_ptr;

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
    // To find ourselves later on...
    wlr_xwayland_surface_ptr->data = xwl_surface_ptr;

    struct wlr_scene_tree *parent_wlr_scene_tree_ptr =
        get_parent_wlr_scene_tree_ptr(xwl_surface_ptr);
    xwl_surface_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        parent_wlr_scene_tree_ptr);
    if (NULL == xwl_surface_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_tree_create(%p)",
               parent_wlr_scene_tree_ptr);
        xwl_surface_destroy(xwl_surface_ptr);
        return NULL;
    }

    wlm_util_connect_listener_signal(
        &xwl_surface_ptr->wlr_scene_tree_ptr->node.events.destroy,
        &xwl_surface_ptr->tree_destroy_listener,
        handle_tree_destroy);

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
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_parent,
        &xwl_surface_ptr->set_parent_listener,
        handle_set_parent);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_decorations,
        &xwl_surface_ptr->set_decorations_listener,
        handle_set_decorations);
    wlm_util_connect_listener_signal(
        &wlr_xwayland_surface_ptr->events.set_geometry,
        &xwl_surface_ptr->set_geometry_listener,
        handle_set_geometry);

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

    wl_list_remove(&xwl_surface_ptr->set_geometry_listener.link);
    wl_list_remove(&xwl_surface_ptr->set_decorations_listener.link);
    wl_list_remove(&xwl_surface_ptr->set_parent_listener.link);
    wl_list_remove(&xwl_surface_ptr->dissociate_listener.link);
    wl_list_remove(&xwl_surface_ptr->associate_listener.link);
    wl_list_remove(&xwl_surface_ptr->request_configure_listener.link);
    wl_list_remove(&xwl_surface_ptr->destroy_listener.link);
    wl_list_remove(&xwl_surface_ptr->tree_destroy_listener.link);

    if (NULL != xwl_surface_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&xwl_surface_ptr->wlr_scene_tree_ptr->node);
        xwl_surface_ptr->wlr_scene_tree_ptr = NULL;
    }

    free(xwl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the tree. Will clean up.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_tree_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, tree_destroy_listener);

    // Prevent double-deletes.
    xwl_surface_ptr->wlr_scene_tree_ptr = NULL;
    xwl_surface_destroy(xwl_surface_ptr);
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

    bs_log(BS_INFO, "Reqeust configure for %p: "
           "%"PRId16" x %"PRId16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_surface_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);

    // Uh, that's ugly. We'd prefer to set the inner size of the view, but the
    // set_size call does include decorations. So we call get_size, substract
    // decorations, and then go from there...
    uint32_t width, height;
    if (xwl_surface_ptr->view_initialized) {
        wlmaker_view_get_size(&xwl_surface_ptr->view, &width, &height);
        width -= xwl_surface_ptr->wlr_xwayland_surface_ptr->width;
        height -= xwl_surface_ptr->wlr_xwayland_surface_ptr->height;
    } else {
        width = 0;
        height = 0;
    }
    width += cfg_event_ptr->width;
    height += cfg_event_ptr->height;

    if (xwl_surface_ptr->view_initialized) {
        wlmaker_view_set_size(&xwl_surface_ptr->view, width, height);
    } else {
        wlr_xwayland_surface_configure(
            xwl_surface_ptr->wlr_xwayland_surface_ptr,
            cfg_event_ptr->x, cfg_event_ptr->y,
            cfg_event_ptr->width, cfg_event_ptr->height);
    }
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
void handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, associate_listener);
    bs_log(BS_INFO, "Associate %p", xwl_surface_ptr);

    BS_ASSERT(!xwl_surface_ptr->view_initialized);

    xwl_surface_ptr->wlr_scene_surface_ptr =
        wlmaker_scene_xwayland_surface_create(
            xwl_surface_ptr->wlr_scene_tree_ptr,
            xwl_surface_ptr->wlr_xwayland_surface_ptr);
    if (NULL == xwl_surface_ptr->wlr_scene_surface_ptr) {
        bs_log(BS_FATAL, "Failed wlmaker_scene_xwayland_surface_create(%p, %p)",
               xwl_surface_ptr->wlr_scene_tree_ptr,
               xwl_surface_ptr->wlr_xwayland_surface_ptr);
        // TODO(kaeser@gubbe.ch): Should pass error back to client, and not
        // abort the server.
        BS_ABORT();
    }

    wlm_util_connect_listener_signal(
        &xwl_surface_ptr->wlr_xwayland_surface_ptr->surface->events.map,
        &xwl_surface_ptr->surface_map_listener,
        handle_surface_map);
    wlm_util_connect_listener_signal(
        &xwl_surface_ptr->wlr_xwayland_surface_ptr->surface->events.unmap,
        &xwl_surface_ptr->surface_unmap_listener,
        handle_surface_unmap);

    // TODO(kaeser@gubbe.ch): Deal as toplevel or non-toplevel.
    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        return;
    }

    wlmaker_view_init(
        &xwl_surface_ptr->view,
        &xwl_surface_view_impl,
        xwl_surface_ptr->xwl_ptr->server_ptr,
        xwl_surface_ptr->wlr_xwayland_surface_ptr->surface,
        xwl_surface_ptr->wlr_scene_tree_ptr,
        wlmaker_xwl_surface_send_close_callback);
    xwl_surface_ptr->view_initialized = true;

    // TODO(kaeser@gubbe.ch): Adapt whether NO_BORDER or NO_TITLE was set.
    wlmaker_view_set_server_side_decoration(
        &xwl_surface_ptr->view,
        (xwl_surface_ptr->wlr_xwayland_surface_ptr->decorations ==
         WLR_XWAYLAND_SURFACE_DECORATIONS_ALL));
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `dissociate` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_dissociate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, dissociate_listener);
    bs_log(BS_INFO, "Dissociate %p", xwl_surface_ptr);

    if (NULL == xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        // TODO(kaeser@gubbe.ch): Deal as toplevel or non-toplevel.

        // Take our node out of the view's tree, otherwise it'll be destroyed.
        // However, we want to keep it.
        wlr_scene_node_reparent(
            &xwl_surface_ptr->wlr_scene_tree_ptr->node,
            &xwl_surface_ptr->xwl_ptr->server_ptr->void_wlr_scene_ptr->tree);

        wlmaker_view_fini(&xwl_surface_ptr->view);
        xwl_surface_ptr->view_initialized = false;
    }

    xwl_surface_ptr->wlr_scene_surface_ptr = NULL;
    wl_list_remove(&xwl_surface_ptr->surface_unmap_listener.link);
    wl_list_remove(&xwl_surface_ptr->surface_map_listener.link);
}


/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_parent` event of `struct wlr_xwayland_surface`.
 *
 * Will identify the `wlr_scene_tree` of the parent's surface, and (if it turns
 * out to be different from the current parent), re-attach this surface's own
 * `wlr_scene_tree` there.
 * If the new surface's parent is NULL, the tree is attached to the void
 * surface.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_set_parent(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_parent_listener);

    struct wlr_scene_tree *parent_wlr_scene_tree_ptr =
        get_parent_wlr_scene_tree_ptr(xwl_surface_ptr);
    if (parent_wlr_scene_tree_ptr ==
        xwl_surface_ptr->wlr_scene_tree_ptr->node.parent) {
        // Uh, nothing to do here.
        return;
    }

    wlr_scene_node_reparent(
        &xwl_surface_ptr->wlr_scene_tree_ptr->node,
        parent_wlr_scene_tree_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_decorations` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_set_decorations(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_decorations_listener);
    if (xwl_surface_ptr->view_initialized) {
        // TODO(kaeser@gubbe.ch): Adapt whether NO_BORDER or NO_TITLE was set.
        wlmaker_view_set_server_side_decoration(
            &xwl_surface_ptr->view,
            (xwl_surface_ptr->wlr_xwayland_surface_ptr->decorations ==
             WLR_XWAYLAND_SURFACE_DECORATIONS_ALL));
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `set_geometry` event of `struct wlr_xwayland_surface`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_set_geometry(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, set_geometry_listener);

    if (NULL != xwl_surface_ptr->wlr_scene_surface_ptr) {
        wlr_scene_node_set_position(
            &xwl_surface_ptr->wlr_scene_surface_ptr->buffer->node,
            xwl_surface_ptr->wlr_xwayland_surface_ptr->x,
            xwl_surface_ptr->wlr_xwayland_surface_ptr->y);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Method for view activation.
 *
 * @param view_ptr
 * @param activated
 *
 * @return Zero, default placeholder for the serial.
 */
uint32_t wlmaker_xwl_surface_set_activated(
    wlmaker_view_t *view_ptr,
    bool activated)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        view_ptr, wlmaker_xwl_surface_t, view);
    wlr_xwayland_surface_activate(
        xwl_surface_ptr->wlr_xwayland_surface_ptr, activated);
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Method for getting the view's size.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
void wlmaker_xwl_surface_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        view_ptr, wlmaker_xwl_surface_t, view);

    *width_ptr = xwl_surface_ptr->wlr_xwayland_surface_ptr->width;
    *height_ptr = xwl_surface_ptr->wlr_xwayland_surface_ptr->height;
}

/* ------------------------------------------------------------------------- */
/**
 * Method for setting the view's size.
 *
 * @param view_ptr
 * @param width
 * @param height
 */
void wlmaker_xwl_surface_set_size(
    wlmaker_view_t *view_ptr,
    int width, int height)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        view_ptr, wlmaker_xwl_surface_t, view);

    wlr_xwayland_surface_configure(
        xwl_surface_ptr->wlr_xwayland_surface_ptr,
        0, 0,
        width, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Method for maximizing the view. TODO(kaeser@gubbe.ch): Implement.
 *
 * @param view_ptr
 * @param maximize
 */
void wlmaker_xwl_surface_set_maximized(
    wlmaker_view_t *view_ptr,
    bool maximize)
{
    bs_log(BS_WARNING, "Not implemented: set_maximized - view %p, %d",
           view_ptr, maximize);
}

/* ------------------------------------------------------------------------- */
/**
 * Method for the view going fullscreen. TODO(kaeser@gubbe.ch): Implement.
 *
 * @param view_ptr
 * @param fullscreen
 */
void wlmaker_xwl_surface_set_fullscreen(
    wlmaker_view_t *view_ptr,
    bool fullscreen)
{
    bs_log(BS_WARNING, "Not implemented: set_fullscreen - view %p, %d",
           view_ptr, fullscreen);
}

/* ------------------------------------------------------------------------- */
/**
 * Method for the view being closed.
 *
 * @param view_ptr
 */
void wlmaker_xwl_surface_send_close_callback(wlmaker_view_t *view_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        view_ptr, wlmaker_xwl_surface_t, view);
    wlr_xwayland_surface_close(xwl_surface_ptr->wlr_xwayland_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal of the surface. Will map to workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, surface_map_listener);
    bs_log(BS_INFO, "Map %p - decorations %x, modal %d, parent %p",
           xwl_surface_ptr,
           xwl_surface_ptr->wlr_xwayland_surface_ptr->decorations,
           xwl_surface_ptr->wlr_xwayland_surface_ptr->modal,
           xwl_surface_ptr->wlr_xwayland_surface_ptr->parent);

    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        // TODO(kaeser@gubbe.ch): This is ... ugly. Refactor. Should be handled
        // by nature of being a toplevel (~= view) or non-toplevel window.
        return;
    }

    wlr_scene_node_set_enabled(
        &xwl_surface_ptr->wlr_scene_tree_ptr->node, true);

    wlmaker_view_map(
        &xwl_surface_ptr->view,
        wlmaker_server_get_current_workspace(xwl_surface_ptr->view.server_ptr),
        WLMAKER_WORKSPACE_LAYER_SHELL);

    wlmaker_workspace_raise_view(
        xwl_surface_ptr->view.workspace_ptr,
        &xwl_surface_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal of the surface. Will unmap from workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, surface_unmap_listener);
    bs_log(BS_INFO, "Unmap %p", xwl_surface_ptr);

    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        // TODO(kaeser@gubbe.ch): This is ... ugly. Refactor. Should be handled
        // by nature of being a toplevel (~= view) or non-toplevel window.
        return;
    }

    wlr_scene_node_set_enabled(
        &xwl_surface_ptr->wlr_scene_tree_ptr->node, false);

    wlmaker_view_unmap(&xwl_surface_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Helper: Returns the XWayland window parent's `struct wlr_scene_tree`, or
 * a pointer to the void_wlr_scene_tree if not available.
 *
 * @param xwl_surface_ptr
 *
 * @return Pointer to the `struct wlr_scene_tree`.
 */
struct wlr_scene_tree *get_parent_wlr_scene_tree_ptr(
    wlmaker_xwl_surface_t *xwl_surface_ptr)
{
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr = NULL;
    if (NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent) {
        BS_ASSERT(NULL != xwl_surface_ptr->wlr_xwayland_surface_ptr->parent->data);
        wlmaker_xwl_surface_t *parent_xwl_surface_ptr =
            xwl_surface_ptr->wlr_xwayland_surface_ptr->parent->data;
        parent_wlr_scene_tree_ptr = parent_xwl_surface_ptr->wlr_scene_tree_ptr;
    } else {
        parent_wlr_scene_tree_ptr =
            &xwl_surface_ptr->xwl_ptr->server_ptr->void_wlr_scene_ptr->tree;
    }
    BS_ASSERT(NULL != parent_wlr_scene_tree_ptr);
    BS_ASSERT(xwl_surface_ptr->wlr_scene_tree_ptr != parent_wlr_scene_tree_ptr);

    return parent_wlr_scene_tree_ptr;
}

/* == End of xwl.c ========================================================= */
