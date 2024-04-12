/* ========================================================================= */
/**
 * @file lock_mgr.h
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
#ifndef __LOCK_MGR_H__
#define __LOCK_MGR_H__

#include "toolkit/toolkit.h"

/** Forward declaration: State of the session lock manager. */
typedef struct _wlmaker_lock_mgr_t wlmaker_lock_mgr_t;
/** Forward declaration: Lock. */
typedef struct _wlmaker_lock_t wlmaker_lock_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the session lock manager.
 *
 * @param server_ptr
 *
 * @return The session lock manager handler or NULL on error.
 */
wlmaker_lock_mgr_t *wlmaker_lock_mgr_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the session lock manager.
 *
 * @param lock_mgr_ptr
 */
void wlmaker_lock_mgr_destroy(wlmaker_lock_mgr_t *lock_mgr_ptr);

/**
 * @returns Pointer to @ref wlmtk_element_t of @ref wlmaker_lock_t::container.
 * */
wlmtk_element_t *wlmaker_lock_element(wlmaker_lock_t *lock_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LOCK_MGR_H__ */
/* == End of lock_mgr.h ==================================================== */
