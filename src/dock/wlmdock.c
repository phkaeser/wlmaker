/* ========================================================================= */
/**
 * @file wlmdock.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#undef WLR_USE_UNSTABLE
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "toolkit/toolkit.h"
#include "util/backtrace.h"
#include "util/files.h"
#include "util/subprocess_monitor.h"
#include "wlclient/layer_surface.h"
#include "wlclient/wlclient.h"

#include "launcher.h"
#include "tilebox.h"
#include "subcompositor.h"

// TODO(kaeser@gubbe.ch): Move into a shared directory.
#include "../config.h"

/* == Declarations ========================================================= */

/** State of the nested client-backed dock. */
typedef struct {
    /** Client wrapper connecting to parent compositor. */
    wlmcl_client_t            *client_ptr;
    /** Layer shell surface on parent compositor. */
    wlmcl_layer_surface_t     *layer_surface_ptr;
    /** Parent's display connection. */
    struct wl_display         *remote_display_ptr;

    /** Local Wayland server display. */
    struct wl_display         *local_display_ptr;
    /** The subcompositor. */
    wlmdock_subcompositor_t   *subcompositor_ptr;

    /** Local wlroots backend. */
    struct wlr_backend        *wlr_backend_ptr;
    /** Local renderer. */
    struct wlr_renderer       *wlr_renderer_ptr;
    /** Local allocator. */
    struct wlr_allocator      *wlr_allocator_ptr;
    /** Box holding the tiles. */
    wlmdock_tilebox_t         *tilebox_ptr;

    /** Monitoring for subprocesses. */
    wlm_util_subprocess_monitor_t *subprocess_monitor_ptr;

    /** Event source for monitoring signals via client's signal_fd. */
    struct wl_event_source    *client_signal_event_source_ptr;
} wlmdock_t;

static int handle_client_signal(int fd, uint32_t mask, void *data_ptr);
static wlmdock_t *_wlmdock_create(
    wlm_util_files_t *files_ptr,
    wlmaker_config_style_t *style_ptr);
static void _wlmdock_destroy(wlmdock_t *dock_ptr);

/* == Data ================================================================= */

#if !defined(WLMAKER_VERSION_MAJOR) || !defined(WLMAKER_VERSION_MINOR) || !defined(WLMAKER_VERSION_FULL)
#error "WLMAKER_VERSION_... not defined!"
#else
// Patch level is optional.
#if defined(WLMAKER_VERSION_PATCH)
static const char *wlmdock_version_string =
    WLMAKER_VERSION_MAJOR "." WLMAKER_VERSION_MINOR "." WLMAKER_VERSION_PATCH;
#else
static const char *wlmdock_version_string =
    WLMAKER_VERSION_MAJOR "." WLMAKER_VERSION_MINOR;
#endif
static const char *wlmdock_version_full = WLMAKER_VERSION_FULL;
#endif

/** Will hold the value of --config_file. */
static char *wlmdock_arg_config_file_ptr = NULL;
/** Will hold the value of --theme_file. */
static char *wlmdock_arg_theme_file_ptr = NULL;

/** Definition of commandline arguments. */
static const bs_arg_t wlmdock_args[] = {
    BS_ARG_STRING(
        "config_file",
        "Optional: Path to a configuration file. If not provided, wlmaker "
        "will scan default paths for a configuration file, or fall back to "
        "a built-in configuration.",
        NULL,
        &wlmdock_arg_config_file_ptr),
    BS_ARG_STRING(
        "theme_file",
        "Optional: Path to a \"theme\" file, configuring the visual style for "
        "elements. If not provided, wlmaker will use a built-in default theme.",
        NULL,
        &wlmdock_arg_theme_file_ptr),
    bs_arg_log_level,
    BS_ARG_SENTINEL()
};

/** Compiled regular expression for extracting file & line no. from wlr_log. */
static regex_t                wlmdock_wlr_log_regex;
/** Regular expression string for extracting file & line no. from wlr_log. */
static const char             *wlmdock_wlr_log_regex_string =
    "^\\[([^\\:]+)\\:([0-9]+)\\]\\ ";

/* ------------------------------------------------------------------------- */
/**
 * Wraps the wlr_log calls on bs_log.
 *
 * @param importance
 * @param fmt
 * @param args
 */
