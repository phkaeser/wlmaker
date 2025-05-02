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
#include <signal.h>
#include <stdarg.h>
#include <sys/signalfd.h>

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
    /** Pointer state, if & when the seat has the capability. */
    struct wl_pointer         *wl_pointer_ptr;

    /** List of registered timers. TODO(kaeser@gubbe.ch): Replace with HEAP. */
    bs_dllist_t               timers;

    /** File descriptor to monitor SIGINT. */
    int                       signal_fd;

    /** Whether to keep the client running. */
    volatile bool             keep_running;
};

/** State of a registered timer. */
typedef struct {
    /** Node within the list of timers, see `wlclient_t.timers`. */
    bs_dllist_node_t          dlnode;
    /** Target time, in usec since epoch. */
    uint64_t                  target_usec;
    /** Callback once the timer is triggered. */
    wlclient_callback_t       callback;
    /** Argument to the callback. */
    void                      *callback_ud_ptr;
} wlclient_timer_t;

/** Descriptor for a wayland object to bind to. */
typedef struct {
    /** The interface definition. */
    const struct wl_interface *wl_interface_ptr;
    /** Version desired to bind to. */
    uint32_t                  desired_version;
    /** Offset of the bound interface, relative to `wlclient_t`. */
    size_t                    bound_ptr_offset;
    /** Additional setup for this wayland object. */
    void (*setup)(wlclient_t *client_ptr);
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

static wlclient_timer_t *wlc_timer_create(
    wlclient_t *client_ptr,
    uint64_t target_usec,
    wlclient_callback_t callback,
    void *callback_ud_ptr);
static void wlc_timer_destroy(
    wlclient_timer_t *timer_ptr);

static void wlc_seat_setup(wlclient_t *client_ptr);
static void wlc_seat_handle_capabilities(
    void *data_ptr,
    struct wl_seat *wl_seat_ptr,
    uint32_t capabilities);
static void wlc_seat_handle_name(
    void *data_ptr,
    struct wl_seat *wl_seat_ptr,
    const char *name_ptr);

static void wlc_pointer_handle_enter(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *surface,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y);
static void wlc_pointer_handle_leave(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *surface);
static void wlc_pointer_handle_motion(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y);
static void wlc_pointer_handle_button(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state);
static void wlc_pointer_handle_axis(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value);
static void wlc_pointer_handle_frame(
    void *data,
    struct wl_pointer *wl_pointer);
static void wlc_pointer_handle_axis_source(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis_source);
static void wlc_pointer_handle_axis_stop(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis);
static void wlc_pointer_handle_axis_discrete(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t discrete);

/* == Data ================================================================= */

/** Listener for the registry, taking note of registry updates. */
static const struct wl_registry_listener registry_listener = {
    .global = handle_global_announce,
    .global_remove = handle_global_remove,
};

/** Listeners for the seat. */
static const struct wl_seat_listener wlc_seat_listener = {
    .capabilities = wlc_seat_handle_capabilities,
    .name = wlc_seat_handle_name,
};

/** Listeners for the pointer. */
static const struct wl_pointer_listener wlc_pointer_listener = {
    .enter = wlc_pointer_handle_enter,
    .leave = wlc_pointer_handle_leave,
    .motion = wlc_pointer_handle_motion,
    .button = wlc_pointer_handle_button,
    .axis = wlc_pointer_handle_axis,
    .frame = wlc_pointer_handle_frame,
    .axis_source = wlc_pointer_handle_axis_source,
    .axis_stop = wlc_pointer_handle_axis_stop,
    .axis_discrete = wlc_pointer_handle_axis_discrete,
};

/** List of wayland objects we want to bind to. */
static const object_t objects[] = {
    { &wl_compositor_interface, 4,
      offsetof(wlclient_attributes_t, wl_compositor_ptr), NULL },
    { &wl_shm_interface, 1,
      offsetof(wlclient_attributes_t, wl_shm_ptr), NULL },
    { &xdg_wm_base_interface, 1,
      offsetof(wlclient_attributes_t, xdg_wm_base_ptr), NULL },
    { &wl_seat_interface, 5,
      offsetof(wlclient_attributes_t, wl_seat_ptr), wlc_seat_setup },
    { &zwlmaker_icon_manager_v1_interface, 1,
      offsetof(wlclient_attributes_t, icon_manager_ptr), NULL },
    { NULL, 0, 0, NULL }  // sentinel.
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

    sigset_t signal_set;
    if (sigemptyset(&signal_set)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed sigemptyset(%p)", &signal_set);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (sigaddset(&signal_set, SIGINT)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed sigemptyset(%p, %d)",
               &signal_set, SIGINT);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    if (sigprocmask(SIG_BLOCK, &signal_set, NULL) == -1) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed sigprocmask(SIG_BLOCK, %p, NULL)",
               &signal_set);
        wlclient_destroy(wlclient_ptr);
        return NULL;
    }
    wlclient_ptr->signal_fd = signalfd(-1, &signal_set, SFD_NONBLOCK);
    if (0 >= wlclient_ptr->signal_fd) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed signalfd(-1, %p, SFD_NONBLOCK)",
               &signal_set);
        wlclient_destroy(wlclient_ptr);
        return NULL;
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
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = bs_dllist_pop_front(&wlclient_ptr->timers))) {
        wlc_timer_destroy((wlclient_timer_t*)dlnode_ptr);
    }

    if (NULL != wlclient_ptr->wl_registry_ptr) {
        wl_registry_destroy(wlclient_ptr->wl_registry_ptr);
        wlclient_ptr->wl_registry_ptr = NULL;
    }

    if (NULL != wlclient_ptr->attributes.wl_display_ptr) {
        wl_display_disconnect(wlclient_ptr->attributes.wl_display_ptr);
        wlclient_ptr->attributes.wl_display_ptr = NULL;
    }

    if (0 < wlclient_ptr->signal_fd) {
        close(wlclient_ptr->signal_fd);
        wlclient_ptr->signal_fd = 0;
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
    wlclient_ptr->keep_running = true;
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

        struct pollfd pollfds[2];
        pollfds[0].fd = wl_display_get_fd(wlclient_ptr->attributes.wl_display_ptr);
        pollfds[0].events = POLLIN;
        pollfds[0].revents = 0;

        pollfds[1].fd = wlclient_ptr->signal_fd;
        pollfds[1].events = POLLIN;
        pollfds[1].revents = 0;

        int rv = poll(&pollfds[0], 2, 100);
        if (0 > rv && EINTR != errno) {
            bs_log(BS_ERROR | BS_ERRNO, "Failed poll(%p, 1, 100)", &pollfds);
            wl_display_cancel_read(wlclient_ptr->attributes.wl_display_ptr);
            break;  // Error!
        }

        if (pollfds[0].revents & POLLIN) {
            if (0 > wl_display_read_events(wlclient_ptr->attributes.wl_display_ptr)) {
                bs_log(BS_ERROR | BS_ERRNO, "Failed wl_display_read_events(%p)",
                       wlclient_ptr->attributes.wl_display_ptr);
                break;  // Error!
            }
        } else {
            wl_display_cancel_read(wlclient_ptr->attributes.wl_display_ptr);
        }

        if (pollfds[1].revents & POLLIN) {
            struct signalfd_siginfo siginfo;
            ssize_t rd = read(wlclient_ptr->signal_fd, &siginfo, sizeof(siginfo));
            if (0 > rd) {
                bs_log(BS_ERROR, "Failed read(%d, %p, %zu)",
                       wlclient_ptr->signal_fd, &siginfo, sizeof(siginfo));
                break;
            } else if ((size_t)rd != sizeof(siginfo)) {
                bs_log(BS_ERROR, "Bytes read from signal_fd %zu != %zd",
                       sizeof(siginfo), rd);
                break;
            }
            bs_log(BS_ERROR, "Signal caught: %d", siginfo.ssi_signo);
            wlclient_ptr->keep_running = false;
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

        // Flush the timer queue.
        uint64_t current_usec = bs_usec();
        bs_dllist_node_t *dlnode_ptr;
        while (NULL != (dlnode_ptr = wlclient_ptr->timers.head_ptr) &&
               ((wlclient_timer_t*)dlnode_ptr)->target_usec <= current_usec) {
            bs_dllist_pop_front(&wlclient_ptr->timers);

            wlclient_timer_t *timer_ptr = (wlclient_timer_t*)dlnode_ptr;
            timer_ptr->callback(wlclient_ptr, timer_ptr->callback_ud_ptr);
            wlc_timer_destroy(timer_ptr);
        }

    } while (wlclient_ptr->keep_running);
}

