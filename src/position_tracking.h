/* ========================================================================= */
/**
 * @file position_tracking.h
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
#ifndef __POSITION_TRACKING_H__
#define __POSITION_TRACKING_H__

/** Forward declaration: Position tracking handle. */
typedef struct _wlmaker_position_tracking_t wlmaker_position_tracking_t;
/** Forward declaration: Tracker object. */
typedef struct _wlmaker_position_tracker_t wlmaker_position_tracker_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a position tracking.
 *
 * @param wl_display_ptr
 * @param wlr_seat_ptr
 * @param wlr_cursor_ptr
 *
 * @return The handle of the position tracking or NULL on error. Must be
 *     destroyed by calling @ref wlmaker_position_tracking_destroy.
 */
wlmaker_position_tracking_t *wlmaker_position_tracking_create(
    struct wl_display *wl_display_ptr,
    struct wlr_seat *wlr_seat_ptr,
    struct wlr_cursor *wlr_cursor_ptr);

/**
 * Destroys the position tracking.
 *
 * @param tracking_ptr
 */
void wlmaker_position_tracking_destroy(
    wlmaker_position_tracking_t *tracking_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __POSITION_TRACKING_H__ */
/* == End of position_tracking.h =========================================== */
