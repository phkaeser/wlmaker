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
#include <time.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#undef WLR_USE_UNSTABLE
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "input/cursor.h"
#include "input/manager.h"
#include "toolkit/toolkit.h"
#include "wlclient/layer_surface.h"
#include "wlclient/wlclient.h"
#include "util/backtrace.h"
#include "util/files.h"

#include "launcher.h"
#include "tilebox.h"

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
    /** Local scene graph. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Local output layout manager. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Local wlroots backend. */
    struct wlr_backend        *wlr_backend_ptr;
    /** Local renderer. */
    struct wlr_renderer       *wlr_renderer_ptr;
    /** Local allocator. */
    struct wlr_allocator      *wlr_allocator_ptr;
    /** Nested output wrapped by backend. */
    struct wlr_output         *wlr_output_ptr;
    /** Node in the scene graph backing the output. */
    struct wlr_scene_output   *wlr_scene_output_ptr;
    /** Root wrapper. */
    wlmtk_root_t              *root_ptr;
    /** Box holding the tiles. */
    wlmdock_tilebox_t         *tilebox_ptr;

    /** Local input manager. */
    wlmim_t                   *input_manager_ptr;

    /** Listener for @ref wlmcl_client_events::keymap. */
    struct wl_listener        wlclient_keymap_listener;
    /** Listener for @ref wlmcl_client_events::keyboard_repeat_info. */
    struct wl_listener        wlclient_keyboard_repeat_info_listener;

    /** Listener for output frame callback. */
    struct wl_listener         frame_listener;
    /** Listener for output destruction. */
    struct wl_listener         output_destroy_listener;

    /** Event source for monitoring signals via client's signal_fd. */
    struct wl_event_source    *client_signal_event_source_ptr;
} wlmdock_t;

