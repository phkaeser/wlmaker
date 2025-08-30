/* ========================================================================= */
/**
 * @file input_observation.h
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
#ifndef __INPUT_OBSERVATION_H__
#define __INPUT_OBSERVATION_H__

/** Forward declaration: Input observation manager handle. */
typedef struct _wlmaker_input_observation_manager_t wlmaker_input_observation_manager_t;
/** Forward declaration: Observer handle. */
typedef struct _wlmaker_input_position_observer_t wlmaker_input_position_observer_t;

struct wl_display;
struct wlr_cursor;
struct wlr_seat;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates an input observation manager.
 *
 * @param wl_display_ptr
 * @param wlr_seat_ptr
 * @param wlr_cursor_ptr
 *
 * @return The handle of the input observation or NULL on error. Must be
 *     destroyed by calling @ref wlmaker_input_observation_manager_destroy.
 */
wlmaker_input_observation_manager_t *wlmaker_input_observation_manager_create(
    struct wl_display *wl_display_ptr,
    struct wlr_seat *wlr_seat_ptr,
    struct wlr_cursor *wlr_cursor_ptr);

/**
 * Destroys the input observation manager.
 *
 * @param manager_ptr
 */
void wlmaker_input_observation_manager_destroy(
    wlmaker_input_observation_manager_t *manager_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __INPUT_OBSERVATION_H__ */
/* == End of input_observation.h =========================================== */
