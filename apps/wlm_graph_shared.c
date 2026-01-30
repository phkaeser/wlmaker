/* ========================================================================= */
/**
 * @file wlm_graph_shared.c
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

#include "wlm_graph_shared.h"

#include <cairo.h>
#include <errno.h>
#include <libbase/libbase.h>
#include <libwlclient/libwlclient.h>
#include <primitives/primitives.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libwlclient/icon.h"
#include "wlm_graph_utildefines.h"

/* == Internal definitions ================================================= */

/** Base icon size for scaling calculations. */
#define WLM_GRAPH_BASE_ICON_SIZE 64

/** Base font size for label (at 64px icon size). */
#define WLM_GRAPH_LABEL_FONT_SIZE_BASE 8

/** Label text color (light grey). */
#define WLM_GRAPH_LABEL_COLOR 0xffc4c4c4

/** Label font face. */
#define WLM_GRAPH_LABEL_FONT_FACE "Monospace"

/** Single sample in the circular buffer. */
typedef struct wlm_graph_sample_t {
    /** Previous sample (older). Traversing via prev walks back in time. */
    struct wlm_graph_sample_t *prev;
    /** Next sample (newer). */
    struct wlm_graph_sample_t *next;
    /** Per-category usage values (0-255 each). */
    wlm_graph_values_t values;
    /** Peak usage value: 0 = no activity, 255 = max activity. */
    uint8_t value_peak;
} wlm_graph_sample_t;

/** Common graph state (managed internally by wlm_graph_app_run). */
typedef struct {
    // -- Sample management --
    /** Allocated sample buffer (graph_size[0] entries). */
    wlm_graph_sample_t *samples_alloc;
    /** Current sample (newest in circular buffer). */
    wlm_graph_sample_t *sample_current;

    // -- Dimensions --
    /** Current icon size [width, height] (for detecting size changes). */
    uint32_t icon_size[2];
    /** Graph size [width, height] in pixels (inner area). */
    uint32_t graph_size[2];
    /** Scaled margin for current icon dimensions. */
    uint32_t margin_px;

    // -- Rendering --
    /** Lookup table: pixel color for each value (0-255). */
    uint32_t pixel_lut[256];
    /** Pixel color for the line (top of usage). */
    uint32_t pixel_line;
    /**
     * Scratch buffer for per-row counts during rendering.
     *
     * Each value's usage fills a vertical bar from bottom to top. row_counts[y]
     * accumulates how many values have bars extending to row y. This count is
     * then mapped to pixel intensity.
     */
    uint32_t *row_counts;
    /** Pre-rendered graph buffer (graph_size[0] * graph_size[1] pixels). */
    uint32_t *graph_pixels;
    /** Minimum y (highest peak) across rendered samples for partial scroll. */
    uint32_t y_min;
    /** Previous y_min value (used for scroll bounds). */
    uint32_t y_min_prev;
    /** Sample with highest peak (defines y_min). */
    wlm_graph_sample_t *sample_peak;
} wlm_graph_state_t;

/** Minimum brightness for solid area (for WLM_GRAPH_COLOR_MODE_ALPHA). */
#define WLM_GRAPH_SOLID_BRIGHTNESS_MIN 32

/** Maximum brightness for solid area (for WLM_GRAPH_COLOR_MODE_ALPHA). */
#define WLM_GRAPH_SOLID_BRIGHTNESS_MAX 128

/** Black pixel (fully opaque). */
#define WLM_GRAPH_PIXEL_BLACK 0xff000000

/** Default line pixel color (green). */
#define WLM_GRAPH_PIXEL_LINE_DEFAULT 0xff008000

/** Constructs a grayscale ARGB pixel from brightness value. */
#define WLM_GRAPH_PIXEL_GRAY(b) (WLM_GRAPH_PIXEL_BLACK | ((uint32_t)(b) << 16) | \
                                 ((uint32_t)(b) << 8) | (uint32_t)(b))

/** Color modes for the graph. */
typedef enum {
    /** Heat map from blue (cold) to red (hot). */
    WLM_GRAPH_COLOR_MODE_HEAT,
    /** Gray-scale with alpha-like intensity. */
    WLM_GRAPH_COLOR_MODE_ALPHA,
} wlm_graph_color_mode_t;

/** Maximum length of font face name. */
#define WLM_GRAPH_FONT_FACE_MAX 64

/** Font specification (XFT-style). */
typedef struct {
    /** Font face name. */
    char face[WLM_GRAPH_FONT_FACE_MAX];
    /** Font size (at base icon size). */
    uint32_t size;
    /** Font weight (CAIRO_FONT_WEIGHT_NORMAL or CAIRO_FONT_WEIGHT_BOLD). */
    cairo_font_weight_t weight;
    /** Font slant (CAIRO_FONT_SLANT_NORMAL, _ITALIC, or _OBLIQUE). */
    cairo_font_slant_t slant;
} wlm_graph_font_spec_t;

/** Common user preferences (from command line arguments). */
typedef struct {
    /** Update interval in microseconds. */
    uint64_t interval_usec;
    /** Bezel margin in logical pixels. */
    uint32_t margin_logical_px;
    /** Color mode for the graph. */
    wlm_graph_color_mode_t color_mode;
    /** Label font specification. */
    wlm_graph_font_spec_t font;
    /** Whether to show label. True implies label_fn is set. */
    bool show_label;
} wlm_graph_prefs_t;

/**
 * Common handle for graph app callbacks.
 *
 * Contains all data needed by the shared icon render and timer callbacks.
 */
typedef struct {
    /** Pointer to the common graph state. */
    wlm_graph_state_t *graph_state;
    /** User preferences (immutable). */
    const wlm_graph_prefs_t *prefs;
    /** Icon handle. */
    wlclient_icon_t *icon_ptr;
    /** Application configuration. */
    const wlm_graph_app_config_t *config;
} wlm_graph_handle_t;

/* == Forward declarations ================================================= */

#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void _wlm_graph_panic(const char *msg);
static void _wlm_graph_sample_values_free(wlm_graph_sample_t *samples, const uint32_t count);
static void _wlm_graph_samples_init(wlm_graph_sample_t *samples, const uint32_t count);
static void _wlm_graph_sample_compute_peak(
    wlm_graph_sample_t *sample,
    const wlm_graph_mode_t accumulate_mode);
static uint32_t _wlm_graph_usage_to_y(const uint8_t usage, const uint32_t height);
static uint32_t _wlm_graph_usage_to_y_with_zero_check(const uint8_t usage, const uint32_t height);
static uint32_t _wlm_graph_column_render(
    wlm_graph_state_t *graph_state,
    const wlm_graph_sample_t *sample,
    const uint32_t column,
    const wlm_graph_mode_t accumulate_mode);
static void _wlm_graph_peak_connector_draw(
    uint32_t *graph_pixels,
    const uint32_t graph_width,
    const uint32_t pixel_line,
    const uint32_t x,
    const uint32_t y_range[2]);
