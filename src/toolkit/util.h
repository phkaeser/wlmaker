/* ========================================================================= */
/**
 * @file util.h
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
#ifndef __WLMTK_UTIL_H__
#define __WLMTK_UTIL_H__

#include <libbase/libbase.h>
#include <wayland-server-core.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Information regarding a client. Drawn from `struct wl_client`. */
typedef struct {
    /** Process ID. */
    pid_t                     pid;
    /** User ID. */
    uid_t                     uid;
    /** Group ID. */
    gid_t                     gid;
} wlmtk_util_client_t;

/** Record for recording a signal, suitable for unit testing. */
typedef struct {
    /** Listener that will get connected to the signal. */
    struct wl_listener        listener;
    /** Counts number of calls since connect or last clear. */
    size_t                    calls;
    /** The |data_ptr| argument of the most recent call. */
    void                      *last_data_ptr;
} wlmtk_util_test_listener_t;

/**
 * Iterates over `list_ptr` and calls func() for each element.
 *
 * Permits removal of `link_ptr` in `func()`. Similar to wl_list_for_each_safe,
 * but not as a macro, and returning true if all invocations succeeded.
 *
 * @return true if none of the `func()` invocations returned false.
 */
bool wlmtk_util_wl_list_for_each(
    struct wl_list *list_ptr,
    bool (*func)(struct wl_list *link_ptr, void *ud_ptr),
    void *ud_ptr);

/**
 * Sets |notifier_func| as the notifier for |listener_ptr|, and registers it
 * with |signal_ptr|.
 *
 * This is merely a convenience helper for the usual two-liner of boilerplate.
 * To disconnect from the listener signal, call `wl_list_remove` on a reference
 * to the `link` element of `*listener_ptr`.
 *
 * @param signal_ptr
 * @param listener_ptr
 * @param notifier_func
 */
// TODO(kaeser@gubbe.ch): Either swap arguments (listener first) or rename,
// eg. . wlm_util_connect_signal_to_listener(...).
void wlmtk_util_connect_listener_signal(
    struct wl_signal *signal_ptr,
    struct wl_listener *listener_ptr,
    void (*notifier_func)(struct wl_listener *, void *));

/**
 * Disconnects a listener from the signal.
 *
 * Does that in a safe way: Will only disconnect, if the `link` is set.
 *
 * @param listener_ptr
 */
void wlmtk_util_disconnect_listener(
    struct wl_listener *listener_ptr);

/**
 * Connects test listener to signal. @see wlmtk_util_connect_listener_signal.
 *
 * @param signal_ptr
 * @param test_listener_ptr
 */
void wlmtk_util_connect_test_listener(
    struct wl_signal *signal_ptr,
    wlmtk_util_test_listener_t *test_listener_ptr);

/**
 * Disconnects a test listener.
 *
 * @param test_listener_ptr
 */
void wlmtk_util_disconnect_test_listener(
    wlmtk_util_test_listener_t *test_listener_ptr);

/**
 * Clears @ref wlmtk_util_test_listener_t::calls and
 * @ref wlmtk_util_test_listener_t::last_data_ptr.
 *
 * @param test_listener_ptr
 */
void wlmtk_util_clear_test_listener(
    wlmtk_util_test_listener_t *test_listener_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_util_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_UTIL_H__ */
/* == End of util.h ======================================================== */
