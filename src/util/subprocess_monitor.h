/* ========================================================================= */
/**
 * @file subprocess_monitor.h
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
#ifndef __WLMAKER_UTIL_SUBPROCESS_MONITOR_H__
#define __WLMAKER_UTIL_SUBPROCESS_MONITOR_H__

#include <libbase/libbase.h>
#include <stdbool.h>

/** Forward definition for the subprocess monitor. */
typedef struct _wlm_util_subprocess_monitor_t wlm_util_subprocess_monitor_t;

struct wl_event_loop;
struct wlm_util_subprocess;

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
typedef void (*wlm_util_subprocess_terminated_callback_t)(
    void *userdata_ptr,
    struct wlm_util_subprocess *subprocess_handle_ptr,
    int state,
    int code);

/**
 * Creates the subprocess monitor.
 *
 * The subprocess monitor keeps references to all entrusted subprocesses, reads
 * their output with appropriate logging, and issues a callback if and when
 * the subprocess is terminated.
 *
 * @param wl_event_loop_ptr
 *
 * @return Pointer to the subprocess monitor or NULL on error.
 */
wlm_util_subprocess_monitor_t* wlm_util_subprocess_monitor_create(
    struct wl_event_loop *wl_event_loop_ptr);

/**
 * Destroys the subprocess monitor.
 *
 * @param monitor_ptr
 */
void wlm_util_subprocess_monitor_destroy(
    wlm_util_subprocess_monitor_t *monitor_ptr);

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
 * @param stdout_dynbuf_ptr
 *
 * @return A pointer to the created subprocess handle or NULL on error.
 */
struct wlm_util_subprocess *wlm_util_subprocess_monitor_entrust(
    wlm_util_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr,
    wlm_util_subprocess_terminated_callback_t terminated_callback,
    void *userdata_ptr,
    bs_dynbuf_t *stdout_dynbuf_ptr);

/**
 * Releases the reference held on `subprocess_handle_ptr`. Once the subprocess
 * terminates, all corresponding resources will be freed.
 *
 * @param monitor_ptr
 * @param subprocess_handle_ptr
 */
void wlm_util_subprocess_monitor_cede(
    wlm_util_subprocess_monitor_t *monitor_ptr,
    struct wlm_util_subprocess *subprocess_handle_ptr);

/**
 * Starts, entrust and cedes a subprocess to the monitor.
 *
 * A convenience wrapper for `bs_subprocess_start`,
 * @ref wlm_util_subprocess_monitor_entrust and
 * @ref wlm_util_subprocess_monitor_cede.
 *
 * @param monitor_ptr
 * @param subprocess_ptr      Subprocess. Ignored, if NULL. Takes ownership.
 *
 * @return true on success.
 */
bool wlm_util_subprocess_monitor_run(
    wlm_util_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_UTIL_SUBPROCESS_MONITOR_H__ */
/* == End of subprocess_monitor.h ========================================== */
