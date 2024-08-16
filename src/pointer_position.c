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

/* == Declarations ========================================================= */

/** State of the pointer position extension. */
struct _wlmaker_pointer_position_t {
    /** The global holding the pointer position's interface. */
    struct wl_global          *wl_global_ptr;
};

static void bind_pointer_position(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);
static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

/* ========================================================================= */

/** Implementation of the pointer position. */
static const struct zwlmaker_pointer_position_v1_interface
pointer_position_v1_implementation = {
    .destroy = handle_resource_destroy,
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

/* == End of pointer_position.c ============================================ */
