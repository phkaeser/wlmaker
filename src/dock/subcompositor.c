/* ========================================================================= */
/**
 * @file subcompositor.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "subcompositor.h"

#include <stdbool.h>
#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/signal.h>
#include <stdlib.h>
#include <toolkit/toolkit.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

#include "scene_surface.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "input/manager.h"

struct zwlr_layer_surface_v1;

/* == Declarations ========================================================= */

/** Subcompositor state. */
struct _wlmdock_subcompositor_t {
    /** Top-level container. Attached to `wlr_scene_ptr->tree`. */
    wlmtk_container_t         container;
    /** Virtual methods of `container`'s element superclass. */
    wlmtk_element_vmt_t       orig_element_vmt;

    /** The client providing the layer shell surface. */
    wlmcl_client_t            *client_ptr;
    /** Layer shell interface, from parent. */
    struct zwlr_layer_shell_v1 *zwlr_layer_shell_ptr;
    /** The positioning data. */
    wlmtk_dock_positioning_t  positioning;

    /** The wayland display. */
    struct wl_display *wl_display_ptr;
    /** The wlroots backend. */
    struct wlr_backend *wlr_backend_ptr;
    /** The wlroots allocator. */
    struct wlr_allocator *wlr_allocator_ptr;
    /** The wlroots renderer. */
    struct wlr_renderer *wlr_renderer_ptr;

    /** Wayland surface backing the layer surface. */
    struct wl_surface         *wl_surface_ptr;
    /** Layer shell surface on the parent compositor. */
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr;
    /** The toolkit container this surface will display. */
    wlmtk_container_t         *container_ptr;

    /** Listens to @ref wlmcl_client_events::outputs_changed. */
    struct bs_listener        outputs_changed_listener;

    /** Input seat for routing events. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The local scene graph. */
    struct wlr_scene          *wlr_scene_ptr;

    /** Output layout for this surface's compositor. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** The scene surface. */
    struct wlmdock_scene_surface *scene_surface_ptr;

    /** Local input manager. */
    wlmim_t                   *input_manager_ptr;
    /** Root, wraps @ref wlmdock_subcompositor_t::container's super element. */
    wlmtk_root_t              *root_ptr;

    /** Listener for @ref wlmcl_client_events::keymap. */
    struct wl_listener        wlclient_keymap_listener;
    /** Listener for @ref wlmcl_client_events::keyboard_repeat_info. */
    struct wl_listener        wlclient_keyboard_repeat_info_listener;

    /** For @ref wlmtk_container_t::events for layout_invalidated. */
    struct wl_listener        container_layout_invalidated_listener;
};

static bool _wlmdock_subcompositor_start(
    wlmdock_subcompositor_t *subcompositor_ptr);
static void _wlmdock_subcompositor_stop(
    wlmdock_subcompositor_t *subcompositor_ptr);

static void _wlmdock_subcompositor_request_size(
    wlmdock_subcompositor_t *subcompositor_ptr);
static void _wlmdock_subcompositor_element_layout(
    wlmtk_element_t *element_ptr);

