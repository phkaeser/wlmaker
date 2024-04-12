/* ========================================================================= */
/**
 * @file root.h
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
#ifndef __ROOT_H__
#define __ROOT_H__

/** Forward declaration: Root element (technically: container). */
typedef struct _wlmaker_root_t wlmaker_root_t;

#include "toolkit/toolkit.h"

#include "lock_mgr.h"

/** Forward declaration: Wlroots scene. */
struct wlr_scene;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the root element.
 *
 * @param wlr_scene_ptr
 * @param env_ptr
 *
 * @return Handle of the root element or NULL on error.
 */
wlmaker_root_t *wlmaker_root_create(
    struct wlr_scene *wlr_scene_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the root element.
 *
 * @param root_ptr
 */
void wlmaker_root_destroy(wlmaker_root_t *root_ptr);

/**
 * Locks the root, using the provided lock.
 *
 * The root must not be locked already. If locked successfully, the root will
 * keep a reference to `lock_ptr`. The lock must call @ref wlmaker_root_unlock
 * to unlock root, and for releasing the reference.
 *
 * @param root_ptr
 * @param lock_ptr
 *
 * @return Whether the lock was established.
 */
bool wlmaker_root_lock(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr);

/**
 * Unlocks the root, and releases the reference from @ref wlmaker_root_lock.
 *
 * Unlocking can only be done with `lock_ptr` matching the `lock_ptr` argument
 * from @ref wlmaker_root_lock.
 *
 * @param root_ptr
 * @param lock_ptr
 *
 * @return Whether the lock was lifted.
 */
bool wlmaker_root_unlock(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr);

/**
 * Releases the lock reference, but keeps the root locked.
 *
 * This is in accordance with the session lock protocol specification [1],
 * stating the session should remain locked if the client dies.
 * This call is a no-op if `lock_ptr` is not currently the lock of `root_ptr`.
 *
 * [1] https://wayland.app/protocols/ext-session-lock-v1
 *
 * @param root_ptr
 * @param lock_ptr
 */
void wlmaker_root_lock_unreference(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr);

/**
 * Temporary: Set the lock surface, so events get passed correctly.
 *
 * TODO(kaeser@gubbe.ch): Remove the method, events should get passed via
 * the container.
 *
 * @param root_ptr
 * @param surface_ptr
 */
void wlmaker_root_set_lock_surface(
    wlmaker_root_t *root_ptr,
    wlmtk_surface_t *surface_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ROOT_H__ */
/* == End of root.h ======================================================== */
