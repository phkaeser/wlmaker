/* ========================================================================= */
/**
 * @file wlm_graph_shared.h
 *
 * Shared graph rendering utilities for wlmaker dock-apps.
 *
 * @copyright
 * Copyright 2026 Google LLC
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
#ifndef WLM_GRAPH_SHARED_H
#define WLM_GRAPH_SHARED_H

#include <stdint.h>

/* == Definitions ========================================================== */

/**
 * Recommended history size for apps implementing regenerate_fn.
 * Apps storing raw values for regeneration can use this as their buffer size.
 * 512 supports up to 4x HiDPI with no bezel (64 suffices for 1x).
 */
#define WLM_GRAPH_REGENERATE_HISTORY_MAX 512

/**
 * Sample accumulation mode: method of accumulating samples for display.
 */
typedef enum {
    /**
     * Each value fills from bottom independently, overlapping to create
     * heat-map colors where multiple values coincide. The peak line shows
     * the maximum value across all categories.
     */
    WLM_GRAPH_ACCUMULATE_MODE_INDEPENDENT,
    /**
     * Values stack cumulatively on top of each other, with each category
     * rendered as a distinct layer. The peak line shows the sum of all
     * values (clamped to 255).
     */
    WLM_GRAPH_ACCUMULATE_MODE_STACKED,
} wlm_graph_mode_t;

/**
 * Buffer for graph sample values.
 *
 * Used by stats_read_fn to store per-category usage values (0-255 each).
 * The callback may reallocate the buffer if the size doesn't match.
 */
typedef struct {
    /** Pointer to values buffer (may be NULL initially). */
    uint8_t *data;
    /** Number of elements in the buffer. */
    uint32_t num;
} wlm_graph_values_t;

/**
 * Return value for stats_read_fn callback.
 */
typedef enum {
    /** Error reading stats; sample will be skipped. */
    WLM_GRAPH_READ_ERROR = -1,
    /** Success; sample filled normally. */
    WLM_GRAPH_READ_OK = 0,
    /** Success; also regenerate historical samples (scale changed). */
    WLM_GRAPH_READ_OK_AND_REGENERATE = 1,
} wlm_graph_read_result_t;

/**
 * Configuration for a graph application.
 *
 * Pass to wlm_graph_app_run() to configure the graph behavior.
 */
typedef struct {
    /** Application name (e.g., "wlmcpugraph"). Used for error messages. */
    const char *app_name;
    /** Application help string. Shown in --help output. */
    const char *app_help;

    /** Sample accumulation mode. */
    wlm_graph_mode_t accumulate_mode;

    /**
     * Callback to read stats from the system.
     *
     * Called periodically to fill the values buffer. The callback owns buffer
     * reallocation: if values->num doesn't match the required count,
     * reallocate values->data and update values->num before filling.
     *
     * @param app_state     The app_state pointer from this config.
     * @param values        Buffer to fill (may reallocate data/num).
     * @return WLM_GRAPH_READ_OK on success, WLM_GRAPH_READ_ERROR to skip,
     *         or WLM_GRAPH_READ_OK_AND_REGENERATE to regenerate historical samples.
     */
    wlm_graph_read_result_t (*stats_read_fn)(void *app_state, wlm_graph_values_t *values);

    /**
     * Optional callback to regenerate historical samples after scale change.
     *
     * Called when stats_read_fn returns WLM_GRAPH_READ_OK_AND_REGENERATE. 
     * The callback should fill the samples array with regenerated values at the 
     * new scale. The current sample (that requested regeneration) is NOT included; 
     * it was already filled at the new scale by stats_read_fn. Samples are ordered
     * newest to oldest (index 0 is the sample just before the current one).
     * Samples without available history must have their data cleared.
     *
     * @param app_state     The app_state pointer from this config.
     * @param samples       Array of sample buffers to regenerate (excludes current).
     * @param sample_count  Number of samples in the array.
     */
    void (*regenerate_fn)(void *app_state, wlm_graph_values_t *samples, uint32_t sample_count);

    /** App-specific state pointer, passed to callbacks. */
    void *app_state;

    /**
     * Callback to free app-specific state.
     *
     * Called during cleanup to free resources in app_state.
     *
     * @param app_state     The app_state pointer from this config.
     */
    void (*state_free_fn)(void *app_state);

    /**
     * Optional custom pixel lookup table (256 entries, ARGB format).
     *
     * If non-NULL, overrides the default heat-map LUT. Index 0 maps to lowest
     * intensity (single value active), index 255 to highest (all values active).
     * Each entry should be fully opaque (0xff000000 | color).
     */
    const uint32_t *pixel_lut;

    /**
     * Optional callback to get a label string for display.
     *
     * Called during rendering. The returned string is displayed in the
     * bottom-left corner of the graph. Return NULL to display no label.
     *
     * @param app_state     The app_state pointer from this config.
     * @return Label string (caller retains ownership), or NULL.
     */
    const char *(*label_fn)(void *app_state);
} wlm_graph_app_config_t;

/* == Public API =========================================================== */

/**
 * Runs a graph application.
 *
 * Handles argument parsing, wlclient setup, icon creation, callback
 * registration, main loop, and cleanup. Apps just need to initialize their
 * state and provide a configuration.
 *
 * Graph state is managed internally by this function.
 *
 * @param argc      Argument count.
 * @param argv      Argument vector.
 * @param config    Application configuration.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE.
 */
int wlm_graph_app_run(
    int argc,
    const char **argv,
    const wlm_graph_app_config_t *config);

#endif /* WLM_GRAPH_SHARED_H */
