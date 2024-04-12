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

#include "toolkit/toolkit.h"

/** Forward declaration: Root element (technically: container). */
typedef struct _wlmaker_root_t wlmaker_root_t;

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

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ROOT_H__ */
/* == End of root.h ======================================================== */
