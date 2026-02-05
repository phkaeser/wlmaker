/* ========================================================================= */
/**
 * @file wlmnetgraph.c
 *
 * Network usage graph dock-app for wlmaker.
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

#include "wlm_graph_shared.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libbase/libbase.h>

/** Application name. */
static const char _app_name[] = "wlmnetgraph";
/** Application help. */
static const char _app_help[] =
    "Displays network activity as a scrolling graph.\n"
    "\n"
    "Shows three activity categories:\n"
    "  - Receive: incoming traffic (blue)\n"
    "  - Transmit: outgoing traffic (cyan)\n"
    "  - Bidirectional: combined traffic (red)\n"
    "\n"
    "The label displays current throughput. Scale auto-adjusts to peak rate.";

/* == Definitions ========================================================== */

/** Number of network categories tracked. */
#define NET_CATEGORY_COUNT 3

/** Minimum peak rate for scaling (1 MB/s in bytes). */
#define PEAK_RATE_MIN (1024ULL * 1024ULL)

/** Peak rate decay divisor (~1% decay per sample). */
#define PEAK_DECAY_DIVISOR 128

/** Threshold below which peak rate doesn't decay. */
#define PEAK_DECAY_THRESHOLD 1024

/** Line buffer size for /proc/net/dev parsing. */
#define LINE_BUFFER_SIZE 512

/** Number of header lines to skip in /proc/net/dev. */
#define HEADER_LINE_COUNT 2

/** Maximum length of the label string. */
#define LABEL_BUFFER_SIZE 16

/** Bytes per kilobyte. */
#define BYTES_PER_KB 1024ULL

/** Bytes per megabyte. */
#define BYTES_PER_MB (1024ULL * 1024ULL)

/** Bytes per gigabyte. */
#define BYTES_PER_GB (1024ULL * 1024ULL * 1024ULL)

/** Network category indices (maps to heat-map colors). */
enum {
    /** Receive activity. */
    NET_CATEGORY_IN = 0,
    /** Transmit activity. */
    NET_CATEGORY_OUT = 1,
    /** Bidirectional activity. */
    NET_CATEGORY_IN_OUT = 2,
};

/** Scale entry pairing a byte value with its display label. */
typedef struct {
    /** Byte threshold for this scale. */
    unsigned long long bytes;
    /** Display label (e.g., "1 MB/s"). */
    const char *label;
} scale_entry_t;

/** Available scale values (1/10/100 × KB/MB/GB). */
static const scale_entry_t _scales[] = {
    { 1ULL * BYTES_PER_KB, "1 KB/s" },
    { 10ULL * BYTES_PER_KB, "10 KB/s" },
    { 100ULL * BYTES_PER_KB, "100 KB/s" },
    { 1ULL * BYTES_PER_MB, "1 MB/s" },
    { 10ULL * BYTES_PER_MB, "10 MB/s" },
    { 100ULL * BYTES_PER_MB, "100 MB/s" },
    { 1ULL * BYTES_PER_GB, "1 GB/s" },
    { 10ULL * BYTES_PER_GB, "10 GB/s" },
    { 100ULL * BYTES_PER_GB, "100 GB/s" },
};

/** Number of scale entries. */
#define SCALE_COUNT (sizeof(_scales) / sizeof(_scales[0]))

/** Raw rate history entry for regeneration. */
typedef struct {
    /** Receive rate (bytes per interval). */
    unsigned long long rx_rate;
    /** Transmit rate (bytes per interval). */
    unsigned long long tx_rate;
} rate_history_t;

/** State for the network graph (mutable runtime data). */
typedef struct {
    /** Open file handle to /proc/net/dev. */
    FILE *proc_fp;
    /** Previous absolute RX byte count for computing rate. */
    unsigned long long prev_rx_bytes;
    /** Previous absolute TX byte count for computing rate. */
    unsigned long long prev_tx_bytes;
    /** Peak observed rate (for auto-scaling). */
    unsigned long long peak_rate;
    /** Index into _scales for current display scale. */
    size_t scale_index;
    /** Raw rate history for regeneration (ring buffer, newest first). */
    rate_history_t history[WLM_GRAPH_REGENERATE_HISTORY_MAX];
    /** Current write position in history (next slot to write). */
    uint32_t history_index;
    /** Number of valid entries in history. */
    uint32_t history_num;
} netgraph_state_t;

/* == Label ================================================================ */

/* ------------------------------------------------------------------------- */
/**
 * Finds index of smallest scale >= val.
 *
 * @param val   Value to find ceiling scale for.
 * @return Index into _scales array.
 */
