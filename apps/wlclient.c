/* ========================================================================= */
/**
 * @file wlclient.c
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

#include "wlclient.h"

#include <wayland-client.h>
#include "wlmaker-icon-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/* == Declarations ========================================================= */

/** Forward declaration: Icon. */
typedef struct _wlclient_icon_t wlclient_icon_t;

/** State of the wayland client. */
struct _wlclient_t {
    /** Wayland display connection. */
    struct wl_display         *wl_display_ptr;
    /** Registry singleton for the above connection. */
    struct wl_registry        *wl_registry_ptr;

    /** The bound compositor interface. */
    struct wl_compositor      *wl_compositor_ptr;
    /** The bound SHM interface. */
    struct wl_shm             *wl_shm_ptr;
    /** The bound XDG wm_base interface. */
    struct xdg_wm_base        *xdg_wm_base_ptr;
    /** The bound Toplevel Icon Manager. */
    struct zwlmaker_icon_manager_v1 *icon_manager_ptr;

    /** The icon. */
    wlclient_icon_t           *icon_ptr;
};

/** State of the icon. */
typedef struct _wlclient_icon_t {
    /** Surface. */
    struct wl_surface         *wl_surface_ptr;
    /** The icon interface. */
    struct zwlmaker_toplevel_icon_v1 *toplevel_icon_ptr;

    /** Width of the icon, once suggested by the server. */
    int32_t                   width;
    /** Height of the icon, once suggested by the server. */
    int32_t                   height;
} wlclient_icon_t;

/** Descriptor for a wayland object to bind to. */
typedef struct {
    /** The interface definition. */
    const struct wl_interface *wl_interface_ptr;
    /** Version desired to bind to. */
    uint32_t                  desired_version;
    /** Offset of the bound interface, relative to `wlclient_t`. */
    size_t                    bound_ptr_offset;
} object_t;

static void handle_global_announce(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name,
    const char *interface_ptr,
    uint32_t version);
static void handle_global_remove(
    void *data_ptr,
    struct wl_registry *registry,
    uint32_t name);

static wlclient_icon_t *wlclient_icon_create(
    wlclient_t *wlclient_ptr);
static void wlclient_icon_destroy(
    wlclient_icon_t *icon_ptr);
static void handle_toplevel_icon_configure(
    void *data_ptr,
    struct zwlmaker_toplevel_icon_v1 *zwlmaker_toplevel_icon_v1_ptr,
    int32_t width,
    int32_t height,
    uint32_t serial);

/* == Data ================================================================= */

/** Listener for the registry, taking note of registry updates. */
static const struct wl_registry_listener registry_listener = {
    .global = handle_global_announce,
    .global_remove = handle_global_remove,
};

/** List of wayland objects we want to bind to. */
static const object_t objects[] = {
    { &wl_compositor_interface, 4,
      offsetof(wlclient_t, wl_compositor_ptr) },
    { &wl_shm_interface, 1,
      offsetof(wlclient_t, wl_shm_ptr) },
    { &xdg_wm_base_interface, 1,
      offsetof(wlclient_t, xdg_wm_base_ptr) },
    { &zwlmaker_icon_manager_v1_interface, 1,
      offsetof(wlclient_t, icon_manager_ptr) },
    { NULL, 0, 0 }  // sentinel.
};

