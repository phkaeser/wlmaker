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
    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

    /** XWayland server and XWM. */
    struct wlr_xwayland       *wlr_xwayland_ptr;

    /** Listener for the `new_surface` signal raised by `wlr_xwayland`. */
    struct wl_listener        new_surface_listener;
};

/** A scene-graph API subtree for a wlroots XWayland surface. */
typedef struct {
    /** Scene tree holding the XWayland surface. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** The wlroots XWayland surface that we're wrapping here. */
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr;
    /** Holds the wlroots surface. Element of `wlr_scene_tree_ptr`. */
    struct wlr_scene_surface  *wlr_scene_surface_ptr;

    /** Listener: `destroy` event of `wlr_scene_tree_ptr`. */
    struct wl_listener        tree_destroy_listener;

    /** Listener: `map` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener: `unmap` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_unmap_listener;
    /** Listener: `destroy` event of `wlr_xwayland_surface_ptr->surface`. */
    struct wl_listener        surface_destroy_listener;
} wlmaker_scene_xwayland_surface_t;

static struct wlr_scene_tree *wlmaker_scene_xwayland_surface_create(
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr);
static void xwls_destroy(
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr);
static void xwls_handle_tree_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
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

    /** The surface's tree, once associated. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;

    /** Listener for the `destroy` signal of `wlr_xwayland_surface`. */
    struct wl_listener        destroy_listener;
    /** Listener for `request_configure` signal of `wlr_xwayland_surface`. */
    struct wl_listener        request_configure_listener;

    /** Listener for the `associate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        associate_listener;
    /** Listener for the `dissociate` signal of `wlr_xwayland_surface`. */
    struct wl_listener        dissociate_listener;

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

