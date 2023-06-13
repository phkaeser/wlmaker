/* ========================================================================= */
/**
 * @file layer_surface.c
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

#include "layer_surface.h"

#include "util.h"
#include "view.h"
#include "xdg_popup.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of a layer surface handler. */
struct _wlmaker_layer_surface_t {
    /** State of the corresponding view. */
    wlmaker_view_t            view;
    /** The layer this surface is on. */
    wlmaker_workspace_layer_t layer;

    /** Double-linked-list node, for |layer_surfaces| of workspace. */
    bs_dllist_node_t          dlnode;

    /** Links to the corresponding `wlr_layer_surface_v1`. */
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr;
    /** The scene graph for the layer node. */
    struct wlr_scene_layer_surface_v1 *wlr_scene_layer_surface_v1_ptr;

    /** Listener for the `destroy` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for the `map` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        map_listener;
    /** Listener for the `unmap` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        unmap_listener;
    /** Listener for `new_popup` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        new_popup_listener;

    /** Listener for the `commit` signal raised by `wlr_surface`. */
    struct wl_listener        surface_commit_listener;
};

static wlmaker_layer_surface_t *layer_surface_from_view(
    wlmaker_view_t *view_ptr);
static void layer_surface_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr);

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** View implementor methods. */
const wlmaker_view_impl_t     layer_surface_view_impl = {
    .set_activated = NULL,
    .get_size = layer_surface_get_size
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_layer_surface_t *wlmaker_layer_surface_create(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = logged_calloc(
        1, sizeof(wlmaker_layer_surface_t));
    if (NULL == layer_surface_ptr) return NULL;
    layer_surface_ptr->wlr_layer_surface_v1_ptr = wlr_layer_surface_v1_ptr;

    layer_surface_ptr->wlr_scene_layer_surface_v1_ptr =
        wlr_scene_layer_surface_v1_create(
            &server_ptr->void_wlr_scene_ptr->tree,
            wlr_layer_surface_v1_ptr);
    if (NULL == layer_surface_ptr->wlr_scene_layer_surface_v1_ptr) {
        wlmaker_layer_surface_destroy(layer_surface_ptr);
        return NULL;
    }

    wlm_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.destroy,
        &layer_surface_ptr->destroy_listener,
        handle_destroy);
    wlm_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.map,
        &layer_surface_ptr->map_listener,
        handle_map);
    wlm_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.unmap,
        &layer_surface_ptr->unmap_listener,
        handle_unmap);
    wlm_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.new_popup,
        &layer_surface_ptr->new_popup_listener,
        handle_new_popup);

    wlm_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->surface->events.commit,
        &layer_surface_ptr->surface_commit_listener,
        handle_surface_commit);

    wlmaker_view_init(
        &layer_surface_ptr->view,
        &layer_surface_view_impl,
        server_ptr,
        layer_surface_ptr->wlr_layer_surface_v1_ptr->surface,
        layer_surface_ptr->wlr_scene_layer_surface_v1_ptr->tree,
        NULL);  // Send close callback.

    struct wlr_box extents;
    wlr_output_layout_get_box(
        server_ptr->wlr_output_layout_ptr, NULL, &extents);
    wlmaker_layer_surface_configure(
        layer_surface_ptr, &extents, &extents);

    bs_log(BS_INFO, "Created layer surface %p, view %p, wlr_surface %p (res %p)",
           layer_surface_ptr, &layer_surface_ptr->view,
           wlr_layer_surface_v1_ptr->surface,
           wlr_layer_surface_v1_ptr->resource);
    return layer_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_layer_surface_destroy(wlmaker_layer_surface_t *layer_surface_ptr)
{
    wlmaker_view_fini(&layer_surface_ptr->view);

    // There is no 'destroy' method for `wlr_scene_layer_surface_v1`.

    wl_list_remove(&layer_surface_ptr->new_popup_listener.link);
    wl_list_remove(&layer_surface_ptr->unmap_listener.link);
    wl_list_remove(&layer_surface_ptr->map_listener.link);
    wl_list_remove(&layer_surface_ptr->destroy_listener.link);
    free(layer_surface_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_layer_surface_is_exclusive(
    wlmaker_layer_surface_t *layer_surface_ptr)
{
    return 0 != layer_surface_ptr->wlr_layer_surface_v1_ptr->current.exclusive_zone;
}

/* ------------------------------------------------------------------------- */
void wlmaker_layer_surface_configure(
    wlmaker_layer_surface_t *layer_surface_ptr,
    const struct wlr_box *full_area_ptr,
    struct wlr_box *usable_area_ptr)
{
    wlr_scene_layer_surface_v1_configure(
        layer_surface_ptr->wlr_scene_layer_surface_v1_ptr,
        full_area_ptr,
        usable_area_ptr);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_layer_surface(
    wlmaker_layer_surface_t *layer_surface_ptr)
{
    return &layer_surface_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmaker_layer_surface_t *wlmaker_layer_surface_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmaker_layer_surface_t, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Typecast: Retrieves the `wlmaker_layer_surface_t` for the given |view_ptr|.
 *
 * @param view_ptr
 *
 * @return A pointer to the `wlmaker_layer_surface_t` holding |view_ptr|.
 */
wlmaker_layer_surface_t *layer_surface_from_view(
    wlmaker_view_t *view_ptr)
{
    BS_ASSERT(view_ptr->impl_ptr == &layer_surface_view_impl);
    return BS_CONTAINER_OF(view_ptr, wlmaker_layer_surface_t, view);
}

/* ------------------------------------------------------------------------- */
/**
 * Gets the size of the layer surface.
 *
 * @param view_ptr
 * @param width_ptr
 * @param height_ptr
 */
void layer_surface_get_size(
    wlmaker_view_t *view_ptr,
    uint32_t *width_ptr,
    uint32_t *height_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr =
        layer_surface_from_view(view_ptr);
    if (NULL != width_ptr) {
        *width_ptr =
            layer_surface_ptr->wlr_layer_surface_v1_ptr->current.actual_width;
    }
    if (NULL != height_ptr) {
        *height_ptr =
            layer_surface_ptr->wlr_layer_surface_v1_ptr->current.actual_height;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the `wlr_layer_surface_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = wl_container_of(
        listener_ptr, layer_surface_ptr, destroy_listener);

    wlmaker_layer_surface_destroy(layer_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal of the `wlr_layer_surface_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = wl_container_of(
        listener_ptr, layer_surface_ptr, map_listener);
    wlmaker_workspace_layer_t layer;

    switch (layer_surface_ptr->wlr_layer_surface_v1_ptr->current.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        layer = WLMAKER_WORKSPACE_LAYER_BACKGROUND;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        layer = WLMAKER_WORKSPACE_LAYER_BOTTOM;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        layer = WLMAKER_WORKSPACE_LAYER_TOP;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        layer = WLMAKER_WORKSPACE_LAYER_OVERLAY;
        break;
    default:
        bs_log(BS_FATAL, "Unhandled layer %d",
               layer_surface_ptr->wlr_layer_surface_v1_ptr->current.layer);
        BS_ABORT();
    }

    wlmaker_view_map(
        &layer_surface_ptr->view,
        wlmaker_server_get_current_workspace(layer_surface_ptr->view.server_ptr),
        layer);
    layer_surface_ptr->layer = layer;
    wlmaker_workspace_layer_surface_add(
        layer_surface_ptr->view.workspace_ptr,
        layer_surface_ptr->layer,
        layer_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal of the `wlr_layer_surface_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = wl_container_of(
        listener_ptr, layer_surface_ptr, unmap_listener);

    wlmaker_workspace_layer_surface_remove(
        layer_surface_ptr->view.workspace_ptr,
        layer_surface_ptr->layer,
        layer_surface_ptr);
    layer_surface_ptr->layer = WLMAKER_WORKSPACE_LAYER_NUM;  // invalid.
    wlmaker_view_unmap(&layer_surface_ptr->view);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_popup` signal of the `wlr_layer_surface_v1`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a `wlr_xdg_popup`.
 */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = wl_container_of(
        listener_ptr, layer_surface_ptr, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr,
        layer_surface_ptr->wlr_scene_layer_surface_v1_ptr->tree
        //layer_surface_ptr->view.wlr_scene_tree_ptr
        );
    bs_log(BS_INFO, "Created popup %p.", xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal raised by `wlr_surface`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the wlr_surface raising the signla.
 */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_layer_surface_t *layer_surface_ptr = wl_container_of(
        listener_ptr, layer_surface_ptr, surface_commit_listener);
    BS_ASSERT(layer_surface_ptr->wlr_layer_surface_v1_ptr->surface ==
              data_ptr);

    if (layer_surface_ptr->view.workspace_ptr) {
        // TODO(kaeser@gubbe.ch: Calls arranging on every update?
        wlmaker_workspace_arrange_views(layer_surface_ptr->view.workspace_ptr);
    }
}

/* == End of layer_surface.c =============================================== */