static void _wlm_graph_peak_connector_draw_between(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t pixel_line,
    const uint8_t usage_curr,
    const uint8_t usage_prev,
    const uint32_t column_curr,
    const uint32_t column_prev);
static void _wlm_graph_fill_columns_black(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t column_end);
static void _wlm_graph_scroll_left(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t y_start);
static void _wlm_graph_rebuild_from_samples(
    wlm_graph_state_t *graph_state,
    const wlm_graph_mode_t accumulate_mode);
static void _wlm_graph_update_with_sample(
    wlm_graph_state_t *graph_state,
    wlm_graph_sample_t *new_sample,
    const wlm_graph_mode_t accumulate_mode);
static uint64_t _wlm_graph_time_next_update(const uint64_t interval_usec);
static bool _wlm_graph_icon_render_callback(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr);
static void _wlm_graph_sample_update(wlm_graph_handle_t *handle);
static void _wlm_graph_timer_callback(wlclient_t *client_ptr, void *ud_ptr);

/* == Utility functions ==================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Parses a string as a 32-bit integer within a specified range.
 *
 * @param opt_name          Option name for error messages.
 * @param str               String to parse.
 * @param val_min           Minimum allowed value.
 * @param val_max           Maximum allowed value.
 * @param result_ptr        Output: parsed value.
 *
 * @return true on success.
 */
