/* ========================================================================= */
/**
 * @file wlmcpugraph.c
 *
 * CPU usage graph dock-app for wlmaker.
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wlm_graph_utildefines.h"

/** Application name. */
static const char _app_name[] = "wlmcpugraph";
/** Application help. */
static const char _app_help[] =
    "Displays CPU usage as a scrolling graph.\n"
    "\n"
    "The peak shows maximum CPU usage.\n"
    "\n"
    "Colors below indicate multi-core activity:\n"
    "  - 1 core active (blue)\n"
    "  - 1/4 cores active (cyan)\n"
    "  - 1/2 cores active (green)\n"
    "  - 3/4 cores active (yellow)\n"
    "  - All cores active (red)";

/** Line buffer size for /proc/stat parsing. */
#define LINE_BUFFER_SIZE 256

/* == Definitions ========================================================== */

/**
 * Absolute CPU time values from `/proc/stat`.
 *
 * Uses unsigned long to match `/proc/stat` format and avoid overflow
 * on systems with high uptime.
 */
typedef struct {
    /** Sum of all CPU time fields. */
    unsigned long total;
    /** Idle time (idle + iowait). */
    unsigned long idle;
} cpu_times_t;

/** State for the CPU graph (mutable runtime data). */
typedef struct {
    /** Open file handle to /proc/stat. */
    FILE *proc_fp;
    /** Previous absolute CPU values for computing usage (dynamically allocated). */
    cpu_times_t *cpu_times_prev;
    /** Number of elements in cpu_times_prev. */
    uint32_t cpu_usage_num;
} cpugraph_state_t;

/* == Memory management ==================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Allocates state arrays for the given number of CPUs.
 *
 * @param state         CPU graph state.
 * @param cpu_usage_num     Number of CPUs.
 *
 * @return true on success, false on allocation failure.
 */
static bool _cpu_state_arrays_alloc(cpugraph_state_t *state, const uint32_t cpu_usage_num)
{
    // Free old allocation.
    free(state->cpu_times_prev);
    state->cpu_times_prev = NULL;
    state->cpu_usage_num = 0;

    state->cpu_times_prev = calloc(cpu_usage_num, sizeof(cpu_times_t));
    if (NULL == state->cpu_times_prev) {
        return false;
    }

    state->cpu_usage_num = cpu_usage_num;
    return true;
}

/* ------------------------------------------------------------------------- */
/** Frees all allocated state. */
static void _state_free(void *app_state)
{
    cpugraph_state_t *state = app_state;

    if (NULL != state->proc_fp) {
        fclose(state->proc_fp);
        state->proc_fp = NULL;
    }

    free(state->cpu_times_prev);
    state->cpu_times_prev = NULL;
    state->cpu_usage_num = 0;
}

/* == CPU statistics ======================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Reads CPU statistics from /proc/stat.
 *
 * Uses the pre-opened file handle, counts CPUs, reallocates the buffer if
 * needed, and fills it with per-CPU usage values.
 *
 * @param app_state     App state (cpugraph_state_t pointer).
 * @param values        Buffer to fill (may reallocate data/num).
 *
 * @return WLM_GRAPH_READ_OK on success, WLM_GRAPH_READ_ERROR on error.
 */
static wlm_graph_read_result_t _stats_read_fn(void *app_state, wlm_graph_values_t *values)
{
    cpugraph_state_t * const state = app_state;
    FILE * const fp = state->proc_fp;

    if (NULL == fp) {
        return WLM_GRAPH_READ_ERROR;
    }

    rewind(fp);

    char line[LINE_BUFFER_SIZE];
    uint32_t cpu_count = 0;

    // First pass: count CPUs.
    while (NULL != fgets(line, sizeof(line), fp)) {
        if (0 != strncmp(line, "cpu", 3)) continue;
        if (ISDIGIT(line[3])) {
            cpu_count++;
        }
    }

    if (0 == cpu_count) {
        return WLM_GRAPH_READ_ERROR;
    }

    // Reallocate buffer if size doesn't match.
    if (cpu_count != values->num) {
        uint8_t *new_buf = realloc(values->data, cpu_count);
        if (NULL == new_buf) {
            return WLM_GRAPH_READ_ERROR;
        }
        values->data = new_buf;
        values->num = cpu_count;
    }

    // Reallocate internal state arrays if CPU count changed.
    if (cpu_count != state->cpu_usage_num) {
        if (!_cpu_state_arrays_alloc(state, cpu_count)) {
            return WLM_GRAPH_READ_ERROR;
        }
    }

    // Rewind and read stats, computing usage from previous absolute values.
    rewind(fp);
    uint32_t read_count = 0;
    while (NULL != fgets(line, sizeof(line), fp) && read_count < cpu_count) {
        // Skip the aggregate "cpu" line, look for "cpu0", "cpu1", etc.
        if (0 != strncmp(line, "cpu", 3)) continue;
        if (!ISDIGIT(line[3])) continue;

        unsigned long user, nice, system, idle, iowait, irq, softirq;
        const int fields_parsed = sscanf(
            line, "%*s %lu %lu %lu %lu %lu %lu %lu",
            &user, &nice, &system, &idle, &iowait, &irq, &softirq);
        if (7 == fields_parsed) {
            const unsigned long abs_total = user + nice + system + idle + iowait + irq + softirq;
            const unsigned long abs_idle = idle + iowait;

            cpu_times_t * const prev = &state->cpu_times_prev[read_count];
            uint8_t usage = 0;

            // Compute usage percentage, handling wraparound as zero.
            // Skip if prev is uninitialized (zero) after CPU hotplug realloc.
            if (0 != prev->total &&
                abs_total > prev->total && abs_idle >= prev->idle) {
                const unsigned long total_diff = abs_total - prev->total;
                const unsigned long idle_diff = MIN2(abs_idle - prev->idle, total_diff);
                usage = (uint8_t)(((total_diff - idle_diff) * 255) / total_diff);
            }

            values->data[read_count] = usage;

            // Update previous absolute values for next iteration.
            prev->total = abs_total;
            prev->idle = abs_idle;

            read_count++;
        }
    }

    return (read_count == cpu_count) ? WLM_GRAPH_READ_OK : WLM_GRAPH_READ_ERROR;
}

/* == Main program ========================================================= */

/** Main program. */
int main(const int argc, const char **argv)
{
    cpugraph_state_t state = { 0 };

    state.proc_fp = fopen("/proc/stat", "r");
    if (NULL == state.proc_fp) {
        fprintf(stderr, "%s: Failed to open /proc/stat\n", _app_name);
        return EXIT_FAILURE;
    }

    // Prime prev values so first real sample computes proper delta.
    {
        wlm_graph_values_t values = { 0 };
        _stats_read_fn(&state, &values);
        free(values.data);
    }

    const wlm_graph_app_config_t config = {
        .app_name = _app_name,
        .app_help = _app_help,
        .accumulate_mode = WLM_GRAPH_ACCUMULATE_MODE_INDEPENDENT,
        .stats_read_fn = _stats_read_fn,
        .app_state = &state,
        .state_free_fn = _state_free,
    };

    return wlm_graph_app_run(argc, argv, &config);
}

/* == End of wlmcpugraph.c ================================================= */
