/* ========================================================================= */
/**
 * @file wlclient.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
#ifndef __WLMAKER_WLCLIENT_H__
#define __WLMAKER_WLCLIENT_H__

#include <inttypes.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>

/** Forward declaration: Wayland client handle. */
typedef struct _wlmcl_client_t wlmcl_client_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * A client's callback, as used in @ref wlmcl_client_register_timer.
 *
 * @param wlmcl_client_ptr
 * @param ud_ptr
 */
typedef void (*wlmcl_client_callback_t)(
    wlmcl_client_t *wlmcl_client_ptr,
    void *ud_ptr);

/** Accessor to 'public' client attributes. */
struct wlmcl_client_attributes {
    /** The bound Input Observation. */
    struct ext_input_observation_manager_v1 *input_observation_manager_ptr;
    /** Wayland display connection. */
    struct wl_display         *wl_display_ptr;
    /** The bound compositor interface. */
    struct wl_compositor      *wl_compositor_ptr;
    /** Pointer state, if & when the seat has the capability. */
    struct wl_pointer         *wl_pointer_ptr;
    /** The bound seat. */
    struct wl_seat            *wl_seat_ptr;
    /** The bound SHM interface. */
    struct wl_shm             *wl_shm_ptr;
    /** The bound cursor shape manager. Will be NULL if not supported. */
    struct wp_cursor_shape_manager_v1 *cursor_shape_manager_ptr;
    /** The bound XDG wm_base interface. */
    struct xdg_wm_base        *xdg_wm_base_ptr;
    /** The bound Toplevel Icon Manager. Will be NULL if not supported. */
    struct zwlmaker_icon_manager_v1 *icon_manager_ptr;
    /** The bound layer shell interface. Will be NULL if not supported. */
    struct zwlr_layer_shell_v1 *layer_shell_ptr;
    /** The bound XDG decoration manager. NULL if not supported. */
    struct zxdg_decoration_manager_v1 *xdg_decoration_manager_ptr;

    /** Application ID, as a string. Or NULL, if not set. */
    const char                *app_id_ptr;
};

/** Events of the client. */
struct wlmcl_client_events {
    /** A key was pressed. */
    struct wl_signal          key;
};

/** Key event. */
struct wlmcl_client_key_event {
    /** The key. */
    xkb_keysym_t              keysym;
    /** Wheter it was pressed (true) or released. */
    bool                      pressed;
};

/**
 * Creates a wayland client for simple buffer interactions.
 *
 * @param app_id_ptr          Application ID or NULL if not set.
 *
 * @return The client state, or NULL on error. The state needs to be free'd
 *     via @ref wlmcl_client_destroy.
 */
wlmcl_client_t *wlmcl_client_create(const char *app_id_ptr);

/**
 * Destroys the wayland client, as created by @ref wlmcl_client_create.
 *
 * @param wlmcl_client_ptr
 */
void wlmcl_client_destroy(wlmcl_client_t *wlmcl_client_ptr);

/**
 * Gets the client attributes.
 *
 * @param wlmcl_client_ptr
 *
 * @return A pointer to the attributes.
 */
const struct wlmcl_client_attributes *wlmcl_client_attributes(
    const wlmcl_client_t *wlmcl_client_ptr);

/** @return A pointer to @ref wlmcl_client_t::events. */
struct wlmcl_client_events *wlmcl_client_events(wlmcl_client_t *wlmcl_client_ptr);

/**
 * Runs the client's mainloop.
 *
 * @param wlmcl_client_ptr
 */
void wlmcl_client_run(wlmcl_client_t *wlmcl_client_ptr);

/**
 * Requests termination of the client-s mainloop. This takes effect only once
 * the mainloop wraps up an iteration.
 *
 * @param wlmcl_client_ptr
 */
void wlmcl_client_request_terminate(wlmcl_client_t *wlmcl_client_ptr);

/**
 * Registers a timer with the client.
 *
 * Once the system clock reaches (or has passed) `target_usec`, `callback` will
 * be called with the provided arguments. This is a one-time registration. For
 * repeated calls, clients need to re-register.
 *
 * @param wlmcl_client_ptr
 * @param target_usec
 * @param callback
 * @param callback_ud_ptr
 *
 * @return true on success.
 */
bool wlmcl_client_register_timer(
    wlmcl_client_t *wlmcl_client_ptr,
    uint64_t target_usec,
    wlmcl_client_callback_t callback,
    void *callback_ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_H__ */
/* == End of wlclient.h ==================================================== */