static bool _wlm_graph_arg_parse_i32(
    const char *opt_name,
    const char *str,
    const int32_t val_min,
    const int32_t val_max,
    int32_t *result_ptr)
{
    char *endptr;
    errno = 0;
    const long val = strtol(str, &endptr, 10);
    if ('\0' != *endptr || 0 != errno || val < val_min || val > val_max) {
        fprintf(stderr, "Error: %s value '%s' must be %d-%d\n",
                opt_name, str, val_min, val_max);
        return false;
    }
    *result_ptr = (int32_t)val;
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Parses a string as a double within a specified range.
 *
 * @param opt_name          Option name for error messages.
 * @param str               String to parse.
 * @param val_min           Minimum allowed value.
 * @param val_max           Maximum allowed value.
 * @param result_ptr        Output: parsed value.
 *
 * @return true on success.
 */
static bool _wlm_graph_arg_parse_f64(
    const char *opt_name,
    const char *str,
    const double val_min,
    const double val_max,
    double *result_ptr)
{
    char *endptr;
    errno = 0;
    const double val = strtod(str, &endptr);
    if ('\0' != *endptr || 0 != errno || val < val_min || val > val_max) {
        fprintf(stderr, "Error: %s value '%s' must be %g-%g\n",
                opt_name, str, val_min, val_max);
        return false;
    }
    *result_ptr = val;
    return true;
}

/* ------------------------------------------------------------------------- */
/** Wrapper for _str_match_prefix_n using sizeof for string literal prefix. */
#define _str_match_prefix(str_ptr, len_ptr, prefix) \
    _str_match_prefix_n(str_ptr, len_ptr, prefix, sizeof(prefix) - 1)

/**
 * Checks if string starts with prefix; on match advances string and adjusts length.
 *
 * @param str_ptr       Pointer to string pointer (advanced on match).
 * @param len_ptr       Pointer to remaining length (decremented on match).
 * @param prefix        Prefix to match.
 * @param prefix_len    Length of prefix.
 *
 * @return true if prefix matched and consumed, false otherwise.
 */
static bool _str_match_prefix_n(
    const char **str_ptr,
    size_t *len_ptr,
    const char *prefix,
    const size_t prefix_len)
{
    if (*len_ptr >= prefix_len && 0 == strncmp(*str_ptr, prefix, prefix_len)) {
        *str_ptr += prefix_len;
        *len_ptr -= prefix_len;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Parses XFT-style font specification: "Name:size=N:weight=W:slant=S".
 *
 * @param opt_name      Option name for error messages (e.g., "--font").
 * @param str           Font specification string.
 * @param font          Font spec struct to populate.
 *
 * @return true on success, false on error (message printed to stderr).
 */
static bool _wlm_graph_arg_parse_font(
    const char *opt_name,
    const char *str,
    wlm_graph_font_spec_t *font)
{
    const char *colon = strchr(str, ':');

    // Extract font name (everything before first colon, or entire string).
    const size_t name_len = (NULL != colon) ? (size_t)(colon - str) : strlen(str);
    if (0 == name_len || name_len >= WLM_GRAPH_FONT_FACE_MAX) {
        fprintf(stderr, "Error: %s font name too long or empty\n", opt_name);
        return false;
    }
    memcpy(font->face, str, name_len);
    font->face[name_len] = '\0';

    // Parse optional key=value pairs after font name.
    while (NULL != colon) {
        const char *pos = colon + 1;
        colon = strchr(pos, ':');
        size_t len = (NULL != colon) ? (size_t)(colon - pos) : strlen(pos);

        if (_str_match_prefix(&pos, &len, "size=")) {
            char size_buf[16];
            if (len >= sizeof(size_buf)) {
                fprintf(stderr, "Error: %s size value too long\n", opt_name);
                return false;
            }
            memcpy(size_buf, pos, len);
            size_buf[len] = '\0';

            int32_t size;
            if (!_wlm_graph_arg_parse_i32(opt_name, size_buf, 4, 72, &size)) {
                return false;
            }
            font->size = (uint32_t)size;
        } else if (_str_match_prefix(&pos, &len, "weight=")) {
            if (_str_match_prefix(&pos, &len, "normal") && 0 == len) {
                font->weight = CAIRO_FONT_WEIGHT_NORMAL;
            } else if (_str_match_prefix(&pos, &len, "bold") && 0 == len) {
                font->weight = CAIRO_FONT_WEIGHT_BOLD;
            } else {
                fprintf(stderr, "Error: %s weight must be 'normal' or 'bold'\n", opt_name);
                return false;
            }
        } else if (_str_match_prefix(&pos, &len, "slant=")) {
            if (_str_match_prefix(&pos, &len, "normal") && 0 == len) {
                font->slant = CAIRO_FONT_SLANT_NORMAL;
            } else if (_str_match_prefix(&pos, &len, "italic") && 0 == len) {
                font->slant = CAIRO_FONT_SLANT_ITALIC;
            } else if (_str_match_prefix(&pos, &len, "oblique") && 0 == len) {
                font->slant = CAIRO_FONT_SLANT_OBLIQUE;
            } else {
                fprintf(stderr, "Error: %s slant must be 'normal', 'italic', or 'oblique'\n", opt_name);
                return false;
            }
        } else if (len > 0) {
            fprintf(stderr, "Error: Unknown %s option starting with '%.20s'\n", opt_name, pos);
            return false;
        }
    }

    return true;
}

/* == Initialization ======================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Initializes the pixel lookup table for graph coloring.
 *
 * @param pixel_lut         Output: 256-entry lookup table for value-to-color mapping.
 * @param pixel_line_ptr    Output: color for the line/peak indicator.
 * @param color_mode        Color mode (heat map or alpha/grayscale).
 */
static void _wlm_graph_pixel_lut_init(
    uint32_t pixel_lut[256],
    uint32_t *pixel_line_ptr,
    const wlm_graph_color_mode_t color_mode)
{
    *pixel_line_ptr = WLM_GRAPH_PIXEL_LINE_DEFAULT;

    if (WLM_GRAPH_COLOR_MODE_ALPHA == color_mode) {
        // Grayscale: values 0-255 scaled from SOLID_BRIGHTNESS_MIN to MAX.
        const uint32_t range = WLM_GRAPH_SOLID_BRIGHTNESS_MAX - WLM_GRAPH_SOLID_BRIGHTNESS_MIN;
        for (uint32_t i = 0; i < 256; i++) {
            const uint8_t b = (uint8_t)(WLM_GRAPH_SOLID_BRIGHTNESS_MIN + ((i * range) / 255));
            pixel_lut[i] = WLM_GRAPH_PIXEL_GRAY(b);
        }
    } else {
        // Heat map: blue (cold) -> green -> yellow -> red (hot).
        // Divide the 0-255 range into 3 color bands.
        const uint32_t band1_start = 256 / 3;        // 85
        const uint32_t band2_start = (2 * 256) / 3;   // 170

        for (uint32_t i = 0; i < 256; i++) {
            uint8_t r, g, b;

            if (i < band1_start) {
                // Blue to Green: B decreases, G increases.
                r = 0;
                g = (uint8_t)((i * 255) / (band1_start - 1));
                b = (uint8_t)(255 - ((i * 255) / (band1_start - 1)));
            } else if (i < band2_start) {
                // Green to Yellow: R increases, G stays max.
                const uint32_t band_pos = i - band1_start;
                r = (uint8_t)((band_pos * 255) / (band2_start - band1_start - 1));
                g = 255;
                b = 0;
            } else {
                // Yellow to Red: G decreases, R stays max.
                const uint32_t band_pos = i - band2_start;
                r = 255;
                g = (uint8_t)(255 - ((band_pos * 255) / (255 - band2_start)));
                b = 0;
            }

            pixel_lut[i] = WLM_GRAPH_PIXEL_BLACK |
                ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}

/* == Buffer management ==================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Resizes graph buffers when icon dimensions change.
 *
 * @param graph_state       Graph state to resize.
 * @param size              New icon dimensions [width, height].
 * @param margin_logical_px Margin in logical pixels (at base icon size).
 * @param panic_fn          Function to call on allocation failure.
 *
 * @return true if buffers were resized, false if dimensions are too small.
 */
static bool _wlm_graph_buffers_resize(
    wlm_graph_state_t *graph_state,
    const uint32_t size[2],
    const uint32_t margin_logical_px,
    void (*panic_fn)(const char *msg))
{
    // Calculate inner dimensions (graph area inside bezel).
    const uint32_t margin_px = (margin_logical_px * size[0]) / WLM_GRAPH_BASE_ICON_SIZE;
    const uint32_t inner_size[2] = {
        (size[0] > (2 * margin_px)) ? size[0] - (2 * margin_px) : 0,
        (size[1] > (2 * margin_px)) ? size[1] - (2 * margin_px) : 0,
    };

    if (0 == inner_size[0] || 0 == inner_size[1]) {
        return false;
    }

    // Check if dimensions changed.
    const bool size_changed[2] = {
        inner_size[0] != graph_state->graph_size[0],
        inner_size[1] != graph_state->graph_size[1],
    };

    // Reallocate samples if width changed.
    if (size_changed[0]) {
        _wlm_graph_sample_values_free(graph_state->samples_alloc, graph_state->graph_size[0]);
        free(graph_state->samples_alloc);
        graph_state->samples_alloc = NULL;
        graph_state->sample_current = NULL;

        // Allocate sample buffer (values allocated lazily during capture).
        graph_state->samples_alloc = calloc(inner_size[0], sizeof(wlm_graph_sample_t));
        if (NULL == graph_state->samples_alloc) {
            panic_fn("Failed to allocate sample buffer");
        }

        // Initialize circular doubly-linked list.
        _wlm_graph_samples_init(graph_state->samples_alloc, inner_size[0]);
        graph_state->sample_current = &graph_state->samples_alloc[0];
    }

    // Reallocate graph buffers if dimensions changed.
    if (size_changed[0] || size_changed[1]) {
        free(graph_state->graph_pixels);
        free(graph_state->row_counts);
        graph_state->graph_pixels = NULL;
        graph_state->row_counts = NULL;

        // Allocate graph data buffer.
        graph_state->graph_pixels = malloc(sizeof(uint32_t) * inner_size[0] * inner_size[1]);
        if (NULL == graph_state->graph_pixels) {
            panic_fn("Failed to allocate graph data buffer");
        }

        // Allocate row counts scratch buffer.
        graph_state->row_counts = malloc(sizeof(uint32_t) * inner_size[1]);
        if (NULL == graph_state->row_counts) {
            panic_fn("Failed to allocate row counts buffer");
        }

        graph_state->graph_size[0] = inner_size[0];
        graph_state->graph_size[1] = inner_size[1];
    }

    graph_state->icon_size[0] = size[0];
    graph_state->icon_size[1] = size[1];
    graph_state->margin_px = margin_px;

    return true;
}

/* == Rendering ============================================================ */

/* ------------------------------------------------------------------------- */
/**
 * Finds sample with highest peak, sets y_min and sample_peak in graph_state.
 *
 * Iterates newest-to-oldest and uses strict `<` so the newest sample wins ties.
 * This keeps sample_peak in the buffer longer, reducing rescan frequency.
 */
static void _wlm_graph_y_min_from_samples(
    wlm_graph_state_t *graph_state,
    wlm_graph_sample_t *current_sample)
{
    const uint32_t height = graph_state->graph_size[1];
    uint32_t y_min = height;
    wlm_graph_sample_t *sample_peak = NULL;
    wlm_graph_sample_t *sample = current_sample;
    do {
        if (0 < sample->value_peak) {
            const uint32_t y = _wlm_graph_usage_to_y(sample->value_peak, height);
            // Strict `<` so first (newest) sample at this y wins.
            if (y < y_min) {
                y_min = y;
                sample_peak = sample;
            }
        }
        sample = sample->prev;
    } while (sample != current_sample);
    graph_state->y_min = y_min;
    graph_state->sample_peak = sample_peak;
}

/* ------------------------------------------------------------------------- */
/**
 * Rebuilds the entire graph from stored samples.
 *
 * Used after resize to re-render all columns from sample history.
 *
 * @param graph_state       Graph state containing samples and pixel buffer.
 * @param accumulate_mode  Sample accumulation mode.
 */
static void _wlm_graph_rebuild_from_samples(
    wlm_graph_state_t *graph_state,
    const wlm_graph_mode_t accumulate_mode)
{
    // All callers verify graph_size is non-zero.
    BS_ASSERT(0 < graph_state->graph_size[0]);

    const uint32_t *graph_size = graph_state->graph_size;

    if (NULL == graph_state->sample_current || 0 == graph_state->sample_current->values.num) {
        // No samples captured yet, fill with black.
        graph_state->y_min = graph_size[1];
        graph_state->y_min_prev = graph_size[1];
        graph_state->sample_peak = NULL;
        for (uint32_t i = 0; i < graph_size[0] * graph_size[1]; i++) {
            graph_state->graph_pixels[i] = WLM_GRAPH_PIXEL_BLACK;
        }
        return;
    }

    // Walk backward from sample_current to render samples right-to-left.
    // sample_current is the newest sample, goes in rightmost column.
    // Walk until we wrap around to sample_current.
    wlm_graph_sample_t *sample = graph_state->sample_current;
    uint32_t column = graph_size[0];

    do {
        column--;
        _wlm_graph_column_render(graph_state, sample, column, accumulate_mode);

        // Draw vertical line to previous sample if needed.
        const uint8_t usage_curr = sample->value_peak;

        // Advance sample.
        sample = sample->prev;

        if (sample != graph_state->sample_current && 0 < column) {
            _wlm_graph_peak_connector_draw_between(
                graph_state->graph_pixels, graph_state->graph_size, graph_state->pixel_line,
                usage_curr, sample->value_peak, column, column - 1);
        }
    } while (0 < column && sample != graph_state->sample_current);

    // Fill remaining columns with black.
    if (0 < column) {
        _wlm_graph_fill_columns_black(graph_state->graph_pixels, graph_size, column);
    }

    // Compute y_min/sample_peak via rescan rather than tracking inline above.
    // This keeps peak tracking logic in one place (_wlm_graph_y_min_from_samples).
    _wlm_graph_y_min_from_samples(graph_state, graph_state->sample_current);
    graph_state->y_min_prev = graph_state->y_min;
}

/* ------------------------------------------------------------------------- */
/**
 * Renders a single column of the graph from sample data.
 *
 * @param graph_state       Graph state containing pixel buffer and LUT.
 * @param sample            Sample data for this column.
 * @param column            Column index to render.
 * @param accumulate_mode  Sample accumulation mode.
 *
 * @return Y coordinate of the peak line for this column.
 */
static uint32_t _wlm_graph_column_render(
    wlm_graph_state_t *graph_state,
    const wlm_graph_sample_t *sample,
    const uint32_t column,
    const wlm_graph_mode_t accumulate_mode)
{
    // All callers verify graph_size is non-zero.
    BS_ASSERT(0 < graph_state->graph_size[1]);

    const uint32_t graph_size[2] = {graph_state->graph_size[0], graph_state->graph_size[1]};
    uint32_t * const pixels = graph_state->graph_pixels;
    uint32_t * const row_counts = graph_state->row_counts;

    // Clear row counts scratch buffer.
    memset(row_counts, 0, sizeof(*row_counts) * graph_size[1]);

    const uint32_t values_num = sample->values.num;
    const uint8_t * const values = sample->values.data;

    // Accumulate row counts based on mode.
    if (WLM_GRAPH_ACCUMULATE_MODE_STACKED == accumulate_mode) {
        // Stacked: values stack on top of each other cumulatively.
        uint32_t cumulative = 0;
        for (uint32_t i = 0; i < values_num; i++) {
            const uint8_t usage = values[i];
            if (0 == usage) continue;

            cumulative += usage;
            const uint8_t level = (uint8_t)MIN2(cumulative, 255);

            // Calculate topmost line from cumulative usage.
            const uint32_t y_top = _wlm_graph_usage_to_y(level, graph_size[1]);

            // Skip if usage maps outside visible area.
            if (y_top >= graph_size[1]) continue;

            // Mark all lines from top to bottom as solid coverage.
            uint32_t *rc = &row_counts[y_top];
            for (uint32_t y = y_top; y < graph_size[1]; y++) {
                (*rc++)++;
            }
        }
    } else {
        // Independent: each value fills from its level to bottom independently.
        for (uint32_t i = 0; i < values_num; i++) {
            const uint8_t usage = values[i];
            if (0 == usage) continue;

            // Calculate topmost line from usage: higher usage = lower y.
            const uint32_t y_top = _wlm_graph_usage_to_y(usage, graph_size[1]);

            // Skip if usage maps outside visible area.
            if (y_top >= graph_size[1]) continue;

            // Mark all lines from top to bottom as solid coverage.
            uint32_t *rc = &row_counts[y_top];
            for (uint32_t y = y_top; y < graph_size[1]; y++) {
                (*rc++)++;
            }
        }
    }

    // Convert peak usage to y coordinate for drawing.
    const uint32_t y_line = _wlm_graph_usage_to_y_with_zero_check(sample->value_peak, graph_size[1]);

    // Cache LUT pointer for inner loop.
    const uint32_t * const pixel_lut = graph_state->pixel_lut;

    // Clear stale pixels above peak, scanning up until we hit black.
    // Works because non-black pixels are contiguous from y_min downward.
    uint32_t *pixel = &pixels[(y_line * graph_size[0]) + column];
    for (uint32_t y = y_line; y > 0; ) {
        y--;
        pixel -= graph_size[0];
        if (WLM_GRAPH_PIXEL_BLACK == *pixel) break;
        *pixel = WLM_GRAPH_PIXEL_BLACK;
    }

    // Rows from peak onwards: render intensity based on count at each row.
    pixel = &pixels[(y_line * graph_size[0]) + column];
    const uint32_t *row_count = &row_counts[y_line];
    if (1 >= values_num) {
        // Single value: all non-zero counts use max LUT index.
        const uint32_t pixel_max = pixel_lut[255];
        for (uint32_t y = y_line; y < graph_size[1]; y++) {
            *pixel = (0 < *row_count++) ? pixel_max : WLM_GRAPH_PIXEL_BLACK;
            pixel += graph_size[0];
        }
    } else {
        // Multiple values: map count (1..values_num) to LUT index (0..255).
        const uint32_t divisor = values_num - 1;
        for (uint32_t y = y_line; y < graph_size[1]; y++) {
            const uint32_t count = *row_count++;
            *pixel = (0 == count) ?
                WLM_GRAPH_PIXEL_BLACK : pixel_lut[((count - 1) * 255) / divisor];
            pixel += graph_size[0];
        }
    }

    // Draw line pixel at peak position.
    if (y_line < graph_size[1]) {
        pixels[(y_line * graph_size[0]) + column] = graph_state->pixel_line;
    }

    return y_line;
}

/* ------------------------------------------------------------------------- */
/** Maps usage (0-255) to y coordinate: y=0 is top, higher usage = higher on screen. */
static uint32_t _wlm_graph_usage_to_y(const uint8_t usage, const uint32_t height)
{
    return (height - 1) - ((usage * (height - 1)) / 255);
}

/* ------------------------------------------------------------------------- */
/** Maps usage (0-255) to y coordinate. Returns height for usage=0 (no bar). */
static uint32_t _wlm_graph_usage_to_y_with_zero_check(const uint8_t usage, const uint32_t height)
{
    return (0 == usage) ? height : _wlm_graph_usage_to_y(usage, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Draws a vertical connector line between y coordinates.
 *
 * @param graph_pixels      Pixel buffer to draw into.
 * @param graph_width       Width of the graph in pixels.
 * @param pixel_line        Color for the connector line.
 * @param x                 X coordinate (column) to draw at.
 * @param y_range           Y range [start, end) to draw.
 */
static void _wlm_graph_peak_connector_draw(
    uint32_t *graph_pixels,
    const uint32_t graph_width,
    const uint32_t pixel_line,
    const uint32_t x,
    const uint32_t y_range[2])
{
    uint32_t *pixel = &graph_pixels[(y_range[0] * graph_width) + x];
    for (uint32_t y = y_range[0]; y < y_range[1]; y++) {
        *pixel = pixel_line;
        pixel += graph_width;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Draws a vertical connector between adjacent columns when peaks differ.
 *
 * @param graph_pixels      Pixel buffer to draw into.
 * @param graph_size        Graph dimensions [width, height].
 * @param pixel_line        Color for the connector line.
 * @param usage_curr        Usage value for current column.
 * @param usage_prev        Usage value for previous column.
 * @param column_curr       Current column index.
 * @param column_prev       Previous column index.
 */
static void _wlm_graph_peak_connector_draw_between(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t pixel_line,
    const uint8_t usage_curr,
    const uint8_t usage_prev,
    const uint32_t column_curr,
    const uint32_t column_prev)
{
    if (usage_curr == usage_prev) {
        return;
    }

    const uint32_t height = graph_size[1];
    const uint32_t y_curr = _wlm_graph_usage_to_y_with_zero_check(usage_curr, height);
    const uint32_t y_prev = _wlm_graph_usage_to_y_with_zero_check(usage_prev, height);

    uint32_t x;
    uint32_t y_range[2];
    if (usage_curr < usage_prev) {
        // Current usage is lower (higher y), draw on current column.
        // Start below line pixel (+1) to avoid overwriting it, unless zero (no line).
        x = column_curr;
        y_range[0] = (0 < usage_prev) ? y_prev + 1 : y_prev;
        y_range[1] = y_curr;
    } else {
        // Previous usage is lower (higher y), draw on previous column.
        // Start below line pixel (+1) to avoid overwriting it, unless zero (no line).
        x = column_prev;
        y_range[0] = (0 < usage_curr) ? y_curr + 1 : y_curr;
        y_range[1] = y_prev;
    }
    _wlm_graph_peak_connector_draw(graph_pixels, graph_size[0], pixel_line, x, y_range);
}

/* ------------------------------------------------------------------------- */
/**
 * Copies graph pixels to the destination graphics buffer.
 *
 * @param gfxbuf_ptr        Destination graphics buffer.
 * @param graph_pixels      Source pixel data.
 * @param graph_size        Graph dimensions [width, height].
 * @param offset            Offset in destination buffer [x, y].
 */
static void _wlm_graph_blit_to_buffer(
    bs_gfxbuf_t *gfxbuf_ptr,
    const uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t offset[2])
{
    const uint32_t stride_dst = gfxbuf_ptr->pixels_per_line;
    const uint32_t width = graph_size[0];
    const size_t copy_bytes = sizeof(*graph_pixels) * width;
    uint32_t *row_dst = &gfxbuf_ptr->data_ptr[(offset[1] * stride_dst) + offset[0]];
    const uint32_t *row_src = graph_pixels;

    // Copy each row from graph_pixels to the destination buffer.
    for (uint32_t y = 0; y < graph_size[1]; y++) {
        memcpy(row_dst, row_src, copy_bytes);
        row_dst += stride_dst;
        row_src += width;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Draws the bezel frame around the graph area.
 *
 * @param gfxbuf_ptr        Graphics buffer to draw into.
 * @param margin_logical_px Margin in logical pixels (at base icon size).
 *
 * @return true on success.
 */
static bool _wlm_graph_bezel_draw(bs_gfxbuf_t *gfxbuf_ptr, const uint32_t margin_logical_px)
{
    BS_ASSERT(0 < margin_logical_px);

    cairo_t * const cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create_from_bs_gfxbuf(%p)", gfxbuf_ptr);
        return false;
    }

    // Scale bezel relative to icon size, like wlmclock.
    const uint32_t size[2] = {gfxbuf_ptr->width, gfxbuf_ptr->height};

    // Offset from edge to bezel position (scales with icon size).
    const uint32_t bezel_offset = ((margin_logical_px - 1) * size[0]) / WLM_GRAPH_BASE_ICON_SIZE;
    const uint32_t bezel_line_width = MAX2(size[0] / WLM_GRAPH_BASE_ICON_SIZE, 1);

    wlm_primitives_draw_bezel_at(
        cairo_ptr,
        bezel_offset, bezel_offset,
        size[0] - (2 * bezel_offset), size[1] - (2 * bezel_offset),
        bezel_line_width, false);

    cairo_destroy(cairo_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Draws the label text in the top-left corner of the graph.
 *
 * @param gfxbuf_ptr        Graphics buffer to draw into.
 * @param margin_px         Margin in pixels (scaled to current icon size).
 * @param label             Label text to draw.
 * @param prefs             Preferences containing font settings.
 */
static void _wlm_graph_label_draw(
    bs_gfxbuf_t *gfxbuf_ptr,
    const uint32_t margin_px,
    const char *label,
    const wlm_graph_prefs_t *prefs)
{
    if ('\0' == *label) {
        return;
    }

    cairo_t * const cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        return;
    }

    const uint32_t size[2] = {gfxbuf_ptr->width, gfxbuf_ptr->height};

    // Compute scale factor relative to base icon size.
    const uint32_t scale = size[0];
    const uint32_t scale_base = WLM_GRAPH_BASE_ICON_SIZE;

    const uint32_t font_size = (prefs->font.size * scale) / scale_base;
    if (0 == font_size) {
        cairo_destroy(cairo_ptr);
        return;
    }
    const uint32_t padding = (2 * scale) / scale_base;

    cairo_select_font_face(
        cairo_ptr,
        prefs->font.face,
        prefs->font.slant,
        prefs->font.weight);
    cairo_set_font_size(cairo_ptr, font_size);

    // Position: top-left corner, inside margin.
    const double x = margin_px + padding;
    const double y = margin_px + padding + font_size;

    // Draw text with black outline for readability.
    cairo_move_to(cairo_ptr, x, y);
    cairo_text_path(cairo_ptr, label);

    // Stroke outline (black).
    cairo_set_source_argb8888(cairo_ptr, WLM_GRAPH_PIXEL_BLACK);
    cairo_set_line_width(cairo_ptr, (2.0 * scale) / scale_base);
    cairo_stroke_preserve(cairo_ptr);

    // Fill text (white).
    cairo_set_source_argb8888(cairo_ptr, WLM_GRAPH_LABEL_COLOR);
    cairo_fill(cairo_ptr);

    cairo_destroy(cairo_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Scrolls graph pixels left by one column.
 *
 * @param graph_pixels      Pixel buffer to scroll.
 * @param graph_size        Graph dimensions [width, height].
 * @param y_start           Starting Y coordinate (skip rows above this).
 */
static void _wlm_graph_scroll_left(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t y_start)
{
    const uint32_t width = graph_size[0];
    const size_t move_bytes = sizeof(*graph_pixels) * (width - 1);
    uint32_t *row = &graph_pixels[y_start * width];
    for (uint32_t y = y_start; y < graph_size[1]; y++) {
        memmove(row, row + 1, move_bytes);
        row += width;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Fills columns with black pixels.
 *
 * @param graph_pixels      Pixel buffer to fill.
 * @param graph_size        Graph dimensions [width, height].
 * @param column_end        Number of columns to fill (0 to column_end-1).
 */
static void _wlm_graph_fill_columns_black(
    uint32_t *graph_pixels,
    const uint32_t graph_size[2],
    const uint32_t column_end)
{
    const uint32_t width = graph_size[0];
    uint32_t *row = graph_pixels;
    for (uint32_t y = 0; y < graph_size[1]; y++) {
        for (uint32_t x = 0; x < column_end; x++) {
            row[x] = WLM_GRAPH_PIXEL_BLACK;
        }
        row += width;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Frees the values arrays within each sample.
 *
 * @param samples           Array of samples to free values from.
 * @param count             Number of samples in the array.
 */
static void _wlm_graph_sample_values_free(wlm_graph_sample_t *samples, const uint32_t count)
{
    if (NULL == samples) return;

    for (uint32_t i = 0; i < count; i++) {
        free(samples[i].values.data);
        samples[i].values.data = NULL;
        samples[i].values.num = 0;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Computes the peak value for a sample from its values array.
 *
 * @param sample            Sample with values already populated.
 * @param accumulate_mode  Sample accumulation mode.
 */
static void _wlm_graph_sample_compute_peak(
    wlm_graph_sample_t *sample,
    const wlm_graph_mode_t accumulate_mode)
{
    const uint32_t values_num = sample->values.num;
    const uint8_t *values = sample->values.data;

    if (WLM_GRAPH_ACCUMULATE_MODE_STACKED == accumulate_mode) {
        // Stacked: peak is sum of all values, clamped to 255.
        uint32_t total = 0;
        for (uint32_t i = 0; i < values_num; i++) {
            total += values[i];
        }
        sample->value_peak = (uint8_t)MIN2(total, 255);
    } else {
        // Independent: peak is maximum of all values.
        uint8_t max_val = 0;
        for (uint32_t i = 0; i < values_num; i++) {
            max_val = MAX2(max_val, values[i]);
        }
        sample->value_peak = max_val;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Initializes sample array as a circular doubly-linked list.
 *
 * @param samples           Array of samples to initialize.
 * @param count             Number of samples in the array.
 */
static void _wlm_graph_samples_init(wlm_graph_sample_t *samples, const uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t i_prev = (i + count - 1) % count;
        const uint32_t i_next = (i + 1) % count;
        samples[i].prev = &samples[i_prev];
        samples[i].next = &samples[i_next];
        samples[i].values.data = NULL;
        samples[i].values.num = 0;
        samples[i].value_peak = 0;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Calculates the timestamp for the next update.
 *
 * @param interval_usec     Update interval in microseconds.
 *
 * @return Timestamp in microseconds for the next update.
 */
static uint64_t _wlm_graph_time_next_update(const uint64_t interval_usec)
{
    return bs_usec() + interval_usec;
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the graph with a new sample, scrolling and rendering.
 *
 * @param graph_state       Graph state to update.
 * @param new_sample        Sample containing the new data.
 * @param accumulate_mode  Sample accumulation mode.
 */
static void _wlm_graph_update_with_sample(
    wlm_graph_state_t *graph_state,
    wlm_graph_sample_t *new_sample,
    const wlm_graph_mode_t accumulate_mode)
{
    const uint32_t *graph_size = graph_state->graph_size;

    // Check if sample being overwritten is the peak sample (before overwriting).
    const bool need_rescan = (new_sample == graph_state->sample_peak);

    // Scroll from previous y_min.
    _wlm_graph_scroll_left(graph_state->graph_pixels, graph_size, graph_state->y_min_prev);
    const uint32_t y_line = _wlm_graph_column_render(graph_state, new_sample, graph_size[0] - 1, accumulate_mode);

    // Update y_min: if peak sample scrolled out, rescan; else check new sample.
    if (need_rescan) {
        _wlm_graph_y_min_from_samples(graph_state, new_sample);
    } else if (y_line <= graph_state->y_min) {
        // Use `<=` so newest sample wins ties, reducing future rescans.
        graph_state->y_min = y_line;
        graph_state->sample_peak = new_sample;
    }
    graph_state->y_min_prev = graph_state->y_min;

    // Advance sample_current to the sample we just wrote.
    graph_state->sample_current = new_sample;

    // Draw vertical line connecting consecutive peaks.
    _wlm_graph_peak_connector_draw_between(
        graph_state->graph_pixels, graph_state->graph_size, graph_state->pixel_line,
        new_sample->value_peak,
        new_sample->prev->value_peak,
        graph_size[0] - 1,
        graph_size[0] - 2);
}

/* ------------------------------------------------------------------------- */
/**
 * Parses command-line arguments into preferences.
 *
 * @param argc              Argument count.
 * @param argv              Argument vector.
 * @param app_name          Application name for error messages.
 * @param app_help          Application help string for --help output.
 * @param has_custom_lut    Whether app supports custom color lookup table.
 * @param has_label         Whether app supports label display.
 * @param prefs             Output: parsed preferences.
 *
 * @return 0 to continue, 1 if --help was shown, -1 on error.
 */
static int _wlm_graph_args_parse(
    const int argc,
    const char **argv,
    const char *app_name,
    const char *app_help,
    const bool has_custom_lut,
    const bool has_label,
    wlm_graph_prefs_t *prefs)
{
    // Set defaults.
    prefs->interval_usec = 1000000;  // 1 second.
    prefs->margin_logical_px = 5;
    prefs->color_mode = WLM_GRAPH_COLOR_MODE_HEAT;
    snprintf(prefs->font.face, sizeof(prefs->font.face), "%s", WLM_GRAPH_LABEL_FONT_FACE);
    prefs->font.size = WLM_GRAPH_LABEL_FONT_SIZE_BASE;
    prefs->font.weight = CAIRO_FONT_WEIGHT_NORMAL;
    prefs->font.slant = CAIRO_FONT_SLANT_NORMAL;
    prefs->show_label = has_label;

    for (int i = 1; i < argc; i++) {
        if (0 == strcmp(argv[i], "--interval") && i + 1 < argc) {
            double secs;
            if (!_wlm_graph_arg_parse_f64(argv[i], argv[i + 1], 0.01, 3600.0, &secs)) {
                return -1;
            }
            prefs->interval_usec = (uint64_t)(secs * 1000000.0);
            i++;
        } else if (0 == strcmp(argv[i], "--bezel-margin") && i + 1 < argc) {
            int32_t val;
            if (!_wlm_graph_arg_parse_i32(argv[i], argv[i + 1], 0, WLM_GRAPH_BASE_ICON_SIZE / 2, &val)) {
                return -1;
            }
            prefs->margin_logical_px = (uint32_t)val;
            i++;
        } else if (!has_custom_lut &&
                   0 == strcmp(argv[i], "--color-mode") && i + 1 < argc) {
            if (0 == strcmp(argv[i + 1], "alpha")) {
                prefs->color_mode = WLM_GRAPH_COLOR_MODE_ALPHA;
            } else if (0 == strcmp(argv[i + 1], "heat")) {
                prefs->color_mode = WLM_GRAPH_COLOR_MODE_HEAT;
            } else {
                fprintf(stderr, "Error: %s value '%s' must be 'alpha' or 'heat'\n",
                        argv[i], argv[i + 1]);
                return -1;
            }
            i++;
        } else if (has_label &&
                   0 == strcmp(argv[i], "--font") && i + 1 < argc) {
            if (!_wlm_graph_arg_parse_font(argv[i], argv[i + 1], &prefs->font)) {
                return -1;
            }
            i++;
        } else if (has_label && 0 == strcmp(argv[i], "--no-label")) {
            prefs->show_label = false;
        } else if (0 == strcmp(argv[i], "--help") || 0 == strcmp(argv[i], "-h")) {
            printf("%s\n\n", app_help);
            printf("Usage: %s [OPTIONS]\n", app_name);
            printf("  --interval SECS   Update interval 0.01-3600 seconds (default: 1.0)\n");
            printf("  --bezel-margin N  Bezel margin in logical pixels (default: 5)\n");
            if (!has_custom_lut) {
                printf("  --color-mode MODE Color mode: heat, alpha (default: heat)\n");
            }
            if (has_label) {
                printf("  --no-label        Disable label display\n");
                printf("  --font SPEC       XFT-style font (default: Monospace:size=8)\n");
                printf("                    (weight: normal|bold, slant: normal|italic|oblique)\n");
            }
            printf("  --help, -h        Show this help\n");
            return 1;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for usage.\n", app_name);
            return -1;
        }
    }

    return 0;
}

/* == Callbacks ============================================================ */

/* ------------------------------------------------------------------------- */
/**
 * Callback invoked when the icon needs to be rendered.
 *
 * @param gfxbuf_ptr        Graphics buffer to render into.
 * @param ud_ptr            User data pointer (wlm_graph_handle_t).
 *
 * @return true on success.
 */
static bool _wlm_graph_icon_render_callback(bs_gfxbuf_t *gfxbuf_ptr, void *ud_ptr)
{
    wlm_graph_handle_t * const handle = ud_ptr;
    wlm_graph_state_t * const gs = handle->graph_state;
    const uint32_t margin_logical_px = handle->prefs->margin_logical_px;
    const uint32_t size[2] = {gfxbuf_ptr->width, gfxbuf_ptr->height};

    // Check if dimensions changed.
    const bool size_changed = (size[0] != gs->icon_size[0] ||
                               size[1] != gs->icon_size[1]);

    // Reset graph buffers if icon size changed.
    if (size_changed) {
        if (!_wlm_graph_buffers_resize(gs, size, margin_logical_px, _wlm_graph_panic)) {
            bs_log(BS_ERROR, "Failed to reset graph buffers for %ux%u icon",
                   size[0], size[1]);
            return false;
        }
        // Re-render graph from samples at new resolution.
        _wlm_graph_rebuild_from_samples(gs, handle->config->accumulate_mode);
    }

    // Clear to transparent (so bezel margin shows dock background).
    bs_gfxbuf_clear(gfxbuf_ptr, 0);

    // Draw beveled bezel.
    if (0 < margin_logical_px) {
        _wlm_graph_bezel_draw(gfxbuf_ptr, margin_logical_px);
    }

    if (0 == gs->graph_size[0] || 0 == gs->graph_size[1]) {
        return true;  // No room for graph.
    }

    // Use pre-calculated margin offset from _wlm_graph_buffers_resize.
    const uint32_t offset[2] = {gs->margin_px, gs->margin_px};

    _wlm_graph_blit_to_buffer(gfxbuf_ptr, gs->graph_pixels, gs->graph_size, offset);

    // Draw label if callback provided.
    if (handle->prefs->show_label) {
        const char *label = handle->config->label_fn(handle->config->app_state);
        if (NULL != label) {
            _wlm_graph_label_draw(gfxbuf_ptr, gs->margin_px, label, handle->prefs);
        }
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Reads stats and updates the graph with a new sample.
 *
 * @param handle            Graph handle containing state and config.
 */
static void _wlm_graph_sample_update(wlm_graph_handle_t *handle)
{
    wlm_graph_state_t * const gs = handle->graph_state;
    const wlm_graph_app_config_t * const config = handle->config;

    // Need at least 2 columns for meaningful graph updates.
    if (NULL == gs->sample_current || gs->graph_size[0] < 2) {
        return;
    }

    // Reuse oldest sample for new data.
    wlm_graph_sample_t * const sample = gs->sample_current->next;

    // Read stats into sample's values buffer (callback handles reallocation).
    const wlm_graph_read_result_t read_result =
        config->stats_read_fn(config->app_state, &sample->values);
    if (WLM_GRAPH_READ_ERROR == read_result) {
        return;
    }

    // Handle regeneration request (scale changed).
    if (WLM_GRAPH_READ_OK_AND_REGENERATE == read_result) {
        BS_ASSERT(NULL != config->regenerate_fn);
        // Count historical samples (excluding the current one we just filled).
        uint32_t sample_count = 0;
        for (wlm_graph_sample_t *s = sample->prev;
             s != sample && sample_count < gs->graph_size[0];
             s = s->prev) {
            sample_count++;
        }

        if (sample_count > 0) {
            // Build temporary array of value buffers for regeneration.
            wlm_graph_values_t *samples = calloc(sample_count, sizeof(wlm_graph_values_t));
            if (NULL != samples) {
                wlm_graph_sample_t *s = sample->prev;
                for (uint32_t i = 0; i < sample_count; i++) {
                    samples[i] = s->values;
                    s = s->prev;
                }

                // Regenerate historical samples at new scale.
                config->regenerate_fn(config->app_state, samples, sample_count);

                // Copy regenerated values back and recompute peaks.
                s = sample->prev;
                for (uint32_t i = 0; i < sample_count; i++) {
                    s->values = samples[i];
                    _wlm_graph_sample_compute_peak(s, config->accumulate_mode);
                    s = s->prev;
                }

                free(samples);
            }
        }

        // Rebuild entire graph with regenerated samples.
        _wlm_graph_sample_compute_peak(sample, config->accumulate_mode);
        gs->sample_current = sample;
        _wlm_graph_rebuild_from_samples(gs, config->accumulate_mode);
        return;
    }

    // Compute peak value from the sample.
    _wlm_graph_sample_compute_peak(sample, config->accumulate_mode);

    // Scroll, render, and advance.
    _wlm_graph_update_with_sample(gs, sample, config->accumulate_mode);
}

/* ------------------------------------------------------------------------- */
/**
 * Timer callback for periodic graph updates.
 *
 * @param client_ptr        Wayland client instance.
 * @param ud_ptr            User data pointer (wlm_graph_handle_t).
 */
static void _wlm_graph_timer_callback(wlclient_t *client_ptr, void *ud_ptr)
{
    wlm_graph_handle_t * const handle = ud_ptr;

    _wlm_graph_sample_update(handle);

    wlclient_icon_register_ready_callback(
        handle->icon_ptr, _wlm_graph_icon_render_callback, handle);
    wlclient_register_timer(
        client_ptr, _wlm_graph_time_next_update(handle->prefs->interval_usec),
        _wlm_graph_timer_callback, handle);
}

/* == Internal state management ============================================ */

/* ------------------------------------------------------------------------- */
/**
 * Prints error message to stderr and terminates the application.
 *
 * @param msg               Error message to display.
 */
#ifdef __GNUC__
__attribute__((noreturn))
#endif
static void _wlm_graph_panic(const char *msg)
{
    fprintf(stderr, "wlm_graph: %s\n", msg);
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */
/**
 * Frees all resources associated with graph state.
 *
 * @param graph_state       Graph state to free, or NULL.
 */
static void _wlm_graph_state_free(wlm_graph_state_t *graph_state)
{
    if (NULL == graph_state) return;

    _wlm_graph_sample_values_free(graph_state->samples_alloc, graph_state->graph_size[0]);
    free(graph_state->samples_alloc);
    free(graph_state->row_counts);
    free(graph_state->graph_pixels);
    free(graph_state);
}

/* == Public API =========================================================== */

/* ------------------------------------------------------------------------- */
int wlm_graph_app_run(
    int argc,
    const char **argv,
    const wlm_graph_app_config_t *config)
{
    bs_log_severity = BS_INFO;

    // Parse command line arguments and initialize preferences.
    wlm_graph_prefs_t prefs;
    const bool has_custom_lut = (NULL != config->pixel_lut);
    const bool has_label = (NULL != config->label_fn);
    const int parse_result = _wlm_graph_args_parse(
        argc, argv, config->app_name, config->app_help,
        has_custom_lut, has_label, &prefs);
    if (0 != parse_result) {
        return (1 == parse_result) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Allocate graph state.
    wlm_graph_state_t *graph_state = calloc(1, sizeof(wlm_graph_state_t));
    if (NULL == graph_state) {
        fprintf(stderr, "%s: Failed to allocate graph state\n", config->app_name);
        return EXIT_FAILURE;
    }

    // Initialize pixel lookup table.
    if (NULL != config->pixel_lut) {
        memcpy(graph_state->pixel_lut, config->pixel_lut, sizeof(graph_state->pixel_lut));
        graph_state->pixel_line = WLM_GRAPH_PIXEL_LINE_DEFAULT;
    } else {
        _wlm_graph_pixel_lut_init(graph_state->pixel_lut, &graph_state->pixel_line, prefs.color_mode);
    }

    // Initialize graph buffers with default icon size.
    const uint32_t size_default[2] = {WLM_GRAPH_BASE_ICON_SIZE, WLM_GRAPH_BASE_ICON_SIZE};
    if (!_wlm_graph_buffers_resize(graph_state, size_default, prefs.margin_logical_px, _wlm_graph_panic)) {
        fprintf(stderr, "Error: Icon dimensions too small for graph\n");
        _wlm_graph_state_free(graph_state);
        return EXIT_FAILURE;
    }
    // Initialize graph pixels (malloc leaves garbage, need opaque black).
    _wlm_graph_rebuild_from_samples(graph_state, config->accumulate_mode);

    // Build wlclient app ID: "wlmaker.<app_name>".
    char wlclient_app_id[64];
    snprintf(wlclient_app_id, sizeof(wlclient_app_id), "wlmaker.%s", config->app_name);

    wlclient_t * const wlclient_ptr = wlclient_create(wlclient_app_id);
    if (NULL == wlclient_ptr) {
        _wlm_graph_state_free(graph_state);
        config->state_free_fn(config->app_state);
        return EXIT_FAILURE;
    }

    if (!wlclient_icon_supported(wlclient_ptr)) {
        bs_log(BS_ERROR, "Icon protocol is not supported.");
        _wlm_graph_state_free(graph_state);
        config->state_free_fn(config->app_state);
        wlclient_destroy(wlclient_ptr);
        return EXIT_FAILURE;
    }

    wlclient_icon_t * const icon_ptr = wlclient_icon_create(wlclient_ptr);
    if (NULL == icon_ptr) {
        bs_log(BS_ERROR, "Failed to create icon.");
        _wlm_graph_state_free(graph_state);
        config->state_free_fn(config->app_state);
        wlclient_destroy(wlclient_ptr);
        return EXIT_FAILURE;
    }

    // Create handle for callbacks.
    wlm_graph_handle_t handle = {
        .graph_state = graph_state,
        .prefs = &prefs,
        .icon_ptr = icon_ptr,
        .config = config,
    };

    wlclient_icon_register_ready_callback(
        icon_ptr, _wlm_graph_icon_render_callback, &handle);
    wlclient_register_timer(
        wlclient_ptr, _wlm_graph_time_next_update(prefs.interval_usec),
        _wlm_graph_timer_callback, &handle);

    wlclient_run(wlclient_ptr);
    wlclient_icon_destroy(icon_ptr);

    _wlm_graph_state_free(graph_state);
    config->state_free_fn(config->app_state);
    wlclient_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}

/* == End of wlm_graph_shared.c ============================================ */
