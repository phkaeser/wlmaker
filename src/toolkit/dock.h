/* ========================================================================= */
/**
 * @file dock.h
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
#ifndef __WLMTK_DOCK_H__
#define __WLMTK_DOCK_H__

#include "env.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Dock handle. */
typedef struct _wlmtk_dock_t wlmtk_dock_t;

/**
 * Creates a dock. A dock contains icons, launchers and the likes.
 *
 * The dock is an implementation of a @ref wlmtk_panel_t.
 *
 * @param env_ptr
 *
 * @return The dock handle, or NULL on error. Must be destroyed calling
 *     @ref wlmtk_dock_destroy.
 */
wlmtk_dock_t *wlmtk_dock_create(wlmtk_env_t *env_ptr);

/**
 * Destroys the dock.
 *
 * @param dock_ptr
 */
void wlmtk_dock_destroy(wlmtk_dock_t *dock_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_DOCK_H__ */
/* == End of dock.h ======================================================== */
