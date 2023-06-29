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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

#include "wlmaker-toplevel-icon-unstable-v1-server-protocol.h"

/* == Declarations ========================================================= */

/** State of the toplevel icon manager. */
struct _wlmaker_toplevel_icon_manager_t {
    /** The global holding the icon manager's interface. */
    struct wl_global          *wl_global_ptr;
};

/** State of a toplevel icon. */
struct _wlmaker_toplevel_icon_t {
    /** Back-link to the client requesting the toplevel. */
    struct wl_client          *wl_client_ptr;
    /** Back-link to the toplevel icon manager. */
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr;
    /** The provided ID. */
    uint32_t                  id;
    /** The XDG toplevel for which the icon is specified. */
    struct wlr_xdg_toplevel   *wlr_xdg_toplevel_ptr;
    /** The surface to use for the icon of this toplevel. */
    struct wlr_surface        *wlr_surface_ptr;

    /** The resource associated with this icon. */
    struct wl_resource        *wl_resource_ptr;
};

static wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_from_resource(
    struct wl_resource *wl_resource_ptr);
static wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_from_resource(
    struct wl_resource *wl_resource_ptr);

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
    struct wl_resource *wl_toplevel_icon_manager_resource_ptr,
    uint32_t id,
    struct wl_resource *wl_toplevel_resource_ptr,
    struct wl_resource *wl_surface_resource_ptr);

static wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_create(
    struct wl_client *wl_client_ptr,
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr,
    uint32_t id,
    int version,
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    struct wlr_surface *wlr_surface_ptr);
static void wlmaker_toplevel_icon_destroy(
    wlmaker_toplevel_icon_t *toplevel_icon_ptr);
static void toplevel_icon_resource_destroy(
    struct wl_resource *wl_resource_ptr);

/* == Data ================================================================= */

/** Implementation of the toplevel icon manager interface. */
static const struct zwlmaker_toplevel_icon_manager_v1_interface
toplevel_icon_manager_v1_implementation = {
    .destroy = handle_resource_destroy,
    .get_toplevel_icon = handle_get_toplevel_icon
};

/** Implementation of the toplevel icon interface. */
static const struct zwlmaker_toplevel_icon_v1_interface
toplevel_icon_v1_implementation = {
    .destroy = handle_resource_destroy,
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
 * Returns the toplevel icon manager from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_toplevel_icon_manager_t.
 */
wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_toplevel_icon_manager_v1_interface,
                  &toplevel_icon_manager_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel icon from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_toplevel_icon_t.
 */
wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_toplevel_icon_v1_interface,
                  &toplevel_icon_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

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
        &toplevel_icon_manager_v1_implementation,  // implementation.
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
 * @param wl_toplevel_icon_manager_resource_ptr
 * @param id
 * @param wl_toplevel_resource_ptr
 * @param wl_surface_resource_ptr
 */
void handle_get_toplevel_icon(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_toplevel_icon_manager_resource_ptr,
    uint32_t id,
    struct wl_resource *wl_toplevel_resource_ptr,
    struct wl_resource *wl_surface_resource_ptr)
{
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr =
        toplevel_icon_manager_from_resource(
            wl_toplevel_icon_manager_resource_ptr);
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr =
        wlr_xdg_toplevel_from_resource(wl_toplevel_resource_ptr);
    struct wlr_surface *wlr_surface_ptr =
        wlr_surface_from_resource(wl_surface_resource_ptr);

    wlmaker_toplevel_icon_t *toplevel_icon_ptr = wlmaker_toplevel_icon_create(
        wl_client_ptr,
        toplevel_icon_manager_ptr,
        id,
        wl_resource_get_version(wl_toplevel_icon_manager_resource_ptr),
        wlr_xdg_toplevel_ptr,
        wlr_surface_ptr);
    if (NULL == toplevel_icon_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a new toplevel icon.
 *
 * @param wl_client_ptr
 * @param toplevel_icon_manager_ptr
 * @param id
 * @param version
 * @param wlr_xdg_toplevel_ptr
 * @param wlr_surface_ptr
 *
 * @return A pointer to the new toplevel icon or NULL on error. The toplevel
 *     icon's resources must be free'd via @ref wlmaker_toplevel_icon_destroy.
 */
wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_create(
    struct wl_client *wl_client_ptr,
    wlmaker_toplevel_icon_manager_t *toplevel_icon_manager_ptr,
    uint32_t id,
    int version,
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    struct wlr_surface *wlr_surface_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr = logged_calloc(
        1, sizeof(wlmaker_toplevel_icon_t));
    if (NULL == toplevel_icon_ptr) return NULL;

    toplevel_icon_ptr->wl_client_ptr = wl_client_ptr;
    toplevel_icon_ptr->toplevel_icon_manager_ptr = toplevel_icon_manager_ptr;
    toplevel_icon_ptr->id = id;
    toplevel_icon_ptr->wlr_xdg_toplevel_ptr = wlr_xdg_toplevel_ptr;
    toplevel_icon_ptr->wlr_surface_ptr = wlr_surface_ptr;

    toplevel_icon_ptr->wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_toplevel_icon_v1_interface,
        version,
        id);
    if (NULL == toplevel_icon_ptr->wl_resource_ptr) {
        bs_log(BS_ERROR, "Failed wl_resource_create(%p, %p, %d, %"PRIu32")",
               wl_client_ptr, &zwlmaker_toplevel_icon_v1_interface, version,
               id);
        wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
        return NULL;
    }
    wl_resource_set_implementation(
        toplevel_icon_ptr->wl_resource_ptr,
        &toplevel_icon_v1_implementation,
        toplevel_icon_ptr,
        toplevel_icon_resource_destroy);

    bs_log(BS_INFO, "created toplevel icon %p for toplevel %p, surface %p",
           toplevel_icon_ptr, wlr_xdg_toplevel_ptr, wlr_surface_ptr);
    return toplevel_icon_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the toplevel icon.
 *
 * @param toplevel_icon_ptr
 */
void wlmaker_toplevel_icon_destroy(
    wlmaker_toplevel_icon_t *toplevel_icon_ptr)
{
    // Note: Not destroying toplevel_icon_ptr->resource, since that causes
    // cycles...

    bs_log(BS_INFO, "Destroying toplevel icon %p", toplevel_icon_ptr);
    free(toplevel_icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor for the toplevel icon's resource.
 *
 * @param wl_resource_ptr
 */
void toplevel_icon_resource_destroy(struct wl_resource *wl_resource_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr =
        wlmaker_toplevel_icon_from_resource(wl_resource_ptr);
    wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
}

/* == End of toplevel_icon_manager.c ======================================= */
