/* ========================================================================= */
/**
 * @file toplevel_icon_manager.c
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

#include "toplevel_icon_manager.h"

#include <libbase/libbase.h>

#include "wlmaker-toplevel-icon-unstable-v1-server-protocol.h"


/* == Declarations ========================================================= */

/** State of the toplevel icon manager. */
struct _wlmaker_toplevel_icon_manager_t {
    /** The global holding the icon manager's interface. */
    struct wl_global          *wl_global_ptr;
};

static void bind_toplevel_icon_manager(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

static void handle_get_toplevel_icon(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    uint32_t id,
    struct wl_resource *wl_toplevel_resource_ptr,
    struct wl_resource *wl_surface_resource_ptr);

/* == Data ================================================================= */

/** Interface of the toplevel icon manager: Pointers to methods. */
static const struct zwlmaker_toplevel_icon_manager_v1_interface
toplevel_icon_manager_v1_interface = {
    .destroy = handle_resource_destroy,
    .get_toplevel_icon = handle_get_toplevel_icon
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_toplevel_icon_manager_t *wlmaker_toplevel_icon_manager_create(
    struct wl_display *wl_display_ptr)
{
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr =
        logged_calloc(1, sizeof(wlmaker_toplevel_icon_manager_t));
    if (NULL == toplevel_icon_manager_ptr) return NULL;

    toplevel_icon_manager_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &zwlmaker_toplevel_icon_manager_v1_interface,
        1,
        toplevel_icon_manager_ptr,
        bind_toplevel_icon_manager);
    if (NULL == toplevel_icon_manager_ptr->wl_global_ptr) {
        wlmaker_toplevel_icon_manager_destroy(toplevel_icon_manager_ptr);
        return NULL;
    }

    return toplevel_icon_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_toplevel_icon_manager_destroy(
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr)
{
    if (NULL != toplevel_icon_manager_ptr->wl_global_ptr) {
        wl_global_destroy(toplevel_icon_manager_ptr->wl_global_ptr);
        toplevel_icon_manager_ptr->wl_global_ptr = NULL;
    }

    free(toplevel_icon_manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Binds an icon manager for the client.
 *
 * @param wl_client_ptr
 * @param data_ptr
 * @param version
 * @param id
 */
void bind_toplevel_icon_manager(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wl_resource *wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_toplevel_icon_manager_v1_interface,
        version, id);
    if (NULL == wl_resource_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
   wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr = data_ptr;

    wl_resource_set_implementation(
        wl_resource_ptr,
        &toplevel_icon_manager_v1_interface,  // implementation.
        toplevel_icon_manager_ptr,  // data
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
 * Handler for the 'get_toplevel_icon' method.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param id
 * @param wl_toplevel_resource_ptr
 * @param wl_surface_resource_ptr
 */
void handle_get_toplevel_icon(
    __UNUSED__ struct wl_client *wl_client_ptr,
    __UNUSED__ struct wl_resource *wl_resource_ptr,
    __UNUSED__ uint32_t id,
    __UNUSED__ struct wl_resource *wl_toplevel_resource_ptr,
    __UNUSED__ struct wl_resource *wl_surface_resource_ptr)
{
}

/* == End of toplevel_icon_manager.c ======================================= */
