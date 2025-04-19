/* ========================================================================= */
/**
 * @file lock.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __WLMTK_LOCK_H__
#define __WLMTK_LOCK_H__

/** Forward declaration: Lock. */
typedef struct _wlmtk_lock_t wlmtk_lock_t;

#include "element.h"
#include "env.h"
#include "root.h"  // IWYU pragma: keep

/** Forward declaration: Session lock handle. */
struct wlr_session_lock_v1;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a session lock handle.
 *
 * @param wlr_session_lock_v1_ptr
 * @param root_ptr
 * @param env_ptr
 *
 * @return The lock handle or NULL on error.
 */
wlmtk_lock_t *wlmtk_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    wlmtk_root_t *root_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the session lock handle.
 *
 * @param lock_ptr
 */
void wlmtk_lock_destroy(wlmtk_lock_t *lock_ptr);

/**
 * @returns Pointer to @ref wlmtk_element_t of @ref wlmtk_lock_t::container.
 * */
wlmtk_element_t *wlmtk_lock_element(wlmtk_lock_t *lock_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_LOCK_H__ */
/* == End of lock.h ======================================================== */
