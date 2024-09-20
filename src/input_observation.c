/* ========================================================================= */
/**
 * @file input_observation.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "input_observation.h"

#include "input-observation-v1-server-protocol.h"

#define WLR_USE_UNSTABLE
#include "wlr/types/wlr_compositor.h"
#include "wlr/types/wlr_cursor.h"
#include "wlr/types/wlr_seat.h"
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the input observation manager. */
struct _wlmaker_input_observation_manager_t {
    /** The global holding the input observation's interface. */
    struct wl_global          *wl_global_ptr;
    /** Link to the wlroots' implementation of wl_seat. */
    struct wlr_seat           *wlr_seat_ptr;
    /** Link to the wlroots' cursor implementation. */
    struct wlr_cursor         *wlr_cursor_ptr;
};

/** State of a tracker. */
struct _wlmaker_position_tracker_t {
    /** The corresponding resource. */
    struct wl_resource        *wl_resource_ptr;
    /** The surface it tracks. */
    struct wlr_surface        *wlr_surface_ptr;
    /** Link to the wlroots' cursor implementation. */
    struct wlr_cursor         *wlr_cursor_ptr;

    /** Listener for `destroy` event of the `wlr_surface_ptr`. */
    struct wl_listener        surface_destroy_listener;
    /** Listener for `frame` event of struct wlr_cursor. */
    struct wl_listener        cursor_frame_listener;
};

static wlmaker_input_observation_manager_t *input_observation_manager_from_resource(
    struct wl_resource *wl_resource_ptr);

