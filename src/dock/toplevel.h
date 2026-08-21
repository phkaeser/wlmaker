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
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct wlmdock_toplevel_handle;

/**
 * Creates a toplevel handle.
 *
 * @return Pointer to the handle, or NULL on error.
 */
struct wlmdock_toplevel_handle *wlmdock_toplevel_handle_create(void);

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