/** Listener implementation for toplevel icon. */
static const struct zwlmaker_toplevel_icon_v1_listener toplevel_icon_listener={
    .configure = handle_toplevel_icon_configure,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_t *wlclient_create(void)
{
    wlclient_t *wlclient_ptr = logged_calloc(1, sizeof(wlclient_t));
    if (NULL == wlclient_ptr) return NULL;

    wlclient_ptr->wl_display_ptr = wl_display_connect(NULL);
    if (NULL == wlclient_ptr->wl_display_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_connect(NULL).");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    wlclient_ptr->wl_registry_ptr = wl_display_get_registry(
        wlclient_ptr->wl_display_ptr);
    if (NULL == wlclient_ptr->wl_registry_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_get_registry(%p).",
               wlclient_ptr->wl_registry_ptr);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    if (0 != wl_registry_add_listener(
            wlclient_ptr->wl_registry_ptr, &registry_listener, wlclient_ptr)) {
        bs_log(BS_ERROR, "Failed wl_registry_add_listener(%p, %p, %p).",
               wlclient_ptr->wl_registry_ptr, &registry_listener, wlclient_ptr);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    wl_display_roundtrip(wlclient_ptr->wl_display_ptr);

    if (NULL == wlclient_ptr->wl_compositor_ptr) {
        bs_log(BS_ERROR, "'wl_compositor' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (NULL == wlclient_ptr->wl_shm_ptr) {
        bs_log(BS_ERROR, "'wl_shm' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (NULL == wlclient_ptr->xdg_wm_base_ptr) {
        bs_log(BS_ERROR, "'xdg_wm_base' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    if (NULL != wlclient_ptr->icon_manager_ptr) {
        wlclient_ptr->icon_ptr = wlclient_icon_create(wlclient_ptr);
    }

    return wlclient_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_destroy(wlclient_t *wlclient_ptr)
{
    if (NULL != wlclient_ptr->icon_ptr) {
        wlclient_icon_destroy(wlclient_ptr->icon_ptr);
    }

    if (NULL != wlclient_ptr->wl_registry_ptr) {
        wl_registry_destroy(wlclient_ptr->wl_registry_ptr);
        wlclient_ptr->wl_registry_ptr = NULL;
    }

    if (NULL != wlclient_ptr->wl_display_ptr) {
        wl_display_disconnect(wlclient_ptr->wl_display_ptr);
        wlclient_ptr->wl_display_ptr = NULL;
    }

    free(wlclient_ptr);
}

/* ------------------------------------------------------------------------- */
void wlclient_add_timer(
    __UNUSED__ wlclient_t *wlclient_ptr,
    __UNUSED__ uint64_t msec,
    __UNUSED__ void (*callback)(wlclient_t *wlclient_ptr, void *ud_ptr),
    __UNUSED__ void *ud_ptr)
{
}

/* ------------------------------------------------------------------------- */
bool wlclient_icon_supported(
    wlclient_t *wlclient_ptr)
{
    return (NULL != wlclient_ptr->icon_manager_ptr);
}

/* ------------------------------------------------------------------------- */
bs_gfxbuf_t *wlclient_icon_gfxbuf(
    wlclient_t *wlclient_ptr)
{
    if (!wlclient_icon_supported(wlclient_ptr)) return NULL;


    // TODO(kaeser@gubbe.ch): Add implementation.
    return NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the announcement of a global object.
 *
 * Called by `struct wl_registry_listener` `global` callback, invoked to notify
 * clients of global objects.
 *
 * @param data_ptr            Points to a @ref wlclient_t.
 * @param wl_registry_ptr     The `struct wl_registry` this is invoked for.
 * @param name                Numeric name of the global object.
 * @param interface_name_ptr  Name of the interface implemented by the object.
 * @param version             Interface version.
 */
void handle_global_announce(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name,
    const char *interface_name_ptr,
    uint32_t version)
{
    for (const object_t *object_ptr = &objects[0];
         NULL != object_ptr->wl_interface_ptr;
         ++object_ptr) {
        // Proceed, if the interface name doesn't match.
        if (0 != strcmp(interface_name_ptr,
                        object_ptr->wl_interface_ptr->name)) {
            continue;
        }

        void *bound_ptr = wl_registry_bind(
            wl_registry_ptr, name,
            object_ptr->wl_interface_ptr,
            object_ptr->desired_version);
        if (NULL == bound_ptr) {
            bs_log(BS_ERROR,
                   "Failed wl_registry_bind(%p, %"PRIu32", %p, %"PRIu32") "
                   "for interface %s, version %"PRIu32".",
                   wl_registry_ptr, name,
                   object_ptr->wl_interface_ptr,
                   object_ptr->desired_version,
                   interface_name_ptr, version);
            continue;
        }

        ((void**)((uint8_t*)data_ptr + object_ptr->bound_ptr_offset))[0] =
            bound_ptr;
        bs_log(BS_DEBUG, "Bound interface %s to %p",
               interface_name_ptr, bound_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the removal of a wayland global object.
 *
 * Called by `struct wl_registry_listener` `global_remove`, invoked to notify
 * clients of removed global objects.
 *
 * @param data_ptr            Points to a @ref wlclient_t.
 * @param wl_registry_ptr     The `struct wl_registry` this is invoked for.
 * @param name                Numeric name of the global object.
 */
void handle_global_remove(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name)
{
    // TODO(kaeser@gubbe.ch): Add implementation.
    bs_log(BS_INFO, "handle_global_remove(%p, %p, %"PRIu32").",
           data_ptr, wl_registry_ptr, name);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates the icon state.
 *
 * @param wlclient_ptr
 *
 * @return The state, or NULL on error.
 */
wlclient_icon_t *wlclient_icon_create(wlclient_t *wlclient_ptr)
{
    wlclient_icon_t *icon_ptr = logged_calloc(1, sizeof(wlclient_icon_t));
    if (NULL == icon_ptr) return NULL;

    icon_ptr->wl_surface_ptr = wl_compositor_create_surface(
        wlclient_ptr->wl_compositor_ptr);
    if (NULL == icon_ptr->wl_surface_ptr) {
        bs_log(BS_ERROR, "Failed wl_compositor_create_surface(%p).",
               wlclient_ptr->wl_compositor_ptr);
        wlclient_icon_destroy(icon_ptr);
        return NULL;
    }

    icon_ptr->toplevel_icon_ptr = zwlmaker_icon_manager_v1_get_toplevel_icon(
        wlclient_ptr->icon_manager_ptr,
        NULL,
        icon_ptr->wl_surface_ptr);
    if (NULL == icon_ptr->toplevel_icon_ptr) {
        bs_log(BS_ERROR, "Failed  zwlmaker_icon_manager_v1_get_toplevel_icon"
               "(%p, NULL, %p).", wlclient_ptr->icon_manager_ptr,
               icon_ptr->wl_surface_ptr);
        wlclient_icon_destroy(icon_ptr);
        return NULL;
    }

    zwlmaker_toplevel_icon_v1_add_listener(
        icon_ptr->toplevel_icon_ptr,
        &toplevel_icon_listener,
        icon_ptr);
    wl_surface_commit(icon_ptr->wl_surface_ptr);

    wl_display_roundtrip(wlclient_ptr->wl_display_ptr);
    return icon_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the icon state.
 *
 * @param icon_ptr
 */
void wlclient_icon_destroy(wlclient_icon_t *icon_ptr)
{
    if (NULL != icon_ptr->toplevel_icon_ptr) {
        // TODO(kaeser@gubbe.ch): Destroy the icon!
        icon_ptr->toplevel_icon_ptr = NULL;
    }

    if (NULL != icon_ptr->wl_surface_ptr) {
        wl_surface_destroy(icon_ptr->wl_surface_ptr);
        icon_ptr->wl_surface_ptr = NULL;
    }

    free(icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'configure' event: stores the suggested dimensions, sends ACK.
 *
 * @param data_ptr
 * @param zwlmaker_toplevel_icon_v1_ptr
 * @param width
 * @param height
 * @param serial
 */
void handle_toplevel_icon_configure(
    void *data_ptr,
    struct zwlmaker_toplevel_icon_v1 *zwlmaker_toplevel_icon_v1_ptr,
    int32_t width,
    int32_t height,
    uint32_t serial)
{
    wlclient_icon_t *icon_ptr = data_ptr;

    icon_ptr->width = width;
    icon_ptr->height = height;
    bs_log(BS_DEBUG, "Configured icon to %"PRId32" x %"PRId32, width, height);
    zwlmaker_toplevel_icon_v1_ack_configure(
        zwlmaker_toplevel_icon_v1_ptr, serial);

    // Hm...  should do a roundtrip for getting the 'configure' ?
}
/* == End of wlclient.c ==================================================== */
