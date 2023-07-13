/* ========================================================================= */
/**
 * @file libwlclient.h
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
#ifndef __LIBWLCLIENT_H__
#define __LIBWLCLIENT_H__

#include <inttypes.h>
#include <stdbool.h>
#include <libbase/libbase.h>

/** Forward declaration: Wayland client handle. */
typedef struct _wlclient_t wlclient_t;

#include "icon.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Accessor to 'public' client attributes. */
typedef struct {
    /** Wayland display connection. */
    struct wl_display         *wl_display_ptr;
    /** The bound compositor interface. */
    struct wl_compositor      *wl_compositor_ptr;
    /** The bound SHM interface. */
    struct wl_shm             *wl_shm_ptr;
    /** The bound XDG wm_base interface. */
    struct xdg_wm_base        *xdg_wm_base_ptr;
    /** The bound Toplevel Icon Manager. Will be NULL if not supported. */
    struct zwlmaker_icon_manager_v1 *icon_manager_ptr;

    /** Application ID, as a string. Or NULL, if not set. */
    const char                *app_id_ptr;
} wlclient_attributes_t;

/**
 * Creates a wayland client for simple buffer interactions.
 *
 * @param app_id_ptr          Application ID or NULL if not set.
 *
 * @return The client state, or NULL on error. The state needs to be free'd
 *     via @ref wlclient_destroy.
 */
wlclient_t *wlclient_create(const char *app_id_ptr);

/**
 * Destroys the wayland client, as created by @ref wlclient_create.
 *
 * @param wlclient_ptr
 */
void wlclient_destroy(wlclient_t *wlclient_ptr);

/**
 * Gets the client attributes.
 *
 * @param wlclient_ptr
 *
 * @return A pointer to the attributes.
 */
const wlclient_attributes_t *wlclient_attributes(
    const wlclient_t *wlclient_ptr);

/**
 * Runs the client's mainloop.
 *
 * @param wlclient_ptr
 */
void wlclient_run(wlclient_t *wlclient_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LIBWLCLIENT_H__ */
/* == End of libwlclient.h ================================================= */
