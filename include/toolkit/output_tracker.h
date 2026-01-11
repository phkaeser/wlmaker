/* ========================================================================= */
/**
 * @file output_tracker.h
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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
#ifndef __WLMAKER_OUTPUT_TRACKER_H__
#define __WLMAKER_OUTPUT_TRACKER_H__

#include <libbase/libbase.h>

struct wlr_output;
struct wlr_output_layout;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the output tracker. */
typedef struct _wlmtk_output_tracker_t wlmtk_output_tracker_t;

/**
 * Called when `wlr_output_ptr` got added to the layout.
 *
 * @param wlr_output_ptr
 * @param ud_ptr The `userdata_ptr` of @ref wlmtk_output_tracker_create.
 *
 * @return NULL for error, or a pointer that will be passed to
 *     @ref wlmtk_output_tracker_output_update_callback_t, respectively to
 *     @ref wlmtk_output_tracker_output_destroy_callback_t.
 */
typedef void *(*wlmtk_output_tracker_output_create_callback_t)(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);

/**
 * Called on layout updates, when `wlr_output_ptr` remains in layout.
 *
 * That can happen eg. when resolution or position changes; or when an
 * unrelated output is added or removed.
 *
 * @param wlr_output_ptr
 * @param ud_ptr
 * @param output_ptr
 */
typedef void (*wlmtk_output_tracker_output_update_callback_t)(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

/**
 * Called when `wlr_output_ptr` got removed from the layout.
 *
 * @param wlr_output_ptr
 * @param ud_ptr
 * @param output_ptr
 */
typedef void (*wlmtk_output_tracker_output_destroy_callback_t)(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

/**
 * Creates an output tracker.
 *
 * @param wlr_output_layout_ptr
 * @param userdata_ptr        Will be passed to the callbacks below.
 * @param create_fn
 * @param update_fn
 * @param destroy_fn
 *
 * @return A tracker handle, or NULL on error.
 */
wlmtk_output_tracker_t *wlmtk_output_tracker_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    void *userdata_ptr,
    wlmtk_output_tracker_output_create_callback_t create_fn,
    wlmtk_output_tracker_output_update_callback_t update_fn,
    wlmtk_output_tracker_output_destroy_callback_t destroy_fn);

/**
 * Destroys the output tracker.
 *
 * @ref wlmtk_output_tracker_output_destroy_callback_t will be called for any
 * yet-remaining output.
 *
 * @param tracker_ptr
 */
void wlmtk_output_tracker_destroy(wlmtk_output_tracker_t *tracker_ptr);

/**
 * Returns the "created output" for `wlr_output_ptr`.
 *
 * @param tracker_ptr
 * @param wlr_output_ptr
 *
 * @return Return value of @ref wlmtk_output_tracker_output_create_callback_t
 *     that was called for the value of `wlr_output_ptr`. Or NULL, if
 *     `wlr_output_ptr` is not tracked.
 */
void *wlmtk_output_tracker_get_output(
    wlmtk_output_tracker_t *tracker_ptr,
    struct wlr_output *wlr_output_ptr);

/** Returns @ref wlmtk_output_tracker_t::wlr_output_layout_ptr. */
struct wlr_output_layout *wlmtk_output_tracker_get_layout(
    wlmtk_output_tracker_t *tracker_ptr);

/** Output tracker unit test. */
extern const bs_test_case_t wlmtk_output_tracker_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_OUTPUT_TRACKER_H__
/* == End of output_tracker.h ============================================== */
