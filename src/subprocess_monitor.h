/* ========================================================================= */
/**
 * @file subprocess_monitor.h
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
#ifndef __SUBPROCESS_MONITOR_H__
#define __SUBPROCESS_MONITOR_H__

#include <libbase/libbase.h>

/** Forward definition for the subprocess monitor. */
typedef struct _wlmaker_subprocess_monitor_t wlmaker_subprocess_monitor_t;
/** Forward definition for a subprocess handle. */
typedef struct _wlmaker_subprocess_handle_t wlmaker_subprocess_handle_t;

#include "server.h"  // IWYU pragma: keep
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Callback for then the subprocess is terminated.
 *
 * @param userdata_ptr
 * @param subprocess_handle_ptr
 * @param state
 * @param code
 */
typedef void (*wlmaker_subprocess_terminated_callback_t)(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int state,
    int code);

/**
 * Callback for when window events happened for the subprocess.
 *
 * @param userdata_ptr
 * @param subprocess_handle_ptr
 * @param window_ptr
 */
typedef void (*wlmaker_subprocess_window_callback_t)(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr);

/**
 * Creates the subprocess monitor
 *
 * @param server_ptr
 *
 * @return Pointer to the subprocess monitor or NULL on error.
 */
wlmaker_subprocess_monitor_t* wlmaker_subprocess_monitor_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the subprocess monitor.
 *
 * @param monitor_ptr
 */
void wlmaker_subprocess_monitor_destroy(
    wlmaker_subprocess_monitor_t *monitor_ptr);

/**
 * Passes ownership of the started `subprocess_ptr` to `monitor_ptr`.
 *
 * Also registers a set of callbacks that will be triggered. Permits to keep
 * a central register of all started sub-processes, to monitor for termination,
 * and to link up connecting clients with the sub-processes.
 *
 * @param monitor_ptr
 * @param subprocess_ptr
 * @param terminated_callback
 * @param userdata_ptr
 * @param window_created_callback
 * @param window_mapped_callback
 * @param window_unmapped_callback
 * @param window_destroyed_callback
 * @param stdout_dynbuf_ptr
 *
 * @return A pointer to the created subprocess handle or NULL on error.
 */
wlmaker_subprocess_handle_t *wlmaker_subprocess_monitor_entrust(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr,
    wlmaker_subprocess_terminated_callback_t terminated_callback,
    void *userdata_ptr,
    wlmaker_subprocess_window_callback_t window_created_callback,
    wlmaker_subprocess_window_callback_t window_mapped_callback,
    wlmaker_subprocess_window_callback_t window_unmapped_callback,
    wlmaker_subprocess_window_callback_t window_destroyed_callback,
    bs_dynbuf_t *stdout_dynbuf_ptr);

/**
 * Releases the reference held on `subprocess_handle_ptr`. Once the subprocess
 * terminates, all corresponding resources will be freed.
 *
 * @param monitor_ptr
 * @param subprocess_handle_ptr
 */
void wlmaker_subprocess_monitor_cede(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr);

/** Returns the `bs_subprocess_t` from the @ref wlmaker_subprocess_handle_t. */
bs_subprocess_t *wlmaker_subprocess_from_subprocess_handle(
    wlmaker_subprocess_handle_t *subprocess_handle_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __SUBPROCESS_MONITOR_H__ */
/* == End of subprocess_monitor.h ========================================== */