static size_t _scale_index_ceil(const unsigned long long val)
{
    for (size_t i = 0; i < SCALE_COUNT; i++) {
        if (_scales[i].bytes >= val) {
            return i;
        }
    }
    // Beyond largest scale, return last index.
    return SCALE_COUNT - 1;
}

/* ------------------------------------------------------------------------- */
/** Returns the scale label. */
static const char *_label_fn(void *app_state)
{
    netgraph_state_t *state = app_state;
    return _scales[state->scale_index].label;
}

/* == Cleanup ============================================================== */

/* ------------------------------------------------------------------------- */
/** Frees all allocated state. */
static void _state_free(void *app_state)
{
    netgraph_state_t *state = app_state;

    if (NULL != state->proc_fp) {
        fclose(state->proc_fp);
        state->proc_fp = NULL;
    }
}

/* == Network statistics =================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Reads network statistics from /proc/net/dev.
 *
 * Parses all interfaces (excluding loopback) and sums RX/TX bytes.
 * Computes rate since last read and scales to 0-255 based on peak rate.
 *
 * @param app_state     App state (netgraph_state_t pointer).
 * @param values        Buffer to fill (may reallocate data/num).
 *
 * @return WLM_GRAPH_READ_OK on success, WLM_GRAPH_READ_ERROR on error.
 */
static wlm_graph_read_result_t _stats_read_fn(void *app_state, wlm_graph_values_t *values)
{
    netgraph_state_t * const state = app_state;
    FILE * const fp = state->proc_fp;

    if (NULL == fp) {
        return WLM_GRAPH_READ_ERROR;
    }

    // Reallocate buffer if size doesn't match.
    if (NET_CATEGORY_COUNT != values->num) {
        uint8_t *new_buf = realloc(values->data, NET_CATEGORY_COUNT);
        if (NULL == new_buf) {
            return WLM_GRAPH_READ_ERROR;
        }
        values->data = new_buf;
        values->num = NET_CATEGORY_COUNT;
    }

    rewind(fp);

    unsigned long long total_rx_bytes = 0;
    unsigned long long total_tx_bytes = 0;

    // Skip header lines in /proc/net/dev.
    char line[LINE_BUFFER_SIZE];
    for (int i = 0; i < HEADER_LINE_COUNT; i++) {
        if (NULL == fgets(line, sizeof(line), fp)) {
            return WLM_GRAPH_READ_ERROR;
        }
    }

    while (NULL != fgets(line, sizeof(line), fp)) {
        // Find interface name (ends with ':').
        char * const colon = strchr(line, ':');
        if (NULL == colon) {
            continue;
        }

        // Extract interface name and skip loopback.
        *colon = '\0';
        char *iface = line;
        while (' ' == *iface) iface++;  // Trim leading spaces.
        if (0 == strcmp(iface, "lo")) {
            continue;
        }

        // Parse stats after colon.
        // Format: rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame
        //         rx_compressed rx_multicast tx_bytes tx_packets ...
        unsigned long long rx_bytes, tx_bytes, skip;
        const int parsed = sscanf(colon + 1,
            "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &rx_bytes, &skip, &skip, &skip, &skip, &skip, &skip, &skip,
            &tx_bytes);
        if (9 == parsed) {
            total_rx_bytes += rx_bytes;
            total_tx_bytes += tx_bytes;
        }
    }

    // Compute rate (bytes since last read).
    unsigned long long rx_rate = 0;
    unsigned long long tx_rate = 0;

    if (total_rx_bytes >= state->prev_rx_bytes) {
        rx_rate = total_rx_bytes - state->prev_rx_bytes;
    }
    if (total_tx_bytes >= state->prev_tx_bytes) {
        tx_rate = total_tx_bytes - state->prev_tx_bytes;
    }

    state->prev_rx_bytes = total_rx_bytes;
    state->prev_tx_bytes = total_tx_bytes;

    // Store raw rates in history for regeneration.
    state->history[state->history_index].rx_rate = rx_rate;
    state->history[state->history_index].tx_rate = tx_rate;
    state->history_index = (state->history_index + 1) % WLM_GRAPH_REGENERATE_HISTORY_MAX;
    if (state->history_num < WLM_GRAPH_REGENERATE_HISTORY_MAX) {
        state->history_num++;
    }

    // Update peak rate for auto-scaling (with decay to adapt to lower rates).
    const unsigned long long total_rate = rx_rate + tx_rate;
    const size_t prev_scale_index = state->scale_index;

    if (total_rate > state->peak_rate) {
        state->peak_rate = total_rate;
        state->scale_index = _scale_index_ceil(state->peak_rate);
    } else if (state->peak_rate > PEAK_DECAY_THRESHOLD) {
        state->peak_rate -= state->peak_rate / PEAK_DECAY_DIVISOR;
        state->scale_index = _scale_index_ceil(state->peak_rate);
    }

    // Ensure minimum peak to avoid division by zero.
    if (state->peak_rate < PEAK_RATE_MIN) {
        state->peak_rate = PEAK_RATE_MIN;
        state->scale_index = _scale_index_ceil(PEAK_RATE_MIN);
    }

    // Scale rates to 0-255 based on peak (clamped to peak).
    // IN_OUT uses min to show bidirectional activity (both directions active).
    const unsigned long long peak = state->peak_rate;
    values->data[NET_CATEGORY_IN] = (uint8_t)((BS_MIN(rx_rate, peak) * 255) / peak);
    values->data[NET_CATEGORY_OUT] = (uint8_t)((BS_MIN(tx_rate, peak) * 255) / peak);
    values->data[NET_CATEGORY_IN_OUT] = (uint8_t)((BS_MIN(rx_rate, tx_rate) * 255) / peak);

    // Request regeneration if scale changed.
    if (state->scale_index != prev_scale_index) {
        return WLM_GRAPH_READ_OK_AND_REGENERATE;
    }

    return WLM_GRAPH_READ_OK;
}

