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

/** Forward declaration: Idle monitor handle. */
typedef struct _wlmaker_idle_monitor_t wlmaker_idle_monitor_t;

#include "server.h"

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

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __IDLE_H__ */
/* == End of idle.h ======================================================== */
