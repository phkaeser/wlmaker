/* ========================================================================= */
/**
 * @file wlmmemgraph.c
 *
 * Memory usage graph dock-app for wlmaker.
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

#include <libbase/libbase.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Application name. */
static const char _app_name[] = "wlmmemgraph";
/** Application help. */
static const char _app_help[] =
    "Displays memory usage as a scrolling graph.\n"
    "\n"
    "Shows stacked memory categories:\n"
    "  - Cached: top of graph (dark blue)\n"
    "  - Buffers: middle of graph (dark cyan)\n"
    "  - Used: bottom of graph (green)\n"
    "\n"
    "The label displays total memory usage.";

/** Line buffer size for /proc/meminfo parsing. */
#define LINE_BUFFER_SIZE 256

/** Number of fields to parse from /proc/meminfo. */
#define MEMINFO_FIELD_COUNT 5

/* == Definitions ========================================================== */

/** Number of memory categories tracked. */
#define MEM_CATEGORY_COUNT 3

/** Memory category indices. */
enum {
    /** Cached (including SReclaimable). */
    MEM_CATEGORY_CACHED = 0,
    /** Buffers. */
    MEM_CATEGORY_BUFFERS = 1,
    /** Used (non-reclaimable). */
    MEM_CATEGORY_USED = 2,
};

/* ------------------------------------------------------------------------- */
/**
 * Generates blue-to-green gradient LUT (256 entries).
 *
 * With 3 memory categories, only indices 0, 127, and 255 are sampled
 * (mapping count 1, 2, 3 respectively). The full 256 entries are required
 * by the API but only 3 discrete colors matter for this use case.
 *
 * @param lut           Output LUT array (256 entries).
 */
