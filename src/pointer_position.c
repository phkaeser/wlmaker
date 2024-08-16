/* ========================================================================= */
/**
 * @file pointer_position.c
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

#include "pointer_position.h"

#include "wlmaker-pointer-position-v1-server-protocol.h"

#define WLR_USE_UNSTABLE
#include "wlr/types/wlr_compositor.h"
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the pointer position extension. */
struct _wlmaker_pointer_position_t {
    /** The global holding the pointer position's interface. */
    struct wl_global          *wl_global_ptr;
};

struct _wlmaker_pointer_position_follow_t {
};

static wlmaker_pointer_position_t *pointer_position_from_resource(
    struct wl_resource *wl_resource_ptr);

static void bind_pointer_position(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);
static void pointer_position_handle_follow(
    struct wl_client *client,
    struct wl_resource *resource,
    uint32_t id,
    struct wl_resource *surface);

static wlmaker_pointer_position_follow_t *wlmaker_pointer_position_follow_create(void);

/* ========================================================================= */

/** Implementation of the pointer position. */
static const struct zwlmaker_pointer_position_v1_interface
pointer_position_v1_implementation = {
    .destroy = handle_resource_destroy,
    .follow = pointer_position_handle_follow,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_pointer_position_t *wlmaker_pointer_position_create(
    struct wl_display *wl_display_ptr)
{
    wlmaker_pointer_position_t *ppos_ptr = logged_calloc(
        1, sizeof(wlmaker_pointer_position_t));
    if (NULL == ppos_ptr) return NULL;

    ppos_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &zwlmaker_pointer_position_v1_interface,
        1,
        ppos_ptr,
        bind_pointer_position);
    if (NULL == ppos_ptr->wl_global_ptr) {
        bs_log(BS_ERROR, "Failed wl_global_create");
        wlmaker_pointer_position_destroy(ppos_ptr);
        return NULL;
    }

    return ppos_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_pointer_position_destroy(wlmaker_pointer_position_t *ppos_ptr)
{
    if (NULL != ppos_ptr->wl_global_ptr) {
        wl_global_destroy(ppos_ptr->wl_global_ptr);
        ppos_ptr->wl_global_ptr = NULL;
    }
    
    free(ppos_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel pointer position from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_pointer_position_t.
 */
wlmaker_pointer_position_t *pointer_position_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_pointer_position_v1_interface,
                  &pointer_position_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Binds the pointer position for the client.
 *
 * @param wl_client_ptr
 * @param data_ptr
 * @param version
 * @param id
 */
void bind_pointer_position(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wl_resource *wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_pointer_position_v1_interface,
        version,
        id);
    if (NULL == wl_resource_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
   wlmaker_pointer_position_t *ppos_ptr = data_ptr;

    wl_resource_set_implementation(
        wl_resource_ptr,
        &pointer_position_v1_implementation,  // implementation.
        ppos_ptr,  // data
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
 * Creates a new position follower object.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param id
 * @param surface
 */
void pointer_position_handle_follow(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    __UNUSED__ uint32_t id,
    struct wl_resource *surface)
{
    __UNUSED__ wlmaker_pointer_position_t *ppos_ptr = pointer_position_from_resource(
        wl_resource_ptr);

    __UNUSED__ struct wlr_surface *wlr_surface_ptr =
        wlr_surface_from_resource(surface);

    wlmaker_pointer_position_follow_t *follow_ptr =
        wlmaker_pointer_position_follow_create();
    if (NULL == follow_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/** Ctor for the follow. */
wlmaker_pointer_position_follow_t *wlmaker_pointer_position_follow_create(void)
{
    wlmaker_pointer_position_follow_t *follow_ptr = logged_calloc(
        1, sizeof(wlmaker_pointer_position_follow_t));
    if (NULL == follow_ptr) return NULL;

    return follow_ptr;
}

/* == End of pointer_position.c ============================================ */
