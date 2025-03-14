/* ========================================================================= */
/**
 * @file output_manager.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMAKER_OUTPUT_MANAGER_H__
#define __WLMAKER_OUTPUT_MANAGER_H__

#include <wayland-server-core.h>

/** Forward declaration: Handle for output manager. */
typedef struct _wlmaker_output_manager_t wlmaker_output_manager_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Ctor. */
wlmaker_output_manager_t *wlmaker_output_manager_create(
    struct wl_display *wl_display_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_OUTPUT_MANAGER_H__ */
/* == End of output_manager.h ============================================== */