static void handle_destroy(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_request_configure(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_associate(struct wl_listener *listener_ptr, void *data_ptr);
static void handle_dissociate(struct wl_listener *listener_ptr, void *data_ptr);

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
 * @param parent_wlr_scene_tree_ptr Scene graph tree this should become
 *     attached to.
 * @param wlr_xwayland_surface_ptr The wlroots XWayland surface to create the
 *     node for.
 *
 * @return A scene graph API node.
 */
struct wlr_scene_tree *wlmaker_scene_xwayland_surface_create(
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        logged_calloc(1, sizeof(wlmaker_scene_xwayland_surface_t));
    if (NULL == scene_xwayland_surface_ptr) return NULL;
    scene_xwayland_surface_ptr->wlr_xwayland_surface_ptr =
        wlr_xwayland_surface_ptr;

    scene_xwayland_surface_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        parent_wlr_scene_tree_ptr);
    if (NULL == scene_xwayland_surface_ptr->wlr_scene_tree_ptr) {
        xwls_destroy(scene_xwayland_surface_ptr);
        return NULL;
    }

    wlm_util_connect_listener_signal(
        &scene_xwayland_surface_ptr->wlr_scene_tree_ptr->node.events.destroy,
        &scene_xwayland_surface_ptr->tree_destroy_listener,
        xwls_handle_tree_destroy);
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
            scene_xwayland_surface_ptr->wlr_scene_tree_ptr,
            wlr_xwayland_surface_ptr->surface);
    if (NULL == scene_xwayland_surface_ptr->wlr_scene_surface_ptr) {
        xwls_destroy(scene_xwayland_surface_ptr);
        return NULL;
    }

    return scene_xwayland_surface_ptr->wlr_scene_tree_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor for the xwayland surface scene node.
 *
 * @param scene_xwayland_surface_ptr
 */
void xwls_destroy(wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr)
{
    if (NULL != scene_xwayland_surface_ptr->wlr_scene_surface_ptr) {
        wlr_scene_node_destroy(
            &scene_xwayland_surface_ptr->wlr_scene_surface_ptr->buffer->node);
        scene_xwayland_surface_ptr->wlr_scene_surface_ptr = NULL;
    }

    wl_list_remove(&scene_xwayland_surface_ptr->surface_destroy_listener.link);
    wl_list_remove(&scene_xwayland_surface_ptr->surface_unmap_listener.link);
    wl_list_remove(&scene_xwayland_surface_ptr->surface_map_listener.link);
    wl_list_remove(&scene_xwayland_surface_ptr->tree_destroy_listener.link);

    if (NULL != scene_xwayland_surface_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(
            &scene_xwayland_surface_ptr->wlr_scene_tree_ptr->node);
        scene_xwayland_surface_ptr->wlr_scene_tree_ptr = NULL;
    }
    free(scene_xwayland_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the tree. Will clean up.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void xwls_handle_tree_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_scene_xwayland_surface_t *scene_xwayland_surface_ptr =
        BS_CONTAINER_OF(listener_ptr, wlmaker_scene_xwayland_surface_t,
                        tree_destroy_listener);
    // Prevent double-deletes.
    scene_xwayland_surface_ptr->wlr_scene_tree_ptr = NULL;
    xwls_destroy(scene_xwayland_surface_ptr);
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
        &scene_xwayland_surface_ptr->wlr_scene_tree_ptr->node, true);
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
        &scene_xwayland_surface_ptr->wlr_scene_tree_ptr->node, false);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the surface. Will destroy the node.
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
    wlr_scene_node_destroy(
        &scene_xwayland_surface_ptr->wlr_scene_tree_ptr->node);
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

    bs_log(BS_INFO, "Reqeust configure for %p: "
           "%"PRId16" x %"PRIx16" size %"PRIu16" x %"PRIu16" mask 0x%"PRIx16,
           xwl_surface_ptr,
           cfg_event_ptr->x, cfg_event_ptr->y,
           cfg_event_ptr->width, cfg_event_ptr->height,
           cfg_event_ptr->mask);

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
void handle_associate(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_surface_t *xwl_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_surface_t, associate_listener);
    bs_log(BS_INFO, "Associate %p", xwl_surface_ptr);

    BS_ASSERT(NULL == xwl_surface_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(!xwl_surface_ptr->view_initialized);

    xwl_surface_ptr->wlr_scene_tree_ptr = wlmaker_scene_xwayland_surface_create(
        &xwl_surface_ptr->xwl_ptr->server_ptr->void_wlr_scene_ptr->tree,
        xwl_surface_ptr->wlr_xwayland_surface_ptr);
    if (NULL == xwl_surface_ptr->wlr_scene_tree_ptr) {
        bs_log(BS_FATAL, "Failed wlmaker_scene_xwayland_surface_create(%p, %p)",
               &xwl_surface_ptr->xwl_ptr->server_ptr->void_wlr_scene_ptr->tree,
               xwl_surface_ptr->wlr_xwayland_surface_ptr);
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

    wlmaker_view_init(
        &xwl_surface_ptr->view,
        &xwl_surface_view_impl,
        xwl_surface_ptr->xwl_ptr->server_ptr,
        xwl_surface_ptr->wlr_xwayland_surface_ptr->surface,
        xwl_surface_ptr->wlr_scene_tree_ptr,
        wlmaker_xwl_surface_send_close_callback);
    xwl_surface_ptr->view_initialized = true;
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

    wlmaker_view_fini(&xwl_surface_ptr->view);
    xwl_surface_ptr->view_initialized = false;

    wl_list_remove(&xwl_surface_ptr->surface_unmap_listener.link);
    wl_list_remove(&xwl_surface_ptr->surface_map_listener.link);

    // Note: xwl_surface_ptr->wlr_scene_tree_ptr not destroyed here, it is
    // cleaned up via the scene tree it was attached to.
    xwl_surface_ptr->wlr_scene_tree_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Method for view activation. TODO(kaeser@gubbe.ch): Implement.
 *
 * @param view_ptr
 * @param activated
 *
 * @return A serial.
 */
uint32_t wlmaker_xwl_surface_set_activated(
    wlmaker_view_t *view_ptr,
    bool activated)
{
    bs_log(BS_WARNING, "Not implemented: set_activated - view %p, %d",
           view_ptr, activated);
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Method for getting the view's size. TODO(kaeser@gubbe.ch): Implement.
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
    bs_log(BS_WARNING, "Not implemented: get_size - view %p", view_ptr);
    *width_ptr = 128;
    *height_ptr = 64;
}

/* ------------------------------------------------------------------------- */
/**
 * Method for setting the view's size. TODO(kaeser@gubbe.ch): Implement.
 *
 * @param view_ptr
 * @param width
 * @param height
 */
void wlmaker_xwl_surface_set_size(
    wlmaker_view_t *view_ptr,
    int width, int height)
{
    bs_log(BS_WARNING, "Not implemented: set_size - view %p, %d x %d",
           view_ptr, width, height);
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
 * Method for the view being closed. TODO(kaeser@gubbe.ch): Implement.
 *
 * @param view_ptr
 */
void wlmaker_xwl_surface_send_close_callback(wlmaker_view_t *view_ptr)
{
    bs_log(BS_WARNING, "Not implemented: send close - view %p", view_ptr);
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
    bs_log(BS_INFO, "Map %p", xwl_surface_ptr);

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

    wlmaker_view_unmap(&xwl_surface_ptr->view);
}

/* == End of xwl.c ========================================================= */