static void _wlmdock_handle_wlclient_keymap(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmdock_handle_wlclient_keyboard_repeat_info(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_output_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void handle_configure(void *ud_ptr, uint32_t width, uint32_t height);
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

/** Log levels. */
static const bs_arg_enum_table_t wlmdock_log_levels[] = {
    { .name_ptr = "DEBUG", BS_DEBUG },
    { .name_ptr = "INFO", BS_INFO },
    { .name_ptr = "WARNING", BS_WARNING },
    { .name_ptr = "ERROR", BS_ERROR },
    { .name_ptr = NULL },
};

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
    BS_ARG_ENUM(
        "log_level",
        "Log level to apply. One of DEBUG, INFO, WARNING, ERROR.",
        "INFO",
        &wlmdock_log_levels[0],
        (int*)&bs_log_severity),
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
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(dock_ptr->client_ptr)->keymap,
        &dock_ptr->wlclient_keymap_listener,
        _wlmdock_handle_wlclient_keymap);
    wlmtk_util_connect_listener_signal(
        &wlmcl_client_events(dock_ptr->client_ptr)->keyboard_repeat_info,
        &dock_ptr->wlclient_keyboard_repeat_info_listener,
        _wlmdock_handle_wlclient_keyboard_repeat_info);

    // 2. Create the client-side layer shell surface.
    dock_ptr->layer_surface_ptr = wlmcl_layer_surface_create(
        dock_ptr->client_ptr,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "wlmdock",
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
        64,
        64);
    if (NULL == dock_ptr->layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed to create client layer surface.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    struct wl_surface *wl_surface_ptr =
        wlmcl_layer_surface_wl_surface(dock_ptr->layer_surface_ptr);

    // 3. Setup local Wayland server display and event loops.
    dock_ptr->local_display_ptr = wl_display_create();
    if (NULL == dock_ptr->local_display_ptr) {
        bs_log(BS_ERROR, "Failed to create local server wl_display.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == dock_ptr->wlr_scene_ptr) {
        bs_log(BS_ERROR, "Failed to create scene graph.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->wlr_output_layout_ptr = wlr_output_layout_create(
        dock_ptr->local_display_ptr);
    if (NULL == dock_ptr->wlr_output_layout_ptr) {
        bs_log(BS_ERROR, "Failed to create output layout.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->tilebox_ptr = wlmdock_tilebox_create(
        &dock_ptr->wlr_scene_ptr->tree,
        WLMTK_BOX_VERTICAL,
        &style_ptr->dock);
    if (NULL == dock_ptr->tilebox_ptr) {
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    dock_ptr->root_ptr = wlmtk_root_create(
        wlmdock_tilebox_element(dock_ptr->tilebox_ptr),
        dock_ptr->wlr_output_layout_ptr);
    if (NULL == dock_ptr->root_ptr) {
        bs_log(BS_ERROR, "Failed to create root element wrapper.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    static const char *plist_ptr =
        "{CommandLine = \"/usr/bin/foot\"; Icon = \"chrome-56x56.png\";}";
    bspl_dict_t *dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(plist_ptr));
    wlmdock_launcher_t *launcher_ptr = wlmdock_launcher_create_from_plist(
            &style_ptr->tile, dict_ptr, files_ptr);
    wlmdock_tilebox_add_tile(
        dock_ptr->tilebox_ptr,
        wlmdock_launcher_tile(launcher_ptr));

    // 5. Initialize wlroots backend, renderer, allocator and nested output.
    dock_ptr->wlr_backend_ptr = wlr_wl_backend_create(
        wl_display_get_event_loop(dock_ptr->local_display_ptr),
        dock_ptr->remote_display_ptr);
    if (NULL == dock_ptr->wlr_backend_ptr) {
        bs_log(BS_ERROR, "Failed to create nested Wayland backend.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 6. Create input seat and input manager before starting the backend
    // and before creating the output, so we don't miss the new_input events.
    struct wlr_seat *wlr_seat_ptr = wlr_seat_create(
        dock_ptr->local_display_ptr, "default");
    if (NULL == wlr_seat_ptr) {
        bs_log(BS_ERROR, "Failed to create wlr_seat.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->input_manager_ptr = wlmim_input_manager_create(
        dock_ptr->local_display_ptr,
        dock_ptr->wlr_backend_ptr,
        dock_ptr->wlr_output_layout_ptr,
        wlr_seat_ptr,
        NULL,
        // TODO(kaeser@gubbe.ch): Find a way to not provide a cursor style,
        // when using the cursor shape extension.
        &style_ptr->cursor,
        dock_ptr->root_ptr);
    if (NULL == dock_ptr->input_manager_ptr) {
        bs_log(BS_ERROR, "Failed to initialize input manager.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->wlr_renderer_ptr = wlr_renderer_autocreate(dock_ptr->wlr_backend_ptr);
    if (NULL == dock_ptr->wlr_renderer_ptr) {
        bs_log(BS_ERROR, "Failed to create renderer.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }
    wlr_renderer_init_wl_display(dock_ptr->wlr_renderer_ptr, dock_ptr->local_display_ptr);

    dock_ptr->wlr_allocator_ptr = wlr_allocator_autocreate(
        dock_ptr->wlr_backend_ptr, dock_ptr->wlr_renderer_ptr);
    if (NULL == dock_ptr->wlr_allocator_ptr) {
        bs_log(BS_ERROR, "Failed to create allocator.");
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

    if (!wlr_backend_start(dock_ptr->wlr_backend_ptr)) {
        bs_log(BS_ERROR, "Failed to start wlroots backend.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    dock_ptr->wlr_output_ptr = wlr_wl_output_create_from_surface(
        dock_ptr->wlr_backend_ptr, wl_surface_ptr);
    if (NULL == dock_ptr->wlr_output_ptr) {
        bs_log(BS_ERROR, "Failed to create nested output from surface.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    if (!wlr_output_init_render(dock_ptr->wlr_output_ptr, dock_ptr->wlr_allocator_ptr, dock_ptr->wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed to initialize output render.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    wlr_output_layout_add_auto(dock_ptr->wlr_output_layout_ptr, dock_ptr->wlr_output_ptr);

    struct wlr_scene_output_layout *wlr_scene_output_layout_ptr =
        wlr_scene_attach_output_layout(dock_ptr->wlr_scene_ptr, dock_ptr->wlr_output_layout_ptr);
    if (NULL == wlr_scene_output_layout_ptr) {
        bs_log(BS_ERROR, "Failed to attach output layout to scene.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // 8. Register layer-surface configure listener.
    wlmcl_layer_surface_register_configure_callback(
        dock_ptr->layer_surface_ptr,
        handle_configure,
        dock_ptr);

    // 9. Monitor the client's signal_fd for signals (like SIGINT).
    struct wl_event_loop *event_loop_ptr = wl_display_get_event_loop(dock_ptr->local_display_ptr);
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

    // Commit empty client surface to trigger the parent compositor configure request.
    wl_surface_commit(wl_surface_ptr);

    return dock_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys wlmdock_t and frees all resources. */
void _wlmdock_destroy(wlmdock_t *dock_ptr)
{
    if (NULL == dock_ptr) return;

    if (NULL != dock_ptr->client_signal_event_source_ptr) {
        wl_event_source_remove(dock_ptr->client_signal_event_source_ptr);
        dock_ptr->client_signal_event_source_ptr = NULL;
    }

    if (NULL != dock_ptr->input_manager_ptr) {
        wlmim_input_manager_destroy(dock_ptr->input_manager_ptr);
    }

    if (NULL != dock_ptr->root_ptr) {
        wlmtk_root_destroy(dock_ptr->root_ptr);
        dock_ptr->root_ptr = NULL;
    }
    if (NULL != dock_ptr->tilebox_ptr) {
        wlmdock_tilebox_destroy(dock_ptr->tilebox_ptr);
        dock_ptr->tilebox_ptr = NULL;
    }

    if (NULL != dock_ptr->wlr_output_ptr) {
        wlr_output_destroy(dock_ptr->wlr_output_ptr);
        dock_ptr->wlr_output_ptr = NULL;
    }
    if (NULL != dock_ptr->wlr_allocator_ptr) {
        wlr_allocator_destroy(dock_ptr->wlr_allocator_ptr);
    }
    if (NULL != dock_ptr->wlr_renderer_ptr) {
        wlr_renderer_destroy(dock_ptr->wlr_renderer_ptr);
    }
    if (NULL != dock_ptr->wlr_output_layout_ptr) {
        wlr_output_layout_destroy(dock_ptr->wlr_output_layout_ptr);
    }
    if (NULL != dock_ptr->wlr_scene_ptr) {
        wlr_scene_node_destroy(&dock_ptr->wlr_scene_ptr->tree.node);
    }
    if (NULL != dock_ptr->local_display_ptr) {
        wl_display_destroy(dock_ptr->local_display_ptr);
    }
    if (NULL != dock_ptr->layer_surface_ptr) {
        wlmcl_layer_surface_destroy(dock_ptr->layer_surface_ptr);
    }
    if (NULL != dock_ptr->client_ptr) {
        wlmtk_util_disconnect_listener(
            &dock_ptr->wlclient_keymap_listener);
        wlmtk_util_disconnect_listener(
            &dock_ptr->wlclient_keyboard_repeat_info_listener);
        wlmcl_client_destroy(dock_ptr->client_ptr);
    }

    free(dock_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Forwards the keymap to the input manager. */
static void _wlmdock_handle_wlclient_keymap(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmdock_t *dock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_t, wlclient_keymap_listener);
    struct xkb_keymap *xkb_keymap_ptr = data_ptr;

    if (NULL == dock_ptr->input_manager_ptr) return;
    wlmim_input_manager_set_keymap(
        dock_ptr->input_manager_ptr,
        xkb_keymap_ptr);
}

/* ------------------------------------------------------------------------- */
/** Forwards the keyboard repeat info to the input manager. */
static void _wlmdock_handle_wlclient_keyboard_repeat_info(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmdock_t *dock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmdock_t, wlclient_keyboard_repeat_info_listener);

    if (NULL == dock_ptr->input_manager_ptr) return;
    int32_t r, d;
    if (wlmcl_client_get_repeat_info(dock_ptr->client_ptr, &r, &d)) {
        wlmim_input_manager_set_repeat_info(dock_ptr->input_manager_ptr, r, d);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles `wlr_output.events.frame`: commits scene passes and sends frame_done. */
void handle_frame(struct wl_listener *listener_ptr, __UNUSED__ void *data_ptr)
{
    wlmdock_t *dock_ptr = wl_container_of(
        listener_ptr, dock_ptr, frame_listener);

    if (NULL != dock_ptr->wlr_scene_output_ptr) {
        if (!wlr_scene_output_commit(dock_ptr->wlr_scene_output_ptr, NULL)) {
            bs_log(BS_WARNING, "wlr_scene_output_commit() failed.");
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(
            dock_ptr->wlr_scene_output_ptr, &now);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles output destruction: cleans listener references and terminates. */
void handle_output_destroy(struct wl_listener *listener_ptr, __UNUSED__ void *data_ptr)
{
    wlmdock_t *dock_ptr = wl_container_of(
        listener_ptr, dock_ptr, output_destroy_listener);
    wl_list_remove(&dock_ptr->frame_listener.link);
    wl_list_remove(&dock_ptr->output_destroy_listener.link);
    dock_ptr->wlr_output_ptr = NULL;
    dock_ptr->wlr_scene_output_ptr = NULL;
    bs_log(BS_INFO, "wlmdock: output destroyed, exiting loop.");
    wl_display_terminate(dock_ptr->local_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Configures the nested output mode and background rect size on resize. */
void handle_configure(void *ud_ptr, uint32_t width, uint32_t height)
{
    wlmdock_t *dock_ptr = ud_ptr;

    bs_log(BS_INFO, "wlmdock: configuring output %p to %dx%d.",
           (void *)dock_ptr->wlr_output_ptr, width, height);

    if (NULL == dock_ptr->wlr_scene_output_ptr) {
        dock_ptr->wlr_scene_output_ptr = wlr_scene_output_create(
            dock_ptr->wlr_scene_ptr, dock_ptr->wlr_output_ptr);
        if (NULL == dock_ptr->wlr_scene_output_ptr) {
            bs_log(BS_ERROR, "Failed to create scene output.");
            return;
        }

        dock_ptr->frame_listener.notify = handle_frame;
        wl_signal_add(&dock_ptr->wlr_output_ptr->events.frame,
                      &dock_ptr->frame_listener);
        dock_ptr->output_destroy_listener.notify = handle_output_destroy;
        wl_signal_add(&dock_ptr->wlr_output_ptr->events.destroy,
                      &dock_ptr->output_destroy_listener);
    }

    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmdock_tilebox_element(dock_ptr->tilebox_ptr));

    if ((int)width != box.width || (int)height != box.height) {
        wlmcl_layer_surface_request_size(
            dock_ptr->layer_surface_ptr,
            box.width, box.height);
        return;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    wlr_output_state_set_custom_mode(
        &state, (int32_t)width, (int32_t)height, 0);
    if (!wlr_output_commit_state(dock_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_ERROR, "Failed wlr_output_commit_state(%dx%d).",
               width, height);
        wlr_output_state_finish(&state);
        return;
    }
    wlr_output_state_finish(&state);

    if (!wlr_scene_output_commit(dock_ptr->wlr_scene_output_ptr, NULL)) {
        bs_log(BS_WARNING, "First wlr_scene_output_commit() failed.");
    }
}

/* ------------------------------------------------------------------------- */
/** Handles client's signal fd events. */
int handle_client_signal(int fd, uint32_t mask, void *data_ptr)
{
    wlmdock_t *dock_ptr = data_ptr;
    struct signalfd_siginfo siginfo;

    if (mask & WL_EVENT_READABLE) {
        ssize_t rd = read(fd, &siginfo, sizeof(siginfo));
        if (rd == sizeof(siginfo)) {
            bs_log(BS_INFO, "wlmdock: Signal %d caught, exiting loop.", siginfo.ssi_signo);
            wl_display_terminate(dock_ptr->local_display_ptr);
        }
    }
    return 0;
}

/* == End of wlmdock.c ===================================================== */
