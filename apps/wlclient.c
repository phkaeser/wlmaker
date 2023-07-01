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

#include <errno.h>
#include <fcntl.h>           /* For O_* constants */
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */

#include <wayland-client.h>
#include "wlmaker-icon-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/* == Declarations ========================================================= */

/** Forward declaration: Icon. */
typedef struct _wlclient_icon_t wlclient_icon_t;

/** All elements contributing to a wl_buffer. */
typedef struct {
    /** Mapped data. */
    void                      *data_ptr;
    /** Shared memory pool. */
    struct wl_shm_pool        *wl_shm_pool_ptr;
    /** Actual wl_buffer. */
    struct wl_buffer          *wl_buffer_ptr;
} wlclient_buffer_t;

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

static wlclient_buffer_t *wlclient_buffer_create(
    wlclient_t *wlclient_ptr,
    int32_t width, int32_t height);
static void wlclient_buffer_destroy(
    wlclient_buffer_t *client_buffer_ptr);
static void handle_wl_buffer_release(
    void *data_ptr,
    struct wl_buffer *wl_buffer_ptr);

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

/** Listener implementation for the `wl_buffer`. */
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = handle_wl_buffer_release,
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
void wlclient_run(wlclient_t *wlclient_ptr)
{
    bool drawn = false;

    do {

        while (0 != wl_display_prepare_read(wlclient_ptr->wl_display_ptr)) {
            if (0 > wl_display_dispatch_pending(wlclient_ptr->wl_display_ptr)) {
                bs_log(BS_ERROR | BS_ERRNO,
                       "Failed wl_display_dispatch_pending(%p)",
                       wlclient_ptr->wl_display_ptr);
                break;   // Error (?)
            }
        }

        if (0 > wl_display_flush(wlclient_ptr->wl_display_ptr)) {
            if (EAGAIN != errno) {
                bs_log(BS_ERROR | BS_ERRNO,
                       "Failed wl_display_flush(%p)", wlclient_ptr->wl_display_ptr);
                wl_display_cancel_read(wlclient_ptr->wl_display_ptr);
                break;  // Error!
            }
        }

        struct pollfd pollfds;
        pollfds.fd = wl_display_get_fd(wlclient_ptr->wl_display_ptr);
        pollfds.events = POLLIN;
        pollfds.revents = 0;
        int rv = poll(&pollfds, 1, 100);
        if (0 > rv && EINTR != errno) {
            bs_log(BS_ERROR | BS_ERRNO, "Failed poll(%p, 1, 100)", &pollfds);
            wl_display_cancel_read(wlclient_ptr->wl_display_ptr);
            break;  // Error!
        }

        if (pollfds.revents & POLLIN) {
            if (0 > wl_display_read_events(wlclient_ptr->wl_display_ptr)) {
                bs_log(BS_ERROR | BS_ERRNO, "Failed wl_display_read_events(%p)",
                       wlclient_ptr->wl_display_ptr);
                break;  // Error!
            }
        } else {
            wl_display_cancel_read(wlclient_ptr->wl_display_ptr);
        }

        // TODO: Well, this needs serious overhaul...
        if (!drawn) {
            wlclient_buffer_t *wlclient_buffer_ptr = wlclient_buffer_create(
                wlclient_ptr,
                wlclient_ptr->icon_ptr->width,
                wlclient_ptr->icon_ptr->height);

            memset(wlclient_buffer_ptr->data_ptr, 0x80, 64 * 64 * 4);


            wl_surface_damage_buffer(
                wlclient_ptr->icon_ptr->wl_surface_ptr,
                0, 0, INT32_MAX, INT32_MAX);
            wl_surface_attach(wlclient_ptr->icon_ptr->wl_surface_ptr,
                              wlclient_buffer_ptr->wl_buffer_ptr, 0, 0);
            wl_surface_commit(wlclient_ptr->icon_ptr->wl_surface_ptr);

            wlclient_buffer_destroy(wlclient_buffer_ptr);

            drawn = true;
        }

        if (0 > wl_display_dispatch_pending(wlclient_ptr->wl_display_ptr)) {
            bs_log(BS_ERROR | BS_ERRNO,
                   "Failed wl_display_dispatch_queue_pending(%p)",
                   wlclient_ptr->wl_display_ptr);

            int err = wl_display_get_error(wlclient_ptr->wl_display_ptr);
            if (0 != err) {
                bs_log(BS_ERROR, "Display error %d", err);
            }
            uint32_t id;
            const struct wl_interface *wl_interface_ptr;
            uint32_t perr = wl_display_get_protocol_error(
                wlclient_ptr->wl_display_ptr, &wl_interface_ptr, &id);
            if (0 != perr) {
                bs_log(BS_ERROR,
                       "Protocol error %"PRIu32", interface %s id %"PRIu32,
                       perr, wl_interface_ptr->name, id);
            }
            break;  // Error!
        }

    } while (true);

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

/* ------------------------------------------------------------------------- */
/**
 * Creates a POSIX shared memory object and allocates `size` bytes to it.
 *
 * @param size
 *
 * @return The file descriptor (a non-negative integer) on success, or -1 on
 * failure.
 */
int shm_alloc(size_t size)
{
    // TODO: Make this dynamic.
    const char *shm_name = "/wlclient_shm_123412341235";

    int fd = shm_open(shm_name, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (0 > fd) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed shm_open(%s, O_RDWR|O_CREAT|O_EXCL, 0600)",
               shm_name);
        return -1;
    }

    if (0 != shm_unlink(shm_name)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed shm_unlink(%d)", fd);
        close(fd);
        return -1;
    }

    while (0 != ftruncate(fd, size)) {
        if (EINTR == errno) continue;  // try again...
        bs_log(BS_ERROR | BS_ERRNO, "Failed ftruncate(%d, %zu)", fd, size);
        close(fd);
        return -1;
    }

    return fd;
}

