/* ========================================================================= */
/**
 * @file pointer_tracking.c
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

#include "pointer_tracking.h"

#include "wlmaker-pointer-tracking-v1-server-protocol.h"

#define WLR_USE_UNSTABLE
#include "wlr/types/wlr_compositor.h"
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the pointer tracking extension. */
struct _wlmaker_pointer_tracking_t {
    /** The global holding the pointer tracking's interface. */
    struct wl_global          *wl_global_ptr;
};

/** State of a tracker. */
struct _wlmaker_pointer_tracker_t {
    /** The corresponding resource. */
    struct wl_resource        *wl_resource_ptr;
    /** The surface it tracks. */
    struct wlr_surface        *wlr_surface_ptr;
    // FIXME: Subscribe to the destroy listener of the surface. We will want
    // to destroy the tracker if the surface disappears.
};

static wlmaker_pointer_tracking_t *pointer_tracking_from_resource(
    struct wl_resource *wl_resource_ptr);

static void bind_pointer_tracking(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void pointer_tracking_handle_track(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id,
    struct wl_resource *surface);

static wlmaker_pointer_tracker_t *wlmaker_pointer_tracker_from_resource(
    struct wl_resource *wl_resource_ptr);
static wlmaker_pointer_tracker_t *wlmaker_pointer_tracker_create(
    struct wl_client *wl_client_ptr,
    wlmaker_pointer_tracking_t *tracking_ptr,
    uint32_t id,
    int version,
    struct wlr_surface *wlr_surface_ptr);
static void wlmaker_pointer_tracker_resource_destroy(
    struct wl_resource *wl_resource_ptr);
static void wlmaker_pointer_tracker_destroy(
    wlmaker_pointer_tracker_t *tracker_ptr);

/* ========================================================================= */

/** Implementation of the pointer tracking. */
static const struct zwlmaker_pointer_tracking_v1_interface
pointer_tracking_v1_implementation = {
    .destroy = handle_resource_destroy,
    .track = pointer_tracking_handle_track,
};

/** Implementation of the pointer (position) tracker. */
static const struct zwlmaker_pointer_tracker_v1_interface
pointer_tracker_v1_implementation = {
    .destroy = handle_resource_destroy,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_pointer_tracking_t *wlmaker_pointer_tracking_create(
    struct wl_display *wl_display_ptr)
{
    wlmaker_pointer_tracking_t *tracking_ptr = logged_calloc(
        1, sizeof(wlmaker_pointer_tracking_t));
    if (NULL == tracking_ptr) return NULL;

    tracking_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &zwlmaker_pointer_tracking_v1_interface,
        1,
        tracking_ptr,
        bind_pointer_tracking);
    if (NULL == tracking_ptr->wl_global_ptr) {
        bs_log(BS_ERROR, "Failed wl_global_create");
        wlmaker_pointer_tracking_destroy(tracking_ptr);
        return NULL;
    }

    return tracking_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_pointer_tracking_destroy(wlmaker_pointer_tracking_t *tracking_ptr)
{
    if (NULL != tracking_ptr->wl_global_ptr) {
        wl_global_destroy(tracking_ptr->wl_global_ptr);
        tracking_ptr->wl_global_ptr = NULL;
    }

    free(tracking_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel pointer tracking from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_pointer_tracking_t.
 */
wlmaker_pointer_tracking_t *pointer_tracking_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_pointer_tracking_v1_interface,
                  &pointer_tracking_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Binds the pointer tracking for the client.
 *
 * @param wl_client_ptr
 * @param data_ptr
 * @param version
 * @param id
 */
void bind_pointer_tracking(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wl_resource *wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_pointer_tracking_v1_interface,
        version,
        id);
    if (NULL == wl_resource_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
   wlmaker_pointer_tracking_t *tracking_ptr = data_ptr;

    wl_resource_set_implementation(
        wl_resource_ptr,
        &pointer_tracking_v1_implementation,  // implementation.
        tracking_ptr,  // data
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
 * Creates a new pointer tracker, associated with the provided surface.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param id
 * @param surface_wl_resource_ptr Resource handle of the surface.
 */
void pointer_tracking_handle_track(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    uint32_t id,
    struct wl_resource *surface_wl_resource_ptr)
{
    wlmaker_pointer_tracking_t *tracking_ptr = pointer_tracking_from_resource(
        wl_resource_ptr);

    struct wlr_surface *wlr_surface_ptr =
        wlr_surface_from_resource(surface_wl_resource_ptr);

    wlmaker_pointer_tracker_t *tracker_ptr = wlmaker_pointer_tracker_create(
        wl_client_ptr,
            tracking_ptr,
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
wlmaker_pointer_tracker_t *wlmaker_pointer_tracker_create(
    struct wl_client *wl_client_ptr,
    __UNUSED__ wlmaker_pointer_tracking_t *tracking_ptr,
    uint32_t id,
    int version,
    struct wlr_surface *wlr_surface_ptr)
{
    wlmaker_pointer_tracker_t *tracker_ptr = logged_calloc(
        1, sizeof(wlmaker_pointer_tracker_t));
    if (NULL == tracker_ptr) return NULL;
    tracker_ptr->wlr_surface_ptr = wlr_surface_ptr;

    tracker_ptr->wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_pointer_tracker_v1_interface,
        version,
        id);
    if (NULL == tracker_ptr->wl_resource_ptr) {
        bs_log(BS_ERROR, "Failed wl_resource_create(%p, %p, %d, %"PRIu32")",
               wl_client_ptr, &zwlmaker_pointer_tracker_v1_interface,
               version, id);
        wlmaker_pointer_tracker_destroy(tracker_ptr);
        return NULL;
    }
    wl_resource_set_implementation(
        tracker_ptr->wl_resource_ptr,
        &pointer_tracker_v1_implementation,
        tracker_ptr,
        wlmaker_pointer_tracker_resource_destroy);

    if (false) {
        zwlmaker_pointer_tracker_v1_send_position(
            tracker_ptr->wl_resource_ptr,
            wlr_surface_ptr->resource,
            1, 2);
    }

    return tracker_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor, from the resource. */
void wlmaker_pointer_tracker_resource_destroy(
    struct wl_resource *wl_resource_ptr)
{
    wlmaker_pointer_tracker_t *tracker_ptr =
        wlmaker_pointer_tracker_from_resource(wl_resource_ptr);
    wlmaker_pointer_tracker_destroy(tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor. */
void wlmaker_pointer_tracker_destroy(wlmaker_pointer_tracker_t *tracker_ptr)
{
    free(tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** Type-safe conversion from resource to tracker. */
wlmaker_pointer_tracker_t *wlmaker_pointer_tracker_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_pointer_tracker_v1_interface,
                  &pointer_tracker_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* == End of pointer_tracking.c ============================================ */