/* ------------------------------------------------------------------------- */
/**
 * Regenerates historical samples at the current scale.
 *
 * @param app_state     App state (netgraph_state_t pointer).
 * @param samples       Array of sample buffers to regenerate (newest first).
 * @param samples_num   Number of samples to regenerate.
 */
static void _regenerate_fn(void *app_state, wlm_graph_values_t *samples, const uint32_t samples_num)
{
    netgraph_state_t * const state = app_state;
    const unsigned long long peak = state->peak_rate;

    uint32_t samples_num_regenerate = 0;

    if (state->history_num > 1) {
        // Determine how many samples have history available (excludes current).
        const uint32_t samples_num_available = state->history_num - 1;
        samples_num_regenerate = BS_MIN(samples_num, samples_num_available);

        // Regenerate samples that have history.
        // samples[0] = newest historical (before current), samples[N-1] = oldest.
        // History index wraps at array size (WLM_GRAPH_REGENERATE_HISTORY_MAX),
        // not history_num. The samples_num_available limit ensures we don't
        // read invalid entries.
        // -2: convert from next-write to most-recent (-1), skip current sample (-1).
        // +WLM_GRAPH_REGENERATE_HISTORY_MAX: ensures modulo works when subtracting.
        const uint32_t history_offset =
            (state->history_index + WLM_GRAPH_REGENERATE_HISTORY_MAX) - 2;

        for (uint32_t i = 0; i < samples_num_regenerate; i++) {
            const uint32_t hist_index =
                (history_offset - i) % WLM_GRAPH_REGENERATE_HISTORY_MAX;
            const rate_history_t *h = &state->history[hist_index];

            // Regenerate scaled values at current peak.
            samples[i].data[NET_CATEGORY_IN] =
                (uint8_t)((BS_MIN(h->rx_rate, peak) * 255) / peak);
            samples[i].data[NET_CATEGORY_OUT] =
                (uint8_t)((BS_MIN(h->tx_rate, peak) * 255) / peak);
            samples[i].data[NET_CATEGORY_IN_OUT] =
                (uint8_t)((BS_MIN(h->rx_rate, h->tx_rate) * 255) / peak);
        }
    }

    // Clear samples without available history.
    for (uint32_t i = samples_num_regenerate; i < samples_num; i++) {
        memset(samples[i].data, 0, samples[i].num);
    }
}

/* == Main program ========================================================= */

/** Main program. */
int main(const int argc, const char **argv)
{
    netgraph_state_t state = {};

    state.proc_fp = fopen("/proc/net/dev", "r");
    if (NULL == state.proc_fp) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed to open /proc/net/dev");
        return EXIT_FAILURE;
    }

    // Prime prev values so first real sample computes proper delta.
    // Reset peak and history after priming (first read sets peak to total bytes).
    {
        wlm_graph_values_t values = {};
        _stats_read_fn(&state, &values);
        free(values.data);
        state.peak_rate = 0;
        state.history_index = 0;
        state.history_num = 0;
    }

    const wlm_graph_app_config_t config = {
        .app_name = _app_name,
        .app_help = _app_help,
        .accumulate_mode = WLM_GRAPH_ACCUMULATE_MODE_INDEPENDENT,
        .stats_read_fn = _stats_read_fn,
        .regenerate_fn = _regenerate_fn,
        .app_state = &state,
        .state_free_fn = _state_free,
        .label_fn = _label_fn,
    };

    return wlm_graph_app_run(argc, argv, &config);
}

/* == End of wlmnetgraph.c ================================================= */