static void _wlmdock_subcompositor_handle_wlclient_keymap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmdock_subcompositor_handle_wlclient_keyboard_repeat_info(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmdock_subcompositor_handle_container_layout_invalidated(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmdock_subcompositor_layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr,
    uint32_t serial,
    uint32_t width,
    uint32_t height);
static void _wlmdock_subcompositor_layer_surface_handle_closed(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr);

static void _wlmdock_subcompositor_handle_outputs_changed(
    struct bs_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** The subcompositor's element virtual method table. */
static const wlmtk_element_vmt_t _wlmdock_subcompositor_element_vmt = {
    .layout = _wlmdock_subcompositor_element_layout
};

/** Listeners for the layer surface. */
static const struct zwlr_layer_surface_v1_listener
_wlmdock_subcompositor_layer_surface_listener = {
    .configure = _wlmdock_subcompositor_layer_surface_handle_configure,
    .closed = _wlmdock_subcompositor_layer_surface_handle_closed,
};


/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmdock_subcompositor_t *wlmdock_subcompositor_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    wlmcl_client_t *client_ptr,
    struct zwlr_layer_shell_v1 *zwlr_layer_shell_ptr,
    struct wlmim_cursor_style *cursor_style_ptr,
    wlmtk_container_t *container_ptr,
    const wlmtk_dock_positioning_t *positioning_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = logged_calloc(
        1, sizeof(*subcompositor_ptr));
    if (NULL == subcompositor_ptr) return NULL;
    subcompositor_ptr->wl_display_ptr = wl_display_ptr;
    subcompositor_ptr->zwlr_layer_shell_ptr = zwlr_layer_shell_ptr;
    subcompositor_ptr->positioning = *positioning_ptr;

    subcompositor_ptr->client_ptr = BS_ASSERT_NOTNULL(client_ptr);
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->keymap,
        &subcompositor_ptr->wlclient_keymap_listener,
        _wlmdock_subcompositor_handle_wlclient_keymap);
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->keyboard_repeat_info,
        &subcompositor_ptr->wlclient_keyboard_repeat_info_listener,
        _wlmdock_subcompositor_handle_wlclient_keyboard_repeat_info);

    subcompositor_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == subcompositor_ptr->wlr_scene_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_create()");
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    if (!wlmtk_container_init_attached(
            &subcompositor_ptr->container,
            &subcompositor_ptr->wlr_scene_ptr->tree)) {
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }
    subcompositor_ptr->orig_element_vmt = wlmtk_element_extend(
        &subcompositor_ptr->container.super_element,
        &_wlmdock_subcompositor_element_vmt);
    wlmtk_element_set_visible(
        &subcompositor_ptr->container.super_element,
        true);
    wlmtk_container_add_element(&subcompositor_ptr->container,
                                &container_ptr->super_element);
    wlmtk_util_connect_listener_signal(
        &container_ptr->events.layout_invalidated,
        &subcompositor_ptr->container_layout_invalidated_listener,
        _wlmdock_subcompositor_handle_container_layout_invalidated);
    subcompositor_ptr->container_ptr = container_ptr;

    subcompositor_ptr->wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);
    if (NULL == subcompositor_ptr->wlr_output_layout_ptr) {
        bs_log(BS_ERROR, "Failed wlr_output_layout_create(%p)",
               wl_display_ptr);
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }


    subcompositor_ptr->root_ptr = wlmtk_root_create(
        wlmdock_subcompositor_element(subcompositor_ptr),
        subcompositor_ptr->wlr_output_layout_ptr);
    if (NULL == subcompositor_ptr->root_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_root_create(%p, %p)",
               wlmdock_subcompositor_element(subcompositor_ptr),
               subcompositor_ptr->wlr_output_layout_ptr);
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    subcompositor_ptr->wlr_seat_ptr = wlr_seat_create(
        wl_display_ptr, "wlmdock");
    if (NULL == subcompositor_ptr->wlr_seat_ptr) {
        bs_log(BS_ERROR, "Failed wlr_seat_create(%p, \"wlmdock\")",
               wl_display_ptr);
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    subcompositor_ptr->input_manager_ptr = wlmim_input_manager_create(
        wl_display_ptr,
        wlr_backend_ptr,
        subcompositor_ptr->wlr_output_layout_ptr,
        subcompositor_ptr->wlr_seat_ptr,
        NULL,  // No config dict.
        cursor_style_ptr,
        subcompositor_ptr->root_ptr);
    if (NULL == subcompositor_ptr->input_manager_ptr) {
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    bs_listener_connect(
        &subcompositor_ptr->outputs_changed_listener,
        _wlmdock_subcompositor_handle_outputs_changed,
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->outputs_changed);
    _wlmdock_subcompositor_handle_outputs_changed(
        &subcompositor_ptr->outputs_changed_listener,
        wlmcl_client_outputs(subcompositor_ptr->client_ptr));

    return subcompositor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_subcompositor_prepare(
    wlmdock_subcompositor_t *subcompositor_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr)
{
    subcompositor_ptr->wlr_backend_ptr = wlr_backend_ptr;
    subcompositor_ptr->wlr_allocator_ptr = wlr_allocator_ptr;
    subcompositor_ptr->wlr_renderer_ptr = wlr_renderer_ptr;

    _wlmdock_subcompositor_handle_outputs_changed(
        &subcompositor_ptr->outputs_changed_listener,
        wlmcl_client_outputs(subcompositor_ptr->client_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmdock_subcompositor_destroy(wlmdock_subcompositor_t *subcompositor_ptr)
{
    _wlmdock_subcompositor_stop(subcompositor_ptr);

    bs_listener_disconnect(
        &subcompositor_ptr->outputs_changed_listener,
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->outputs_changed);

    if (NULL != subcompositor_ptr->input_manager_ptr) {
        wlmim_input_manager_destroy(subcompositor_ptr->input_manager_ptr);
        subcompositor_ptr->input_manager_ptr = NULL;
    }

    if (NULL == subcompositor_ptr->wlr_seat_ptr) {
        wlr_seat_destroy(subcompositor_ptr->wlr_seat_ptr);
        subcompositor_ptr->wlr_seat_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->container_ptr) {
        wlmtk_util_disconnect_listener(
            &subcompositor_ptr->container_layout_invalidated_listener);
        wlmtk_container_remove_element(
            &subcompositor_ptr->container,
            &subcompositor_ptr->container_ptr->super_element);
        subcompositor_ptr->container_ptr = NULL;
    }
    wlmtk_container_fini(&subcompositor_ptr->container);

    if (NULL != subcompositor_ptr->root_ptr) {
        wlmtk_root_destroy(subcompositor_ptr->root_ptr);
        subcompositor_ptr->root_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->wlr_scene_ptr) {
        wlr_scene_node_destroy(&subcompositor_ptr->wlr_scene_ptr->tree.node);
        subcompositor_ptr->wlr_scene_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->wlr_output_layout_ptr) {
        wlr_output_layout_destroy(subcompositor_ptr->wlr_output_layout_ptr);
        subcompositor_ptr->wlr_output_layout_ptr = NULL;
    }

    wlmtk_util_disconnect_listener(
        &subcompositor_ptr->wlclient_keymap_listener);
    wlmtk_util_disconnect_listener(
        &subcompositor_ptr->wlclient_keyboard_repeat_info_listener);

    free(subcompositor_ptr);
}


/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmdock_subcompositor_element(
    wlmdock_subcompositor_t *subcompositor_ptr)
{
    return &subcompositor_ptr->container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Creates the surface and starts the subcompositor. */
bool _wlmdock_subcompositor_start(wlmdock_subcompositor_t *subcompositor_ptr)
{
    subcompositor_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlmcl_client_attributes(subcompositor_ptr->client_ptr
            )->wl_compositor_ptr);
    if (NULL == subcompositor_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p)",
               wlmcl_client_attributes(subcompositor_ptr->client_ptr
                   )->wl_compositor_ptr);
        _wlmdock_subcompositor_stop(subcompositor_ptr);
        return false;
    }
    subcompositor_ptr->zwlr_layer_surface_ptr =
        zwlr_layer_shell_v1_get_layer_surface(
            subcompositor_ptr->zwlr_layer_shell_ptr,
            subcompositor_ptr->wl_surface_ptr,
            NULL,  // Let compositor choose output.
            ZWLR_LAYER_SHELL_V1_LAYER_TOP,
            "wlmdock");
    if (NULL == subcompositor_ptr->zwlr_layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed zwlr_layer_shell_v1_get_layer_surface(...)");
        _wlmdock_subcompositor_stop(subcompositor_ptr);
        return false;
    }

    // Configure.
    zwlr_layer_surface_v1_set_size(
        subcompositor_ptr->zwlr_layer_surface_ptr, 64, 64);
    zwlr_layer_surface_v1_set_anchor(
        subcompositor_ptr->zwlr_layer_surface_ptr,
        subcompositor_ptr->positioning.anchor |
        subcompositor_ptr->positioning.edge);
    zwlr_layer_surface_v1_set_exclusive_zone(
        subcompositor_ptr->zwlr_layer_surface_ptr, 64);
    zwlr_layer_surface_v1_set_exclusive_edge(
        subcompositor_ptr->zwlr_layer_surface_ptr,
        subcompositor_ptr->positioning.edge);
    wl_surface_commit(subcompositor_ptr->wl_surface_ptr);

    if (0 != zwlr_layer_surface_v1_add_listener(
            subcompositor_ptr->zwlr_layer_surface_ptr,
            &_wlmdock_subcompositor_layer_surface_listener,
            subcompositor_ptr)) {
        bs_log(BS_ERROR, "Failed zwlr_layer_surface_v1_add_listener.");
        _wlmdock_subcompositor_stop(subcompositor_ptr);
        return false;
    }

    subcompositor_ptr->scene_surface_ptr = wlmdock_scene_surface_create(
        subcompositor_ptr->wlr_output_layout_ptr,
        subcompositor_ptr->wlr_backend_ptr,
        subcompositor_ptr->wlr_allocator_ptr,
        subcompositor_ptr->wlr_renderer_ptr,
        subcompositor_ptr->wlr_scene_ptr,
        subcompositor_ptr->wl_surface_ptr);
    if (NULL == subcompositor_ptr->scene_surface_ptr) {
        _wlmdock_subcompositor_stop(subcompositor_ptr);
        return false;
    }

    _wlmdock_subcompositor_request_size(subcompositor_ptr);
    wlmdock_scene_surface_commit(subcompositor_ptr->scene_surface_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Stops the subcompositor. */
void _wlmdock_subcompositor_stop(wlmdock_subcompositor_t *subcompositor_ptr)
{
    if (NULL != subcompositor_ptr->scene_surface_ptr) {
        wlmdock_scene_surface_destroy(subcompositor_ptr->scene_surface_ptr);
        subcompositor_ptr->scene_surface_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->zwlr_layer_surface_ptr) {
        zwlr_layer_surface_v1_destroy(
            subcompositor_ptr->zwlr_layer_surface_ptr);
        subcompositor_ptr->zwlr_layer_surface_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->wl_surface_ptr) {
        wl_surface_destroy(subcompositor_ptr->wl_surface_ptr);
        subcompositor_ptr->wl_surface_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/** Checks the element's size and requests updated size, if needed. */
void _wlmdock_subcompositor_request_size(
    wlmdock_subcompositor_t *subcompositor_ptr)
{
    if (NULL == subcompositor_ptr->container_ptr) return;

    if (NULL == subcompositor_ptr->scene_surface_ptr) return;

    // Retrieve needed size. Only request an update, if it changed.
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        &subcompositor_ptr->container_ptr->super_element);

    wlmdock_scene_surface_request_size(
        subcompositor_ptr->scene_surface_ptr,
        box.width,
        box.height);

    zwlr_layer_surface_v1_set_size(
        subcompositor_ptr->zwlr_layer_surface_ptr,
        box.width, box.height);
}

/* ------------------------------------------------------------------------- */
/** Updates the subcompositor's surface layout dimensions accordingly. */
void _wlmdock_subcompositor_element_layout(wlmtk_element_t *element_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        element_ptr, wlmdock_subcompositor_t, container.super_element);

    subcompositor_ptr->orig_element_vmt.layout(element_ptr);
    _wlmdock_subcompositor_request_size(subcompositor_ptr);
}

/* ------------------------------------------------------------------------- */
/** Forwards the keymap to the input manager. */
static void _wlmdock_subcompositor_handle_wlclient_keymap(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_subcompositor_t, wlclient_keymap_listener);
    struct xkb_keymap *xkb_keymap_ptr = data_ptr;

    if (NULL == subcompositor_ptr->input_manager_ptr) return;
    wlmim_input_manager_set_keymap(
        subcompositor_ptr->input_manager_ptr,
        xkb_keymap_ptr);
}

/* ------------------------------------------------------------------------- */
/** Forwards the keyboard repeat info to the input manager. */
static void _wlmdock_subcompositor_handle_wlclient_keyboard_repeat_info(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_subcompositor_t,
        wlclient_keyboard_repeat_info_listener);

    if (NULL == subcompositor_ptr->input_manager_ptr) return;
    int32_t repeat, delay;
    if (wlmcl_client_get_repeat_info(
            subcompositor_ptr->client_ptr, &repeat, &delay)) {
        wlmim_input_manager_set_repeat_info(
            subcompositor_ptr->input_manager_ptr, repeat, delay);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles when the tilebox' layout is invalidated. Reconfigure the output. */
void _wlmdock_subcompositor_handle_container_layout_invalidated(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_subcompositor_t,
        container_layout_invalidated_listener);

    _wlmdock_subcompositor_request_size(subcompositor_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the wlr_layer_surface's configure event: ACK and trigger commit. */
void _wlmdock_subcompositor_layer_surface_handle_configure(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr,
    uint32_t serial,
    uint32_t width,
    uint32_t height)
{
    wlmdock_subcompositor_t *subcompositor_ptr = data_ptr;
    bs_log(BS_DEBUG, "Subcompositor %p: layer surface %p configure: %" PRIu32
           "x%" PRIu32 ", serial %" PRIu32,
           subcompositor_ptr,
           zwlr_layer_surface_ptr,
           width, height, serial);

    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_ptr, serial);

    wlmdock_scene_surface_configure(
        subcompositor_ptr->scene_surface_ptr, width, height);
    wlmdock_scene_surface_commit(subcompositor_ptr->scene_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the wlr_layer_surface's closed event: Destroy the surface. */
void _wlmdock_subcompositor_layer_surface_handle_closed(
    void *data_ptr,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_ptr)
{
    __UNUSED__ wlmdock_subcompositor_t *subcompositor_ptr = data_ptr;

    bs_log(BS_INFO, "Subcompositor %p: layer surface %p closed",
           subcompositor_ptr, zwlr_layer_surface_ptr);

    _wlmdock_subcompositor_stop(subcompositor_ptr);
}

/* ------------------------------------------------------------------------- */
/** The outputs have changed. We may need to start/stop the subcompositor. */
void _wlmdock_subcompositor_handle_outputs_changed(
    struct bs_listener *listener_ptr,
    void *data_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_subcompositor_t, outputs_changed_listener);
    bs_dllist_t *outputs_ptr = data_ptr;

    if (NULL == subcompositor_ptr->wlr_backend_ptr) return;

    if (bs_dllist_empty(outputs_ptr)) {
        if (NULL != subcompositor_ptr->wl_surface_ptr) {
            _wlmdock_subcompositor_stop(subcompositor_ptr);
        }
        return;
    }

    if (NULL != subcompositor_ptr->wl_surface_ptr) return;
    _wlmdock_subcompositor_start(subcompositor_ptr);
}

/* == Unit Tests =========================================================== */

/* == End of subcompositor.c =============================================== */