static void wlr_to_bs_log(
    enum wlr_log_importance importance,
    const char *fmt,
    va_list args)
{
    bs_log_severity_t severity = BS_DEBUG;

    switch (importance) {
    case WLR_SILENT:  // Fall-through to DEBUG severity.
    case WLR_DEBUG: severity = BS_DEBUG; break;
    case WLR_INFO: severity = BS_INFO; break;
    case WLR_ERROR: severity = BS_ERROR; break;
    default: severity = BS_INFO; break;
    }

    if (!bs_will_log(severity)) return;

    // Log to buffer. Ignores overflows.
    char buf[BS_LOG_MAX_BUF_SIZE];
    vsnprintf(buf, sizeof(buf), fmt, args);

    regmatch_t matches[4];
    if (0 != regexec(&wlmdock_wlr_log_regex, buf, 4, &matches[0], 0) ||
        matches[0].rm_so != 0 ||
        !(matches[0].rm_eo >= 6) ||  // Minimum "[x:1] ".
        matches[1].rm_so != 1 ||
        !(matches[2].rm_so > 2) ||
        matches[3].rm_so != -1) {
        bs_log(severity, "%s (wlr_log unexpected format!)", buf);
        return;
    }

    buf[matches[1].rm_eo] = '\0';
    buf[matches[2].rm_eo] = '\0';
    uint64_t line_no = 0;
    bs_strconvert_uint64(&buf[matches[2].rm_so], &line_no, 10);
    line_no = BS_MIN((uint64_t)INT_MAX, line_no);

    bs_log_write(severity, &buf[matches[1].rm_so], (int)line_no, "%s",
        &buf[matches[0].rm_eo]);
}