static void bind_input_observation(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void input_observation_manager_handle_pointer_position(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id,
    struct wl_resource *surface);

static wlmaker_position_tracker_t *wlmaker_position_tracker_from_resource(
    struct wl_resource *wl_resource_ptr);
static wlmaker_position_tracker_t *wlmaker_position_tracker_create(
    struct wl_client *wl_client_ptr,
    wlmaker_input_observation_manager_t *manager_ptr,
    uint32_t id,
    int version,
    struct wlr_surface *wlr_surface_ptr);
static void wlmaker_position_tracker_resource_destroy(
    struct wl_resource *wl_resource_ptr);
static void wlmaker_position_tracker_destroy(
    wlmaker_position_tracker_t *tracker_ptr);

static void _wlmaker_position_tracker_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_position_tracker_handle_cursor_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* ========================================================================= */

/** Implementation of the position tracking. */
static const struct ext_input_observation_manager_v1_interface
input_observation_manager_v1_implementation = {
    .destroy = handle_resource_destroy,
    .pointer_position = input_observation_manager_handle_pointer_position,
};

/** Implementation of the position (position) tracker. */
static const struct ext_input_position_observation_v1_interface
position_tracker_v1_implementation = {
    .destroy = handle_resource_destroy,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_input_observation_manager_t *wlmaker_input_observation_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_seat *wlr_seat_ptr,
    struct wlr_cursor *wlr_cursor_ptr)
{
    wlmaker_input_observation_manager_t *manager_ptr = logged_calloc(
        1, sizeof(wlmaker_input_observation_manager_t));
    if (NULL == manager_ptr) return NULL;
    manager_ptr->wlr_seat_ptr = wlr_seat_ptr;
    manager_ptr->wlr_cursor_ptr = wlr_cursor_ptr;

    manager_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &ext_input_observation_manager_v1_interface,
        1,
        manager_ptr,
        bind_input_observation);
    if (NULL == manager_ptr->wl_global_ptr) {
        bs_log(BS_ERROR, "Failed wl_global_create");
        wlmaker_input_observation_manager_destroy(manager_ptr);
        return NULL;
    }

    return manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_input_observation_manager_destroy(
    wlmaker_input_observation_manager_t *manager_ptr)
{
    if (NULL != manager_ptr->wl_global_ptr) {
        wl_global_destroy(manager_ptr->wl_global_ptr);
        manager_ptr->wl_global_ptr = NULL;
    }

    free(manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel position tracking from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Position to the @ref wlmaker_input_observation_manager_t.
 */
wlmaker_input_observation_manager_t *input_observation_manager_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &ext_input_observation_manager_v1_interface,
                  &input_observation_manager_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Binds the position tracking for the client.
 *
 * @param wl_client_ptr
 * @param data_ptr
 * @param version
 * @param id
 */
void bind_input_observation(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wl_resource *wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &ext_input_observation_manager_v1_interface,
        version,
        id);
    if (NULL == wl_resource_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
   wlmaker_input_observation_manager_t *manager_ptr = data_ptr;

    wl_resource_set_implementation(
        wl_resource_ptr,
        &input_observation_manager_v1_implementation,  // implementation.
        manager_ptr,  // data
        NULL);  // dtor. We don't have an explicit one.

}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the 'destroy' method: Destroys the resource.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 */
void handle_resource_destroy(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    wl_resource_destroy(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a position tracker for pointer, associated with the surface.
 *
 * Requires that @ref wlmaker_input_observation_manager_t::wlr_seat_ptr is set
 * and has the `WL_SEAT_CAPABILITY_POINTER` capability.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param id
 * @param surface_wl_resource_ptr Resource handle of the surface.
 */
void input_observation_manager_handle_pointer_position(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    uint32_t id,
    struct wl_resource *surface_wl_resource_ptr)
{
    wlmaker_input_observation_manager_t *manager_ptr =
        input_observation_manager_from_resource(wl_resource_ptr);

    // Guard clause: We require the position capability to be (or have been)
    // present for the seat.
    if (!(manager_ptr->wlr_seat_ptr->accumulated_capabilities &
          WL_SEAT_CAPABILITY_POINTER)) {
        wl_resource_post_error(
            wl_resource_ptr,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Missing pointer capability on seat");
        return;
    }

    struct wlr_surface *wlr_surface_ptr =
        wlr_surface_from_resource(surface_wl_resource_ptr);

    wlmaker_position_tracker_t *tracker_ptr = wlmaker_position_tracker_create(
        wl_client_ptr,
            manager_ptr,
            id,
        wl_resource_get_version(wl_resource_ptr),
        wlr_surface_ptr);
    if (NULL == tracker_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/** Ctor for the tracker. */
wlmaker_position_tracker_t *wlmaker_position_tracker_create(
    struct wl_client *wl_client_ptr,
    wlmaker_input_observation_manager_t *manager_ptr,
    uint32_t id,
    int version,
    struct wlr_surface *wlr_surface_ptr)
{
    wlmaker_position_tracker_t *tracker_ptr = logged_calloc(
        1, sizeof(wlmaker_position_tracker_t));
    if (NULL == tracker_ptr) return NULL;
    tracker_ptr->wlr_surface_ptr = wlr_surface_ptr;
    tracker_ptr->wlr_cursor_ptr = manager_ptr->wlr_cursor_ptr;

    tracker_ptr->wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &ext_input_position_observation_v1_interface,
        version,
        id);
    if (NULL == tracker_ptr->wl_resource_ptr) {
        bs_log(BS_ERROR, "Failed wl_resource_create(%p, %p, %d, %"PRIu32")",
               wl_client_ptr, &ext_input_position_observation_v1_interface,
               version, id);
        wlmaker_position_tracker_destroy(tracker_ptr);
        return NULL;
    }
    wl_resource_set_implementation(
        tracker_ptr->wl_resource_ptr,
        &position_tracker_v1_implementation,
        tracker_ptr,
        wlmaker_position_tracker_resource_destroy);

    wlmtk_util_connect_listener_signal(
        &tracker_ptr->wlr_surface_ptr->events.destroy,
        &tracker_ptr->surface_destroy_listener,
        _wlmaker_position_tracker_handle_surface_destroy);
    wlmtk_util_connect_listener_signal(
        &tracker_ptr->wlr_cursor_ptr->events.frame,
        &tracker_ptr->cursor_frame_listener,
        _wlmaker_position_tracker_handle_cursor_frame);

    return tracker_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor, from the resource. */
void wlmaker_position_tracker_resource_destroy(
    struct wl_resource *wl_resource_ptr)
{
    wlmaker_position_tracker_t *tracker_ptr =
        wlmaker_position_tracker_from_resource(wl_resource_ptr);
    wlmaker_position_tracker_destroy(tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor. */
void wlmaker_position_tracker_destroy(wlmaker_position_tracker_t *tracker_ptr)
{
    wlmtk_util_disconnect_listener(&tracker_ptr->cursor_frame_listener);
    wlmtk_util_disconnect_listener(&tracker_ptr->surface_destroy_listener);
    free(tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** Type-safe conversion from resource to tracker. */
wlmaker_position_tracker_t *wlmaker_position_tracker_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &ext_input_position_observation_v1_interface,
                  &position_tracker_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles surface destruction: Destroy the tracker. */
static void _wlmaker_position_tracker_handle_surface_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_position_tracker_t *tracker_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_position_tracker_t, surface_destroy_listener);
    wl_resource_destroy(tracker_ptr->wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles cursor frame events: Sends the current pointer position. */
void _wlmaker_position_tracker_handle_cursor_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_position_tracker_t *tracker_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_position_tracker_t, cursor_frame_listener);

    // Guard clause: No wlmtk surface or no scene means it's not fully mapped.
    wlmtk_surface_t *surface_ptr = tracker_ptr->wlr_surface_ptr->data;
    if (NULL == surface_ptr ||
        NULL == surface_ptr->wlr_scene_tree_ptr) return;

    // Get coordinates. Returns false if not all parents rae enabled.
    int node_x, node_y;
    if (!wlr_scene_node_coords(
            &surface_ptr->wlr_scene_tree_ptr->node, &node_x, &node_y)) return;

    // Then, compute the cursor position, relative to the surface dimenions.
    // Note: This assumes the surface remains aligned to X/Y axes.
    int width, height;
    wlmtk_surface_get_size(surface_ptr, &width, &height);

    // Translate to 24:8 fixpoint and bound to range.
    double x = 256.0 * (double)(
        tracker_ptr->wlr_cursor_ptr->x - node_x) / width;
    double y = 256.0 * (double)(
        tracker_ptr->wlr_cursor_ptr->y - node_y) / height;
    ext_input_position_observation_v1_send_position(
        tracker_ptr->wl_resource_ptr,
        tracker_ptr->wlr_surface_ptr->resource,
        BS_MAX(INT32_MIN, BS_MIN(INT32_MAX, x)),
        BS_MAX(INT32_MIN, BS_MIN(INT32_MAX, y)));
}

/* == End of input_observation.c =========================================== */