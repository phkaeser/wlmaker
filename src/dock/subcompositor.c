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

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <libbase/libbase.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <toolkit/toolkit.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "input/manager.h"

/* == Declarations ========================================================= */

/** Subcompositor state. */
struct _wlmdock_subcompositor_t {
    /** Top-level container. Attached to `wlr_scene_ptr->tree`. */
    wlmtk_container_t         container;
    /** Virtual methods of `container`'s element superclass. */
    wlmtk_element_vmt_t       orig_element_vmt;

    /** The client providing the layer shell surface. */
    wlmcl_client_t            *client_ptr;
    /** Layer shell surface on parent compositor. */
    wlmcl_layer_surface_t     *layer_surface_ptr;
    /** The toolkit container this surface will display. */
    wlmtk_container_t         *container_ptr;

    /** Output layout for this surface's compositor. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** wlroots output for this surface's compositor. */
    struct wlr_output         *wlr_output_ptr;
    /** Input seat for routing events. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The local scene graph. */
    struct wlr_scene          *wlr_scene_ptr;
    /** The scene-graph output for `wlr_scene_ptr` on `wlr_output_ptr`. */
    struct wlr_scene_output   *wlr_scene_output_ptr;

    /** Local input manager. */
    wlmim_t                   *input_manager_ptr;
    /** Root, wraps @ref wlmdock_subcompositor_t::container's super element. */
    wlmtk_root_t              *root_ptr;

    /** Listener for @ref wlmcl_client_events::keymap. */
    struct wl_listener        wlclient_keymap_listener;
    /** Listener for @ref wlmcl_client_events::keyboard_repeat_info. */
    struct wl_listener        wlclient_keyboard_repeat_info_listener;

    /** For `events.frame` of @ref wlmdock_subcompositor_t::wlr_output_ptr. */
    struct wl_listener        output_frame_listener;
    /** For @ref wlmtk_container_t::events for layout_invalidated. */
    struct wl_listener        container_layout_invalidated_listener;

    /** Requested width of the layout's surface, in pixels. */
    int                       requested_width;
    /** Requested height of the layout's surface, in pixels. */
    int                       requested_height;

    /** The configured width of the layou't surface, in pixels. */
    int                       configured_width;
    /** The configured height of the layou't surface, in pixels. */
    int                       configured_height;
};

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

static void _wlmdock_subcompositor_handle_output_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmdock_subcompositor_handle_container_layout_invalidated(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmdock_subcompositor_handle_layer_surface_configure(
    void *userdata_ptr,
    uint32_t width,
    uint32_t height);
static void _wlmdock_subcompositor_commit(
    wlmdock_subcompositor_t *subcompositor_ptr);

/* == Data ================================================================= */

/** The subcompositor's element virtual method table. */
static const wlmtk_element_vmt_t _wlmdock_subcompositor_element_vmt = {
    .layout = _wlmdock_subcompositor_element_layout
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmdock_subcompositor_t *wlmdock_subcompositor_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    wlmcl_client_t *client_ptr,
    wlmcl_layer_surface_t *layer_surface_ptr,
    struct wlmim_cursor_style *cursor_style_ptr,
    wlmtk_container_t *container_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = logged_calloc(
        1, sizeof(*subcompositor_ptr));
    if (NULL == subcompositor_ptr) return NULL;
    subcompositor_ptr->layer_surface_ptr = BS_ASSERT_NOTNULL(layer_surface_ptr);

    subcompositor_ptr->client_ptr = BS_ASSERT_NOTNULL(client_ptr);
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->keymap,
        &subcompositor_ptr->wlclient_keymap_listener,
        _wlmdock_subcompositor_handle_wlclient_keymap);
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(subcompositor_ptr->client_ptr)->keyboard_repeat_info,
        &subcompositor_ptr->wlclient_keyboard_repeat_info_listener,
        _wlmdock_subcompositor_handle_wlclient_keyboard_repeat_info);

    subcompositor_ptr->wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);
    if (NULL == subcompositor_ptr->wlr_output_layout_ptr) {
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    subcompositor_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == subcompositor_ptr->wlr_scene_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_create()");
        wlmdock_subcompositor_destroy(subcompositor_ptr);
        return NULL;
    }

    if (NULL == wlr_scene_attach_output_layout(
            subcompositor_ptr->wlr_scene_ptr,
            subcompositor_ptr->wlr_output_layout_ptr)) {
        // Note: The `struct wlr_scene_output_layout` is destroyed when the
        // scene or output layout gets destroyed.
        bs_log(BS_ERROR, "Failed wlr_scene_attach_output_layout(%p, %p)",
               subcompositor_ptr->wlr_scene_ptr,
               subcompositor_ptr->wlr_output_layout_ptr);
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

    wlmcl_layer_surface_register_configure_callback(
        subcompositor_ptr->layer_surface_ptr,
        _wlmdock_subcompositor_handle_layer_surface_configure,
        subcompositor_ptr);
    return subcompositor_ptr;
}

/* ------------------------------------------------------------------------- */
bool wlmdock_subcompositor_start(
    wlmdock_subcompositor_t *subcompositor_ptr,
    struct wlr_backend *wlr_backend_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr)
{
    subcompositor_ptr->wlr_output_ptr = wlr_wl_output_create_from_surface(
        wlr_backend_ptr,
        wlmcl_layer_surface_wl_surface(subcompositor_ptr->layer_surface_ptr));
    if (NULL == subcompositor_ptr->wlr_output_ptr) return false;
    wlmtk_util_connect_listener_signal(
        &subcompositor_ptr->wlr_output_ptr->events.frame,
        &subcompositor_ptr->output_frame_listener,
        _wlmdock_subcompositor_handle_output_frame);