/* ------------------------------------------------------------------------- */
/** Creates a buffer. */
wlclient_buffer_t *wlclient_buffer_create(
    wlclient_t *wlclient_ptr,
    int32_t width, int32_t height)
{
    wlclient_buffer_t *client_buffer_ptr = logged_calloc(
        1, sizeof(wlclient_buffer_t));
    if (NULL == client_buffer_ptr) return NULL;

    size_t shm_pool_size = width * height * sizeof(uint32_t);
    int fd = shm_alloc(shm_pool_size);
    if (0 >= fd) {
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }
    client_buffer_ptr->data_ptr = mmap(
        NULL, shm_pool_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == client_buffer_ptr->data_ptr) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed mmap(NULL, %zu, "
               "PROT_READ|PROT_WRITE, MAP_SHARED, %d, 0)",
               shm_pool_size, fd);
        close(fd);
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }

    bs_log(BS_WARNING, "FIXME: %p, %d, %zu",
           wlclient_ptr->wl_shm_ptr, fd, shm_pool_size);
    struct wl_shm_pool *wl_shm_pool_ptr = wl_shm_create_pool(
        wlclient_ptr->wl_shm_ptr, fd, shm_pool_size);
    if (NULL == wl_shm_pool_ptr) {
        bs_log(BS_ERROR, "Failed wl_shm_create_pool(%p, %d, %zu)",
               wlclient_ptr->wl_shm_ptr, fd, shm_pool_size);
        close(fd);
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }
    client_buffer_ptr->wl_buffer_ptr = wl_shm_pool_create_buffer(
        wl_shm_pool_ptr, 0,
        width, height,
        width * sizeof(uint32_t),
        WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(wl_shm_pool_ptr);
    close(fd);

    if (NULL == client_buffer_ptr->wl_buffer_ptr) {
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }

    wl_buffer_add_listener(
        client_buffer_ptr->wl_buffer_ptr,
        &wl_buffer_listener,
        client_buffer_ptr);
    return client_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the buffer. */
void wlclient_buffer_destroy(wlclient_buffer_t *client_buffer_ptr)
{
    free(client_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles the `release` notification of the wl_buffer interface. */
static void handle_wl_buffer_release(
    __UNUSED__ void *data_ptr,
    __UNUSED__ struct wl_buffer *wl_buffer_ptr)
{
    // TODO(kaeser@gubbe.ch): Implement.
}

/* == End of wlclient.c ==================================================== */