static void _memgraph_lut_init(uint32_t lut[256])
{
    for (uint32_t i = 0; i < 256; i++) {
        // Blue to green gradient, with lower values (cached/buffers) darker.
        // Cached and buffers are darker as this memory is technically free.
        // Brightness scales from 1/3 at i=0 to 2/3 at i=255.
        const uint32_t brightness = 85 + (i * 85) / 255;  // 85-170 (1/3-2/3)
        const uint8_t g = (uint8_t)((i * brightness) / 255);
        const uint8_t b = (uint8_t)(((255 - i) * brightness) / 255);
        lut[i] = 0xff000000 | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

/** Maximum length of the label string. */
#define LABEL_BUFFER_SIZE 16

/** Kilobytes per megabyte. */
#define KB_PER_MB 1024UL

/** Kilobytes per gigabyte. */
#define KB_PER_GB (1024UL * 1024)

/** Kilobytes per terabyte. */
#define KB_PER_TB (1024UL * 1024 * 1024)

/** State for the memory graph (mutable runtime data). */
typedef struct {
    /** Open file handle to /proc/meminfo. */
    FILE *proc_fp;
    /** Total memory in kB (from /proc/meminfo). */
    unsigned long mem_total_kb;
    /** Used memory in kB (non-reclaimable). */
    unsigned long mem_used_kb;
    /** Formatted label string. */
    char label[LABEL_BUFFER_SIZE];
} memgraph_state_t;

/* == Cleanup ============================================================== */

/* ------------------------------------------------------------------------- */
/** Frees all allocated state. */
static void _state_free(void *app_state)
{
    memgraph_state_t *state = app_state;

    if (NULL != state->proc_fp) {
        fclose(state->proc_fp);
        state->proc_fp = NULL;
    }
}

/* == Label ================================================================ */

/* ------------------------------------------------------------------------- */
/**
 * Formats memory size with appropriate suffix (GB, MB, kB).
 *
 * @param buf           Output buffer.
 * @param buf_size      Size of output buffer.
 * @param kb            Memory size in kilobytes.
 */
static void _format_memory_size(char *buf, const size_t buf_size, const unsigned long kb)
{
    BS_ASSERT(0 < buf_size);

    if (kb >= KB_PER_TB) {
        // TB range: X.XXXX TB (4 decimal places).
        const unsigned long scale = 10000;
        const unsigned long val = (kb * scale) / KB_PER_TB;
        snprintf(buf, buf_size, "%lu.%04lu TB", val / scale, val % scale);
    } else if (kb >= KB_PER_GB) {
        // GB range: X.XX GB (2 decimal places).
        const unsigned long scale = 100;
        const unsigned long val = (kb * scale) / KB_PER_GB;
        snprintf(buf, buf_size, "%lu.%02lu GB", val / scale, val % scale);
    } else if (kb >= KB_PER_MB) {
        // MB range: X.X MB (1 decimal place).
        const unsigned long scale = 10;
        const unsigned long val = (kb * scale) / KB_PER_MB;
        snprintf(buf, buf_size, "%lu.%lu MB", val / scale, val % scale);
    } else {
        // kB range.
        snprintf(buf, buf_size, "%lu kB", kb);
    }

    buf[buf_size - 1] = '\0';
}

/* ------------------------------------------------------------------------- */
/** Returns the formatted memory usage label. */
static const char *_label_fn(void *app_state)
{
    memgraph_state_t *state = app_state;
    return state->label;
}

/* == Memory statistics ==================================================== */

/**
 * Matches line label against a string literal.
 *
 * Compares `label_len` bytes of `line` against the literal. Used for parsing
 * /proc/meminfo where each line has format "Label: value kB".
 *
 * @param literal       String literal to match against.
 */
#define LABEL_MATCH(literal) \
        (sizeof(literal) - 1 == label_len && 0 == memcmp(line, literal, sizeof(literal) - 1))

/* ------------------------------------------------------------------------- */
/**
 * Reads memory statistics from /proc/meminfo.
 *
 * Parses MemTotal, MemFree, Buffers, Cached, and SReclaimable to compute
 * per-category usage percentages.
 *
 * @param app_state     App state (memgraph_state_t pointer).
 * @param values        Buffer to fill (may reallocate data/num).
 *
 * @return WLM_GRAPH_READ_OK on success, WLM_GRAPH_READ_ERROR on error.
 */
static wlm_graph_read_result_t _stats_read_fn(void *app_state, wlm_graph_values_t *values)
{
    memgraph_state_t * const state = app_state;
    FILE * const fp = state->proc_fp;

    if (NULL == fp) {
        return WLM_GRAPH_READ_ERROR;
    }

    // Reallocate buffer if size doesn't match.
    if (MEM_CATEGORY_COUNT != values->num) {
        uint8_t *new_buf = realloc(values->data, MEM_CATEGORY_COUNT);
        if (NULL == new_buf) {
            return WLM_GRAPH_READ_ERROR;
        }
        values->data = new_buf;
        values->num = MEM_CATEGORY_COUNT;
    }

    rewind(fp);

    unsigned long mem_total = 0;
    unsigned long mem_free = 0;
    unsigned long buffers = 0;
    unsigned long cached = 0;
    unsigned long sreclaimable = 0;

    char line[LINE_BUFFER_SIZE];
    size_t label_len = 0;
    int fields_found = 0;
    while (NULL != fgets(line, sizeof(line), fp) &&
           fields_found < MEMINFO_FIELD_COUNT) {
        // Find the colon to extract the value efficiently.
        char * const colon = strchr(line, ':');
        if (NULL == colon) {
            continue;
        }

        unsigned long value;
        if (1 != sscanf(colon + 1, "%lu", &value)) {
            continue;
        }

        // Match label by prefix length (colon position).
        label_len = colon - line;
        if (LABEL_MATCH("MemTotal")) {
            mem_total = value;
            fields_found++;
        } else if (LABEL_MATCH("MemFree")) {
            mem_free = value;
            fields_found++;
        } else if (LABEL_MATCH("Buffers")) {
            buffers = value;
            fields_found++;
        } else if (LABEL_MATCH("Cached")) {
            cached = value;
            fields_found++;
        } else if (LABEL_MATCH("SReclaimable")) {
            sreclaimable = value;
            fields_found++;
        }
    }

    if (0 == mem_total) {
        return WLM_GRAPH_READ_ERROR;
    }

    // Calculate usage percentages (0-255).
    // Used = MemTotal - MemFree - Buffers - Cached - SReclaimable
    const unsigned long reclaimable = buffers + cached + sreclaimable;
    unsigned long used = 0;
    if (mem_total > mem_free + reclaimable) {
        used = mem_total - mem_free - reclaimable;
    }

    // Scale each category to 0-255 based on total memory.
    values->data[MEM_CATEGORY_USED] = (uint8_t)((used * 255) / mem_total);
    values->data[MEM_CATEGORY_BUFFERS] = (uint8_t)((buffers * 255) / mem_total);
    values->data[MEM_CATEGORY_CACHED] = (uint8_t)(((cached + sreclaimable) * 255) / mem_total);

    // Store values and format label for display.
    state->mem_total_kb = mem_total;
    state->mem_used_kb = used;
    _format_memory_size(state->label, sizeof(state->label), used);

    return WLM_GRAPH_READ_OK;
}
#undef LABEL_MATCH

/* == Main program ========================================================= */

/** Main program. */
int main(const int argc, const char **argv)
{
    memgraph_state_t state = {};

    state.proc_fp = fopen("/proc/meminfo", "r");
    if (NULL == state.proc_fp) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed to open /proc/meminfo");
        return EXIT_FAILURE;
    }

    // Initialize custom LUT (blue-to-green gradient).
    uint32_t pixel_lut[256];
    _memgraph_lut_init(pixel_lut);

    const wlm_graph_app_config_t config = {
        .app_name = _app_name,
        .app_help = _app_help,
        .accumulate_mode = WLM_GRAPH_ACCUMULATE_MODE_STACKED,
        .stats_read_fn = _stats_read_fn,
        .app_state = &state,
        .state_free_fn = _state_free,
        .pixel_lut = pixel_lut,
        .label_fn = _label_fn,
    };

    return wlm_graph_app_run(argc, argv, &config);
}

/* == End of wlmmemgraph.c ================================================= */
