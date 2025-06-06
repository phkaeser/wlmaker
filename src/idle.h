/* ========================================================================= */
/**
 * @file idle.h
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
#ifndef __IDLE_H__
#define __IDLE_H__

#include <stdbool.h>

/** Forward declaration: Idle monitor handle. */
typedef struct _wlmaker_idle_monitor_t wlmaker_idle_monitor_t;

#include "server.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the idle monitor.
 *
 * @param server_ptr
 *
 * @return Handle of the idle monitor or NULL on error.
 */
wlmaker_idle_monitor_t *wlmaker_idle_monitor_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the idle monitor.
 *
 * @param idle_monitor_ptr
 */
void wlmaker_idle_monitor_destroy(wlmaker_idle_monitor_t *idle_monitor_ptr);

/**
 * Resets the idle monitor: For example, when a key is pressed.
 *
 * @param idle_monitor_ptr
 */
void wlmaker_idle_monitor_reset(wlmaker_idle_monitor_t *idle_monitor_ptr);

/**
 * Executes the configured 'Command' for locking. Overrides inhibits.
 *
 * @param idle_monitor_ptr
 *
 * @return true on success.
 */
bool wlmaker_idle_monitor_lock(wlmaker_idle_monitor_t *idle_monitor_ptr);

/**
 * Inhibits locking: Increases inhibitor counter, which will prevent locking
 * when the idle timer expires. @see wlmaker_idle_monitor_uninhibit for
 * releasing the inhibitor.
 *
 * @param idle_monitor_ptr
 */
void wlmaker_idle_monitor_inhibit(wlmaker_idle_monitor_t *idle_monitor_ptr);

/**
 * Uninhibits locking: Decreases the counter. If 0, and no other inhibitors
 * found, an expired idle timer will lock.
 *
 * @param idle_monitor_ptr
 */
void wlmaker_idle_monitor_uninhibit(wlmaker_idle_monitor_t *idle_monitor_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __IDLE_H__ */
/* == End of idle.h ======================================================== */