/* == Main program ========================================================= */
/** The dock's main program. */
int main(int argc, const char **argv)
{
    if (!wlm_util_backtrace_setup(argv[0])) return EXIT_FAILURE;

    for (int i = 1; i < argc; ++i) {
        if (0 == strcmp(argv[i], "--help")) {
            bs_arg_print_usage(stdout, wlmdock_args);
            return EXIT_SUCCESS;
        } else if (0 == strcmp(argv[i], "--version")) {
            fprintf(stdout, "wlmdock version %s (%s)\n",
                    wlmdock_version_string, wlmdock_version_full);
            return EXIT_SUCCESS;
        } else {
            bs_log(BS_ERROR, "Unhandled extra argument \"%s\"", argv[i]);
            return EXIT_FAILURE;
        }
    }

    int rv = regcomp(
        &wlmdock_wlr_log_regex,
        wlmdock_wlr_log_regex_string,
        REG_EXTENDED);
    if (0 != rv) {
        char err_buf[512];
        regerror(rv, &wlmdock_wlr_log_regex, err_buf, sizeof(err_buf));
        bs_log(BS_ERROR, "Failed compiling regular expression: %s", err_buf);
        return EXIT_FAILURE;
    }
    wlr_log_init(WLR_DEBUG, wlr_to_bs_log);

    bs_log_severity = BS_INFO;  // Will be overwritten in bs_arg_parse().
    if (!bs_arg_parse(wlmdock_args, BS_ARG_MODE_EXTRA_ARGS, &argc, argv)) {
        fprintf(stderr, "Failed to parse commandline arguments.\n");
        bs_arg_print_usage(stderr, wlmdock_args);
        return EXIT_FAILURE;
    }

    bs_log(BS_INFO, "Starting wlmdock %s (%s)",
           wlmdock_version_string, wlmdock_version_full);

    wlm_util_files_t *files_ptr = wlm_util_files_create("wlmaker");
    if (NULL == files_ptr) {
        bs_log(BS_ERROR, "Failed wlm_util_files_create(\"wlmaker\")");
        return EXIT_FAILURE;
    }

    bspl_dict_t *config_dict_ptr = wlmaker_config_load(
        files_ptr, wlmdock_arg_config_file_ptr);
    if (NULL != wlmdock_arg_config_file_ptr) free(wlmdock_arg_config_file_ptr);
    if (NULL == config_dict_ptr) {
        fprintf(stderr, "Failed to load & initialize configuration.\n");
        return EXIT_FAILURE;
    }

    const char *theme_file_ptr = wlmdock_arg_theme_file_ptr;
    if (NULL == theme_file_ptr) {
        theme_file_ptr = bspl_dict_get_string_value(
            config_dict_ptr, "ThemeFile");
    }
    wlmaker_config_style_t style = {};
    bool theme_loaded = wlmaker_theme_load(files_ptr, theme_file_ptr, &style);
    bspl_dict_unref(config_dict_ptr);
    if (!theme_loaded) {
        fprintf(stderr, "Failed to load & initialize theme.\n");
        return EXIT_FAILURE;
    }

    wlmdock_t *dock_ptr = _wlmdock_create(files_ptr, &style);
    if (NULL == dock_ptr) {
        bs_log(BS_ERROR, "Failed to create wlmdock.");
        return EXIT_FAILURE;
    }

    if (0 > wl_display_roundtrip(dock_ptr->remote_display_ptr)) {
        bs_log(BS_ERROR, "Failed parent wl_display_roundtrip.");
        _wlmdock_destroy(dock_ptr);
        return EXIT_FAILURE;
    }

    bs_log(BS_INFO, "wlmdock: entering event loop.");
    wl_display_run(dock_ptr->local_display_ptr);
    bs_log(BS_INFO, "wlmdock: event loop exited.");

    _wlmdock_destroy(dock_ptr);
    bspl_decoded_destroy(wlmaker_config_style_desc, &style);

    if (NULL != files_ptr) wlm_util_files_destroy(files_ptr);

    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/** Creates and initializes wlmdock_t. */
wlmdock_t *_wlmdock_create(
    wlm_util_files_t *files_ptr,
    wlmaker_config_style_t *style_ptr)
{
    wlmdock_t *dock_ptr = logged_calloc(1, sizeof(wlmdock_t));
    if (NULL == dock_ptr) return NULL;

    // 1. Initialize client wrapper and connect to parent compositor.
    dock_ptr->client_ptr = wlmcl_client_create("wlmaker.wlmdock");
    if (NULL == dock_ptr->client_ptr) {
        bs_log(BS_ERROR, "Failed to connect to parent compositor.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    const struct wlmcl_client_attributes *attrs =
        wlmcl_client_attributes(dock_ptr->client_ptr);
    dock_ptr->remote_display_ptr = attrs->wl_display_ptr;

    // 2. Create the client-side layer shell surface.
    dock_ptr->layer_surface_ptr = wlmcl_layer_surface_create(
        dock_ptr->client_ptr,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "wlmdock",
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT, // | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
        64,
        64);
    if (NULL == dock_ptr->layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed to create client layer surface.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    wlmcl_layer_surface_set_exclusive_zone(dock_ptr->layer_surface_ptr, 64);

    // 3. Setup local Wayland server display and event loops.
    dock_ptr->local_display_ptr = wl_display_create();
    if (NULL == dock_ptr->local_display_ptr) {
        bs_log(BS_ERROR, "Failed to create local server wl_display.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 4. Initialize wlroots backend, renderer, allocator and nested output.
    dock_ptr->wlr_backend_ptr = wlr_wl_backend_create(
        wl_display_get_event_loop(dock_ptr->local_display_ptr),
        dock_ptr->remote_display_ptr);
    if (NULL == dock_ptr->wlr_backend_ptr) {
        bs_log(BS_ERROR, "Failed to create nested Wayland backend.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 5. Setup dock contents.
    dock_ptr->tilebox_ptr = wlmdock_tilebox_create(
        WLMTK_BOX_VERTICAL,
        &style_ptr->dock);
    if (NULL == dock_ptr->tilebox_ptr) {
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 6. Prepare renderer and allocator.
    dock_ptr->wlr_renderer_ptr = wlr_renderer_autocreate(
        dock_ptr->wlr_backend_ptr);
    if (NULL == dock_ptr->wlr_renderer_ptr) {
        bs_log(BS_ERROR, "Failed to create renderer.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    wlr_renderer_init_wl_display(
        dock_ptr->wlr_renderer_ptr,
        dock_ptr->local_display_ptr);

    dock_ptr->wlr_allocator_ptr = wlr_allocator_autocreate(
        dock_ptr->wlr_backend_ptr, dock_ptr->wlr_renderer_ptr);
    if (NULL == dock_ptr->wlr_allocator_ptr) {
        bs_log(BS_ERROR, "Failed to create allocator.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 7. Create subcompositor.
    dock_ptr->subcompositor_ptr = wlmdock_subcompositor_create(
        dock_ptr->local_display_ptr,
        dock_ptr->wlr_backend_ptr,
        dock_ptr->client_ptr,
        dock_ptr->layer_surface_ptr,
        // TODO(kaeser@gubbe.ch): Find a way to not provide a cursor style,
        // when using the cursor shape extension.
        &style_ptr->cursor,
        wlmdock_tilebox_container(dock_ptr->tilebox_ptr));
    if (NULL == dock_ptr->subcompositor_ptr) {
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 8. Start it all.
    if (!wlr_backend_start(dock_ptr->wlr_backend_ptr)) {
        bs_log(BS_ERROR, "Failed to start wlroots backend.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    wlmdock_subcompositor_start(
        dock_ptr->subcompositor_ptr,
        dock_ptr->wlr_backend_ptr,
        dock_ptr->wlr_allocator_ptr,
        dock_ptr->wlr_renderer_ptr);

    // 9. Monitor the client's signal_fd for signals (like SIGINT).
    struct wl_event_loop *event_loop_ptr = wl_display_get_event_loop(
        dock_ptr->local_display_ptr);
    dock_ptr->client_signal_event_source_ptr = wl_event_loop_add_fd(
        event_loop_ptr,
        wlmcl_client_signal_fd(dock_ptr->client_ptr),
        WL_EVENT_READABLE,
        handle_client_signal,
        dock_ptr);
    if (NULL == dock_ptr->client_signal_event_source_ptr) {
        bs_log(BS_ERROR, "Failed to register client signal fd to event loop.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->subprocess_monitor_ptr = wlm_util_subprocess_monitor_create(
        event_loop_ptr);
    if (NULL == dock_ptr->subprocess_monitor_ptr) {
        bs_log(BS_ERROR, "Failed wlm_util_subprocess_monitor_create(%p)",
               event_loop_ptr);
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    struct wlr_compositor *wlr_compositor_ptr = wlr_compositor_create(
        dock_ptr->local_display_ptr, 5, dock_ptr->wlr_renderer_ptr);
    if (NULL == wlr_compositor_ptr) {
        bs_log(BS_ERROR, "Failed to create compositor.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    static const char *plist_ptr =
        "{CommandLine = \"/usr/bin/foot\"; Icon = \"chrome-56x56.png\";}";
    bspl_dict_t *dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(plist_ptr));
    wlmdock_launcher_t *launcher_ptr = wlmdock_launcher_create_from_plist(
            &style_ptr->tile,
            dict_ptr,
            dock_ptr->subprocess_monitor_ptr,
            files_ptr);
    wlmdock_tilebox_add_tile(
        dock_ptr->tilebox_ptr,
        wlmdock_launcher_tile(launcher_ptr));

    launcher_ptr = wlmdock_launcher_create_from_plist(
            &style_ptr->tile,
            dict_ptr,
            dock_ptr->subprocess_monitor_ptr,
            files_ptr);
    wlmdock_tilebox_add_tile(
        dock_ptr->tilebox_ptr,
        wlmdock_launcher_tile(launcher_ptr));

    bspl_dict_unref(dict_ptr);

    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys wlmdock_t and frees all resources. */
void _wlmdock_destroy(wlmdock_t *dock_ptr)
{
    if (NULL == dock_ptr) return;

    if (NULL != dock_ptr->subprocess_monitor_ptr) {
        wlm_util_subprocess_monitor_destroy(dock_ptr->subprocess_monitor_ptr);
        dock_ptr->subprocess_monitor_ptr = NULL;
    }

    if (NULL != dock_ptr->client_signal_event_source_ptr) {
        wl_event_source_remove(dock_ptr->client_signal_event_source_ptr);
        dock_ptr->client_signal_event_source_ptr = NULL;
    }

    if (NULL != dock_ptr->subcompositor_ptr) {
        wlmdock_subcompositor_destroy(dock_ptr->subcompositor_ptr);
        dock_ptr->subcompositor_ptr = NULL;
    }
    if (NULL != dock_ptr->wlr_renderer_ptr) {
        wlr_renderer_destroy(dock_ptr->wlr_renderer_ptr);
        dock_ptr->wlr_renderer_ptr = NULL;
    }
    if (NULL != dock_ptr->wlr_allocator_ptr) {
        wlr_allocator_destroy(dock_ptr->wlr_allocator_ptr);
        dock_ptr->wlr_allocator_ptr = NULL;
    }
    if (NULL != dock_ptr->tilebox_ptr) {
        wlmdock_tilebox_destroy(dock_ptr->tilebox_ptr);
        dock_ptr->tilebox_ptr = NULL;
    }

    if (NULL != dock_ptr->local_display_ptr) {
        wl_display_destroy(dock_ptr->local_display_ptr);
        dock_ptr->local_display_ptr = NULL;
    }

    if (NULL != dock_ptr->layer_surface_ptr) {
        wlmcl_layer_surface_destroy(dock_ptr->layer_surface_ptr);
        dock_ptr->layer_surface_ptr = NULL;
    }
    if (NULL != dock_ptr->client_ptr) {
        wlmcl_client_destroy(dock_ptr->client_ptr);
        dock_ptr->client_ptr = NULL;
    }

    free(dock_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles client's signal fd events. */
int handle_client_signal(int fd, uint32_t mask, void *data_ptr)
{
    wlmdock_t *dock_ptr = data_ptr;
    struct signalfd_siginfo siginfo;

    if (mask & WL_EVENT_READABLE) {
        ssize_t rd = read(fd, &siginfo, sizeof(siginfo));
        if (rd == sizeof(siginfo)) {
            bs_log(BS_INFO, "wlmdock: Signal %d caught, exiting loop.",
                   siginfo.ssi_signo);
            wl_display_terminate(dock_ptr->local_display_ptr);
        }
    }
    return 0;
}

/* == End of wlmdock.c ===================================================== */
