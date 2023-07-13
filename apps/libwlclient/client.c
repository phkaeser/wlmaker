/* ========================================================================= */
/**
 * @file client.c
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

#include "libwlclient.h"

#include <errno.h>
#include <poll.h>
#include <stdarg.h>

#include <wayland-client.h>
#include "wlmaker-icon-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/* == Declarations ========================================================= */

/** State of the wayland client. */
struct _wlclient_t {
    /** Shareable attributes. */
    wlclient_attributes_t     attributes;

    /** Registry singleton for the above connection. */
    struct wl_registry        *wl_registry_ptr;
};

/** Descriptor for a wayland object to bind to. */
typedef struct {
    /** The interface definition. */
    const struct wl_interface *wl_interface_ptr;
    /** Version desired to bind to. */
    uint32_t                  desired_version;
    /** Offset of the bound interface, relative to `wlclient_t`. */
    size_t                    bound_ptr_offset;
} object_t;

static void wl_to_bs_log(
    const char *fmt,
    va_list args);

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

/* == Data ================================================================= */

/** Listener for the registry, taking note of registry updates. */
static const struct wl_registry_listener registry_listener = {
    .global = handle_global_announce,
    .global_remove = handle_global_remove,
};

/** List of wayland objects we want to bind to. */
static const object_t objects[] = {
    { &wl_compositor_interface, 4,
      offsetof(wlclient_attributes_t, wl_compositor_ptr) },
    { &wl_shm_interface, 1,
      offsetof(wlclient_attributes_t, wl_shm_ptr) },
    { &xdg_wm_base_interface, 1,
      offsetof(wlclient_attributes_t, xdg_wm_base_ptr) },
    { &zwlmaker_icon_manager_v1_interface, 1,
      offsetof(wlclient_attributes_t, icon_manager_ptr) },
    { NULL, 0, 0 }  // sentinel.
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_t *wlclient_create(const char *app_id_ptr)
{
    wlclient_t *wlclient_ptr = logged_calloc(1, sizeof(wlclient_t));
    if (NULL == wlclient_ptr) return NULL;
    wl_log_set_handler_client(wl_to_bs_log);

    if (NULL != app_id_ptr) {
        wlclient_ptr->attributes.app_id_ptr = logged_strdup(app_id_ptr);
        if (NULL == wlclient_ptr->attributes.app_id_ptr) {
            wlclient_destroy(wlclient_ptr);
            return NULL;
        }
    }

    wlclient_ptr->attributes.wl_display_ptr = wl_display_connect(NULL);
    if (NULL == wlclient_ptr->attributes.wl_display_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_connect(NULL).");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    wlclient_ptr->wl_registry_ptr = wl_display_get_registry(
        wlclient_ptr->attributes.wl_display_ptr);
    if (NULL == wlclient_ptr->wl_registry_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_get_registry(%p).",
               wlclient_ptr->wl_registry_ptr);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    if (0 != wl_registry_add_listener(
            wlclient_ptr->wl_registry_ptr,
            &registry_listener,
            &wlclient_ptr->attributes)) {
        bs_log(BS_ERROR, "Failed wl_registry_add_listener(%p, %p, %p).",
               wlclient_ptr->wl_registry_ptr,
               &registry_listener,
               &wlclient_ptr->attributes);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    wl_display_roundtrip(wlclient_ptr->attributes.wl_display_ptr);

    if (NULL == wlclient_ptr->attributes.wl_compositor_ptr) {
        bs_log(BS_ERROR, "'wl_compositor' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (NULL == wlclient_ptr->attributes.wl_shm_ptr) {
        bs_log(BS_ERROR, "'wl_shm' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (NULL == wlclient_ptr->attributes.xdg_wm_base_ptr) {
        bs_log(BS_ERROR, "'xdg_wm_base' interface not found on Wayland.");
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }

    return wlclient_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_destroy(wlclient_t *wlclient_ptr)
{
    if (NULL != wlclient_ptr->wl_registry_ptr) {
        wl_registry_destroy(wlclient_ptr->wl_registry_ptr);
        wlclient_ptr->wl_registry_ptr = NULL;
    }

    if (NULL != wlclient_ptr->attributes.wl_display_ptr) {
        wl_display_disconnect(wlclient_ptr->attributes.wl_display_ptr);
        wlclient_ptr->attributes.wl_display_ptr = NULL;
    }

    if (NULL != wlclient_ptr->attributes.app_id_ptr) {
        // Cheated when saying it's const...
        free((char*)wlclient_ptr->attributes.app_id_ptr);
        wlclient_ptr->attributes.app_id_ptr = NULL;
    }

    free(wlclient_ptr);
}

/* ------------------------------------------------------------------------- */
const wlclient_attributes_t *wlclient_attributes(
    const wlclient_t *wlclient_ptr)
{
    return &wlclient_ptr->attributes;
}

/* ------------------------------------------------------------------------- */
// TODO(kaeser@gubbe.ch): Clean up.
void wlclient_run(wlclient_t *wlclient_ptr)
{
    do {

        while (0 != wl_display_prepare_read(wlclient_ptr->attributes.wl_display_ptr)) {
            if (0 > wl_display_dispatch_pending(wlclient_ptr->attributes.wl_display_ptr)) {
                bs_log(BS_ERROR | BS_ERRNO,
                       "Failed wl_display_dispatch_pending(%p)",
                       wlclient_ptr->attributes.wl_display_ptr);
                break;   // Error (?)
            }
        }

        if (0 > wl_display_flush(wlclient_ptr->attributes.wl_display_ptr)) {
            if (EAGAIN != errno) {
                bs_log(BS_ERROR | BS_ERRNO,
                       "Failed wl_display_flush(%p)", wlclient_ptr->attributes.wl_display_ptr);
                wl_display_cancel_read(wlclient_ptr->attributes.wl_display_ptr);
                break;  // Error!
            }
        }

        struct pollfd pollfds;
        pollfds.fd = wl_display_get_fd(wlclient_ptr->attributes.wl_display_ptr);
        pollfds.events = POLLIN;
        pollfds.revents = 0;
        int rv = poll(&pollfds, 1, 100);
        if (0 > rv && EINTR != errno) {
            bs_log(BS_ERROR | BS_ERRNO, "Failed poll(%p, 1, 100)", &pollfds);
            wl_display_cancel_read(wlclient_ptr->attributes.wl_display_ptr);
            break;  // Error!
        }

        if (pollfds.revents & POLLIN) {
            if (0 > wl_display_read_events(wlclient_ptr->attributes.wl_display_ptr)) {
                bs_log(BS_ERROR | BS_ERRNO, "Failed wl_display_read_events(%p)",
                       wlclient_ptr->attributes.wl_display_ptr);
                break;  // Error!
            }
        } else {
            wl_display_cancel_read(wlclient_ptr->attributes.wl_display_ptr);
        }

        if (0 > wl_display_dispatch_pending(wlclient_ptr->attributes.wl_display_ptr)) {
            bs_log(BS_ERROR | BS_ERRNO,
                   "Failed wl_display_dispatch_queue_pending(%p)",
                   wlclient_ptr->attributes.wl_display_ptr);

            int err = wl_display_get_error(wlclient_ptr->attributes.wl_display_ptr);
            if (0 != err) {
                bs_log(BS_ERROR, "Display error %d", err);
            }
            uint32_t id;
            const struct wl_interface *wl_interface_ptr;
            uint32_t perr = wl_display_get_protocol_error(
                wlclient_ptr->attributes.wl_display_ptr, &wl_interface_ptr, &id);
            if (0 != perr) {
                bs_log(BS_ERROR,
                       "Protocol error %"PRIu32", interface %s id %"PRIu32,
                       perr, wl_interface_ptr->name, id);
            }
            break;  // Error!
        }

    } while (true);

}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Redirects a wayland log call into s_log.
 *
 * @param fmt_ptr
 * @param args
 */
void wl_to_bs_log(
    const char *fmt_ptr,
    va_list args)
{
    bs_log_vwrite(BS_ERROR, __FILE__, __LINE__, fmt_ptr, args);
}

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

/* == End of client.c ====================================================== */