    if (!wlr_output_init_render(
            subcompositor_ptr->wlr_output_ptr,
            wlr_allocator_ptr,
            wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer(%p, %p, %p)",
               subcompositor_ptr->wlr_output_ptr,
               wlr_allocator_ptr,
               wlr_renderer_ptr);
        return false;
    }
    wlr_output_layout_add_auto(
        subcompositor_ptr->wlr_output_layout_ptr,
        subcompositor_ptr->wlr_output_ptr);

    subcompositor_ptr->wlr_scene_output_ptr = wlr_scene_output_create(
        subcompositor_ptr->wlr_scene_ptr,
        subcompositor_ptr->wlr_output_ptr);
    if (NULL == subcompositor_ptr->wlr_scene_output_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_output_create(%p, %p)",
               subcompositor_ptr->wlr_scene_ptr,
               subcompositor_ptr->wlr_output_ptr);
        return false;
    }

    _wlmdock_subcompositor_request_size(subcompositor_ptr);
    _wlmdock_subcompositor_commit(subcompositor_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmdock_subcompositor_destroy(wlmdock_subcompositor_t *subcompositor_ptr)
{
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

    wlmtk_util_disconnect_listener(&subcompositor_ptr->output_frame_listener);
    if (NULL != subcompositor_ptr->wlr_scene_output_ptr) {
        wlr_scene_output_destroy(subcompositor_ptr->wlr_scene_output_ptr);
        subcompositor_ptr->wlr_scene_output_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->root_ptr) {
        wlmtk_root_destroy(subcompositor_ptr->root_ptr);
        subcompositor_ptr->root_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->wlr_scene_ptr) {
        wlr_scene_node_destroy(&subcompositor_ptr->wlr_scene_ptr->tree.node);
        subcompositor_ptr->wlr_scene_ptr = NULL;
    }

    if (NULL != subcompositor_ptr->wlr_output_ptr) {
        wlr_output_destroy(subcompositor_ptr->wlr_output_ptr);
        subcompositor_ptr->wlr_output_ptr = NULL;
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
/** Checks the element's size and requests updated size, if needed. */
void _wlmdock_subcompositor_request_size(
    wlmdock_subcompositor_t *subcompositor_ptr)
{
    if (NULL == subcompositor_ptr->container_ptr) return;

    // Retrieve needed size. Only request an update, if it changed.
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        &subcompositor_ptr->container_ptr->super_element);
    if (subcompositor_ptr->requested_width == box.width &&
        subcompositor_ptr->requested_height == box.height) return;

    subcompositor_ptr->requested_width = box.width;
    subcompositor_ptr->requested_height = box.height;
    if (0 >= box.width || 0 >= box.height) {
        // Disable the output, if no more size available.
        if (NULL != subcompositor_ptr->wlr_output_ptr) {
            struct wlr_output_state state;
            wlr_output_state_init(&state);
            wlr_output_state_set_enabled(&state, false);
            if (!wlr_output_commit_state(subcompositor_ptr->wlr_output_ptr,
                                         &state)) {
                bs_log(BS_WARNING, "Failed wlr_output_commit_state(%p, %p)",
                       subcompositor_ptr->wlr_output_ptr,
                       &state);
            }
            wlr_output_state_finish(&state);
            // Unclear: Do we need to also attach a NULL buffer to the surface
            // and commit?
        }
        return;
    }

    zwlr_layer_surface_v1_set_size(
        wlmcl_layer_surface_wlr_layer_surface(
            subcompositor_ptr->layer_surface_ptr),
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
/** Commits the frame. */
void _wlmdock_subcompositor_handle_output_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmdock_subcompositor_t *subcompositor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_subcompositor_t, output_frame_listener);

    if (!wlr_scene_output_commit(subcompositor_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "wlr_scene_output_commit(%p, NULL) failed.",
               subcompositor_ptr->wlr_scene_output_ptr);
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(
        subcompositor_ptr->wlr_scene_output_ptr, &now);
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
/** Handles server-side `configure` request. */
void _wlmdock_subcompositor_handle_layer_surface_configure(
    void *userdata_ptr,
    uint32_t width,
    uint32_t height)
{
    wlmdock_subcompositor_t *subcompositor_ptr = userdata_ptr;

    subcompositor_ptr->configured_width = width;
    subcompositor_ptr->configured_height = height;
    _wlmdock_subcompositor_commit(subcompositor_ptr);
}

/* ------------------------------------------------------------------------- */
/** Commits output dimensions and scene graph. */
void _wlmdock_subcompositor_commit(wlmdock_subcompositor_t *subcompositor_ptr)
{
    if (NULL == subcompositor_ptr->wlr_output_ptr ||
        NULL == subcompositor_ptr->wlr_scene_output_ptr) return;

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(
        &state,
        subcompositor_ptr->configured_width != 0 &&
        subcompositor_ptr->configured_height != 0);
    wlr_output_state_set_custom_mode(
        &state,
        subcompositor_ptr->configured_width,
        subcompositor_ptr->configured_height,
        0);
    if (!wlr_output_commit_state(subcompositor_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_WARNING, "Failed wlr_output_commit_state(%p, %p) for %d x %d",
               subcompositor_ptr->wlr_output_ptr,
               &state,
               subcompositor_ptr->configured_width,
               subcompositor_ptr->configured_height);
    }
    wlr_output_state_finish(&state);

    if (!wlr_scene_output_commit(subcompositor_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "Failed wlr_scene_output_commit(%p, NULL)",
               subcompositor_ptr->wlr_scene_output_ptr);
    }
}

/* == Unit Tests =========================================================== */

/* == End of subcompositor.c =============================================== */
