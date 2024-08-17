/* ========================================================================= */
/**
 * @file pointer_tracking.h
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
#ifndef __POINTER_TRACKING_H__
#define __POINTER_TRACKING_H__

/** Forward declaration: Pointer tracking handle. */
typedef struct _wlmaker_pointer_tracking_t wlmaker_pointer_tracking_t;
/** Forward declaration: Tracker object. */
typedef struct _wlmaker_pointer_tracker_t wlmaker_pointer_tracker_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a pointer tracking.
 *
 * @param wl_display_ptr
 *
 * @return The handle of the pointer tracking or NULL on error. Must be
 *     destroyed by calling @ref wlmaker_pointer_tracking_destroy.
 */
wlmaker_pointer_tracking_t *wlmaker_pointer_tracking_create(
    struct wl_display *wl_display_ptr);

/**
 * Destroys the pointer tracking.
 *
 * @param tracking_ptr
 */
void wlmaker_pointer_tracking_destroy(
    wlmaker_pointer_tracking_t *tracking_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __POINTER_TRACKING_H__ */
/* == End of pointer_tracking.h ============================================ */
