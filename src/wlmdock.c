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

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
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
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"
#include "util/backtrace.h"
#include "util/files.h"
#include "util/subprocess_monitor.h"
#include "util/wlr_log.h"
#include "wlclient/layer_surface.h"
#include "wlclient/wlclient.h"

#include "dock/launcher.h"
#include "dock/tilebox.h"
#include "dock/subcompositor.h"

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

/** TODO: Replace this. */
struct wlmdock_state {
    /** Positioning data. */
    wlmtk_dock_positioning_t  positioning;
    /** Launchers. */
    bspl_array_t            *launchers_array_ptr;
};

static wlmdock_t *_wlmdock_create(
    const wlmtk_dock_positioning_t positioning,
    wlmaker_config_style_t *style_ptr);
static void _wlmdock_destroy(wlmdock_t *dock_ptr);
static int handle_client_signal(int fd, uint32_t mask, void *data_ptr);
static bool _wlmdock_decode_launchers(
    bspl_object_t *object_ptr,
    const union bspl_desc_value *desc_value_ptr,
    void *value_ptr);

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
/** Will hold the value of --state_file. */
static char *wlmdock_arg_state_file_ptr = NULL;
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
        "state_file",
        "Optional: Path to a state file, with state of workspaces, dock and "
        "clips configured. If not provided, wlmaker will scan default paths "
        "for a state file, or fall back to a built-in default.",
        NULL,
        &wlmdock_arg_state_file_ptr),
    BS_ARG_STRING(
        "theme_file",
        "Optional: Path to a \"theme\" file, configuring the visual style for "
        "elements. If not provided, wlmaker will use a built-in default theme.",
        NULL,
        &wlmdock_arg_theme_file_ptr),
    bs_arg_log_level,
    BS_ARG_SENTINEL()
};

/** Enum descriptor for `enum wlr_edges`. */
static const bspl_enum_desc_t _wlmdock_edges[] = {
    BSPL_ENUM("TOP", WLR_EDGE_TOP),
    BSPL_ENUM("BOTTOM", WLR_EDGE_BOTTOM),
    BSPL_ENUM("LEFT", WLR_EDGE_LEFT),
    BSPL_ENUM("RIGHT", WLR_EDGE_RIGHT),
    BSPL_ENUM_SENTINEL(),
};


/** Descriptor for the dock's plist. */
const bspl_desc_t _wlmdock_plist_desc[] = {
    BSPL_DESC_ENUM("Edge", true, struct wlmdock_state,
                   positioning.edge, positioning.edge,
                   WLR_EDGE_NONE, _wlmdock_edges),
    BSPL_DESC_ENUM("Anchor", true, struct wlmdock_state,
                   positioning.anchor, positioning.anchor,
                   WLR_EDGE_NONE, _wlmdock_edges),
    BSPL_DESC_CUSTOM("Launchers", true, struct wlmdock_state,
                     launchers_array_ptr, launchers_array_ptr,
                     _wlmdock_decode_launchers,
                     NULL,
                     NULL,
                     NULL),
    BSPL_DESC_SENTINEL(),
};

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

    if (!wlm_util_wlr_log_init(WLR_DEBUG)) return EXIT_FAILURE;

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

    bspl_dict_t *state_dict_ptr = wlmaker_state_load(
        files_ptr, wlmdock_arg_state_file_ptr);
    if (NULL != wlmdock_arg_state_file_ptr) free(wlmdock_arg_state_file_ptr);
    if (NULL == state_dict_ptr) {
        fprintf(stderr, "Failed to load & initialize state.\n");
        return EXIT_FAILURE;
    }
    bspl_dict_t *dock_dict_ptr = bspl_dict_get_dict(state_dict_ptr, "Dock");
    struct wlmdock_state state = {};
    if (!bspl_decode_dict(dock_dict_ptr, _wlmdock_plist_desc, &state)) {
        bs_log(BS_ERROR, "Failed to parse the State.");
        return EXIT_FAILURE;
    }

    wlmdock_t *dock_ptr = _wlmdock_create(state.positioning, &style);
    if (NULL == dock_ptr) {
        bs_log(BS_ERROR, "Failed to create wlmdock.");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < bspl_array_size(state.launchers_array_ptr); ++i) {
        bspl_dict_t *dict_ptr = bspl_dict_from_object(
            bspl_array_at(state.launchers_array_ptr, i));
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "Elements of 'Launchers' must be dicts.");
            return EXIT_FAILURE;
        }

        wlmdock_launcher_t *launcher_ptr = wlmdock_launcher_create_from_plist(
            &style.tile,
            dict_ptr,
            dock_ptr->subprocess_monitor_ptr,
            files_ptr);
        if (NULL == launcher_ptr) return EXIT_FAILURE;
        wlmdock_tilebox_add_tile(
            dock_ptr->tilebox_ptr,
            wlmdock_launcher_tile(launcher_ptr));
    }
    if (state.launchers_array_ptr) bspl_array_unref(state.launchers_array_ptr);

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

    bspl_dict_unref(state_dict_ptr);
    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/** Creates and initializes wlmdock_t. */
wlmdock_t *_wlmdock_create(
    const wlmtk_dock_positioning_t positioning,
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
        "wlmdock");
    if (NULL == dock_ptr->layer_surface_ptr) {
        bs_log(BS_ERROR, "Failed to create client layer surface.");
        _wlmdock_destroy(dock_ptr);
        return NULL;
    }

    // Configure.
    zwlr_layer_surface_v1_set_size(
        wlmcl_layer_surface_wlr_layer_surface(dock_ptr->layer_surface_ptr),
        64, 64);
    zwlr_layer_surface_v1_set_anchor(
        wlmcl_layer_surface_wlr_layer_surface(dock_ptr->layer_surface_ptr),
        positioning.anchor | positioning.edge);
    zwlr_layer_surface_v1_set_exclusive_zone(
        wlmcl_layer_surface_wlr_layer_surface(dock_ptr->layer_surface_ptr),
        64);
    zwlr_layer_surface_v1_set_exclusive_edge(
        wlmcl_layer_surface_wlr_layer_surface(dock_ptr->layer_surface_ptr),
        positioning.edge);
    wl_surface_commit(
        wlmcl_layer_surface_wl_surface(dock_ptr->layer_surface_ptr));

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
    wlmtk_box_orientation_t orientation = WLMTK_BOX_VERTICAL;
    if (positioning.edge == WLR_EDGE_TOP ||
        positioning.edge == WLR_EDGE_BOTTOM) {
        orientation = WLMTK_BOX_HORIZONTAL;
    }
    dock_ptr->tilebox_ptr = wlmdock_tilebox_create(
        orientation,
        (positioning.anchor == WLR_EDGE_BOTTOM ||
         positioning.anchor == WLR_EDGE_RIGHT),
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
/** Decoder for the "Launchers" array. Currently just stores a reference. */
bool _wlmdock_decode_launchers(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    void *value_ptr)
{
    bspl_array_t **array_ptr_ptr = value_ptr;

    *array_ptr_ptr = bspl_array_from_object(object_ptr);
    if (NULL == *array_ptr_ptr) return false;

    bspl_object_ref(bspl_object_from_array(*array_ptr_ptr));
    return true;
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
            bs_log(BS_INFO, "wlmdock: Signal %d caught, exiting loop.",
                   siginfo.ssi_signo);
            wl_display_terminate(dock_ptr->local_display_ptr);
        }
    }
    return 0;
}

/* == End of wlmdock.c ===================================================== */
