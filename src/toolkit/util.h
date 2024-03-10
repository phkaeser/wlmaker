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

/** Unit test cases. */
extern const bs_test_case_t wlmtk_util_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_UTIL_H__ */
/* == End of util.h ======================================================== */