/* ------------------------------------------------------------------------- */
bool wlclient_register_timer(
    wlclient_t *wlclient_ptr,
    uint64_t target_usec,
    wlclient_callback_t callback,
    void *callback_ud_ptr)
{
    wlclient_timer_t *timer_ptr = wlc_timer_create(
        wlclient_ptr, target_usec, callback, callback_ud_ptr);
    return (timer_ptr != NULL);
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

        if (NULL != object_ptr->setup) object_ptr->setup(data_ptr);
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
 * Creates a timer and registers it with the client.
 *
 * @param client_ptr
 * @param target_usec
 * @param callback
 * @param callback_ud_ptr
 *
 * @return A pointer to the created timer, or NULL on error. The pointer must
 *     be destroyed by @ref wlc_timer_destroy.
 */
wlclient_timer_t *wlc_timer_create(
    wlclient_t *client_ptr,
    uint64_t target_usec,
    wlclient_callback_t callback,
    void *callback_ud_ptr)
{
    wlclient_timer_t *timer_ptr = logged_calloc(1, sizeof(wlclient_timer_t));
    if (NULL == timer_ptr) return NULL;

    timer_ptr->target_usec = target_usec;
    timer_ptr->callback = callback;
    timer_ptr->callback_ud_ptr = callback_ud_ptr;

    // TODO(kaeser@gubbe.ch): This should be a HEAP.
    bs_dllist_node_t *dlnode_ptr = client_ptr->timers.head_ptr;
    for (; dlnode_ptr != NULL; dlnode_ptr = dlnode_ptr->next_ptr) {
        wlclient_timer_t *ref_timer_ptr = (wlclient_timer_t *)dlnode_ptr;
        if (timer_ptr->target_usec > ref_timer_ptr->target_usec) continue;
        bs_dllist_insert_node_before(
            &client_ptr->timers, dlnode_ptr, &timer_ptr->dlnode);
    }
    if (NULL == dlnode_ptr) {
        bs_dllist_push_back(&client_ptr->timers, &timer_ptr->dlnode);
    }

    return timer_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the timer. Note: The timer will NOT be unregistered first.
 *
 * @param timer_ptr
 */
void wlc_timer_destroy(wlclient_timer_t *timer_ptr)
{
    free(timer_ptr);
}

/* ------------------------------------------------------------------------- */
/** Set up the seat: Registers the client's seat listeners. */
void wlc_seat_setup(wlclient_t *client_ptr)
{
    wl_seat_add_listener(
        client_ptr->attributes.wl_seat_ptr,
        &wlc_seat_listener,
        client_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the seat's capability updates.
 *
 * Un-/Registers listeners for the pointer, if the capability is available.
 *
 * @param data_ptr
 * @param wl_seat_ptr
 * @param capabilities
 */
void wlc_seat_handle_capabilities(
    void *data_ptr,
    struct wl_seat *wl_seat_ptr,
    uint32_t capabilities)
{
    wlclient_t *client_ptr = data_ptr;

    bool supports_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
    if (supports_pointer && NULL == client_ptr->wl_pointer_ptr) {
        client_ptr->wl_pointer_ptr = wl_seat_get_pointer(wl_seat_ptr);
        wl_pointer_add_listener(
            client_ptr->wl_pointer_ptr,
            &wlc_pointer_listener,
            client_ptr);
    } else if (!supports_pointer && NULL != client_ptr->wl_pointer_ptr) {
        wl_pointer_release(client_ptr->wl_pointer_ptr);
        client_ptr->wl_pointer_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/** Handles the unique identifier callback. */
void wlc_seat_handle_name(
    void *data_ptr,
    struct wl_seat *wl_seat_ptr,
    const char *name_ptr)
{
    bs_log(BS_DEBUG, "Client %p bound to seat %p: %s",
           data_ptr, wl_seat_ptr, name_ptr);
}

/* ------------------------------------------------------------------------- */
/** Called when the client obtains pointer focus. */
void wlc_pointer_handle_enter(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t serial,
    __UNUSED__ struct wl_surface *surface,
    __UNUSED__ wl_fixed_t surface_x,
    __UNUSED__ wl_fixed_t surface_y)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called when the client looses pointer focus. */
void wlc_pointer_handle_leave(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t serial,
    __UNUSED__ struct wl_surface *surface)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon pointer motion. */
void wlc_pointer_handle_motion(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t time,
    __UNUSED__ wl_fixed_t surface_x,
    __UNUSED__ wl_fixed_t surface_y)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon pointer button events. */
void wlc_pointer_handle_button(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t serial,
    __UNUSED__ uint32_t time,
    __UNUSED__ uint32_t button,
    __UNUSED__ uint32_t state)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon axis events. */
void wlc_pointer_handle_axis(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t time,
    __UNUSED__ uint32_t axis,
    __UNUSED__ wl_fixed_t value)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon frame events. */
void wlc_pointer_handle_frame(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon axis source events. */
void wlc_pointer_handle_axis_source(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t axis_source)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Axis stop events. */
void wlc_pointer_handle_axis_stop(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t time,
    __UNUSED__ uint32_t axis)
{
    /* Currently nothing done. */
}

/* ------------------------------------------------------------------------- */
/** Called upon axis click events. */
void wlc_pointer_handle_axis_discrete(
    __UNUSED__ void *data,
    __UNUSED__ struct wl_pointer *wl_pointer,
    __UNUSED__ uint32_t axis,
    __UNUSED__ int32_t discrete)
{
    /* Currently nothing done. */
}

/* == End of client.c ====================================================== */
