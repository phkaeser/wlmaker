/* ========================================================================= */
/**
 * @file toplevel.h
 *
 * Maintains handles for listing and manipulating toplevels, as provided by
 * the relevant Wayland protocols. Namely:
 * * https://wayland.app/protocols/ext-foreign-toplevel-list-v1
 * * https://wayland.app/protocols/wlr-foreign-toplevel-management-unstable-v1
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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
#ifndef __WLMAKER_TOPLEVEL_H__
#define __WLMAKER_TOPLEVEL_H__

#include <libbase/libbase.h>
#include <libbase/signal.h>
#include <stdbool.h>

#include "wlclient/wlclient.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmdock_toplevel_tracker;
struct wlmdock_toplevel_handle;

/** Events of the toplevel tracker. */
struct wlmdock_toplevel_tracker_events {
    /** An App ID changed (added, removed, changed). Takes the app ID. */
    struct bs_signal          app_id_changed;
};

/**
 * Creates the toplevel tracker.
 *
 * @return Pointer to the toplevel tracker, or NULL on error.
 */
struct wlmdock_toplevel_tracker *wlmdock_toplevel_tracker_create(void);

/**
 * Destroys the toplevel tracker.
 *
 * @param tracker_ptr
 */
void wlmdock_toplevel_tracker_destroy(
    struct wlmdock_toplevel_tracker *tracker_ptr);

/**
 * Registers an `ext-foreign-toplevel-list-v1` interface for the tracker.
 *
 * @param client_ptr
 * @param tracker_ptr
 *
 * @return true on success.
 */
bool wlmdock_toplevel_register_ext_foreign_toplevel_list(
    wlmcl_client_t *client_ptr,
    struct wlmdock_toplevel_tracker *tracker_ptr);

/** @return @ref wlmdock_toplevel_tracker::events. */
struct wlmdock_toplevel_tracker_events *wlmdock_toplevel_tracker_events(
    struct wlmdock_toplevel_tracker *tracker_ptr);

/**
 * Registers the handle for the app_id tracker.
 *
 * @param tracker_ptr
 * @param handle_ptr          Requires @ref wlmdock_toplevel_handle::app_id_ptr
 *                            to be set.
 *
 * @return true on success.
 */
bool wlmdock_toplevel_tracker_register_app_id(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    struct wlmdock_toplevel_handle *handle_ptr);

/**
 * Unregisters the handle from the app_id tracker.
 *
 * @param tracker_ptr
 * @param handle_ptr          Requires @ref wlmdock_toplevel_handle::app_id_ptr
 *                            to be set.
 */
void wlmdock_toplevel_tracker_unregister_app_id(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    struct wlmdock_toplevel_handle *handle_ptr);

/**
 * Returns @ref wlmdock_toplevel_app_ids::toplevel_handles for `app_id_ptr`.
 *
 * @param tracker_ptr
 * @param app_id_ptr
 *
 * @return The list of toplevel handles, or NULL if no such app id is tracked.
 */
bs_dllist_t *wlmdock_toplevel_tracker_app_id_handles(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    const char *app_id_ptr);

/**
 * Creates a toplevel handle.
 *
 * @param tracker_ptr
 *
 * @return Pointer to the handle, or NULL on error.
 */
struct wlmdock_toplevel_handle *wlmdock_toplevel_handle_create(
    struct wlmdock_toplevel_tracker *tracker_ptr);

/**
 * Destroys the toplevel handle.
 *
 * @param handle_ptr
 */
void wlmdock_toplevel_handle_destroy(
    struct wlmdock_toplevel_handle *handle_ptr);

/**
 * Sets (or clears) the app id of the toplevel.
 *
 * @param handle_ptr
 * @param app_id_ptr          May be NULL.
 *
 * @return true on success.
 */
bool wlmdock_toplevel_handle_set_app_id(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *app_id_ptr);

/**
 * Sets (or clears) the title of the toplevel.
 *
 * @param handle_ptr
 * @param title_ptr           May be NULL.
 *
 * @return true on success.
 */
bool wlmdock_toplevel_handle_set_title(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *title_ptr);

/**
 * Sets (or clears) the identifier of the toplevel.
 *
 * @param handle_ptr
 * @param identifier_ptr      Once set, the identifier must not be changed.
 *
 * @return true on success.
 */

bool wlmdock_toplevel_handle_set_identifier(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *identifier_ptr);

/** Unit test set for toplevel. */
extern const bs_test_set_t wlmdock_toplevel_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_TOPLEVEL_H__
/* == End of toplevel.h ==================================================== */
