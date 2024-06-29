/* ========================================================================= */
/**
 * @file wlmaker.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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

/// setenv() is a POSIX extension.
#define _POSIX_C_SOURCE 200112L

#include <libbase/libbase.h>
#include <wlr/util/log.h>

#include <limits.h>
#include <regex.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "conf/plist.h"

#include "action.h"
#include "clip.h"
#include "config.h"
#include "dock.h"
#include "server.h"
#include "task_list.h"

#include "style_default.h"

/** Will hold the value of --config_file. */
static char *wlmaker_arg_config_file_ptr = NULL;
/** Will hold the value of --style_file. */
static char *wlmaker_arg_style_file_ptr = NULL;

/** Startup options for the server. */
static wlmaker_server_options_t wlmaker_server_options = {};

/** Definition of commandline arguments. */
static const bs_arg_t wlmaker_args[] = {
#if defined(WLMAKER_HAVE_XWAYLAND)
    BS_ARG_BOOL(
        "start_xwayland",
        "Optional: Whether to start XWayland. Disabled by default.",
        false,
        &wlmaker_server_options.start_xwayland),
#endif  // defined(WLMAKER_HAVE_XWAYLAND)
    BS_ARG_STRING(
        "config_file",
        "Optional: Path to a configuration file. If not provided, wlmaker "
        "will scan default paths for a configuration file, or fall back to "
        "a built-in configuration.",
        NULL,
        &wlmaker_arg_config_file_ptr),
    BS_ARG_STRING(
        "style_file",
        "Optional: Path to a style (\"theme\") file. If not provided, wlmaker "
        "will use a built-in default style.",
        NULL,
        &wlmaker_arg_style_file_ptr),
    BS_ARG_SENTINEL()
};

/** References auto-started subprocesses. */
static bs_ptr_stack_t         wlmaker_subprocess_stack;

/** Compiled regular expression for extracting file & line no. from wlr_log. */
static regex_t                wlmaker_wlr_log_regex;
/** Regular expression string for extracting file & line no. from wlr_log. */
static const char             *wlmaker_wlr_log_regex_string =
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
    if (0 != regexec(&wlmaker_wlr_log_regex, buf, 4, &matches[0], 0) ||
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

/* ------------------------------------------------------------------------- */
/** Launches a sub-process, and keeps it on the subprocess stack. */
bool start_subprocess(const char *cmdline_ptr)
{
    bs_subprocess_t *sp_ptr = bs_subprocess_create_cmdline(cmdline_ptr);
    if (NULL == sp_ptr) {
        bs_log(BS_ERROR, "Failed bs_subprocess_create_cmdline(\"%s\")",
               cmdline_ptr);
        return false;
    }
    if (!bs_subprocess_start(sp_ptr)) {
        bs_log(BS_ERROR, "Failed bs_subprocess_start for \"%s\".",
               cmdline_ptr);
        return false;
    }
    // Push to stack. Ignore errors: We'll keep it running untracked.
    bs_ptr_stack_push(&wlmaker_subprocess_stack, sp_ptr);
    return true;
}

/* == Main program ========================================================= */
/** The main program. */
int main(__UNUSED__ int argc, __UNUSED__ const char **argv)
{
    wlmaker_dock_t            *dock_ptr = NULL;
    wlmaker_clip_t            *clip_ptr = NULL;
    wlmaker_task_list_t       *task_list_ptr = NULL;
    int                       rv = EXIT_SUCCESS;

    rv = regcomp(
        &wlmaker_wlr_log_regex,
        wlmaker_wlr_log_regex_string,
        REG_EXTENDED);
    if (0 != rv) {
        char err_buf[512];
        regerror(rv, &wlmaker_wlr_log_regex, err_buf, sizeof(err_buf));
        bs_log(BS_ERROR, "Failed compiling regular expression: %s", err_buf);
        return EXIT_FAILURE;
    }

    wlr_log_init(WLR_DEBUG, wlr_to_bs_log);
    bs_log_severity = BS_INFO;

    BS_ASSERT(bs_ptr_stack_init(&wlmaker_subprocess_stack));

    if (!bs_arg_parse(wlmaker_args, BS_ARG_MODE_NO_EXTRA, &argc, argv)) {
        fprintf(stderr, "Failed to parse commandline arguments.\n");
        bs_arg_print_usage(stderr, wlmaker_args);
        return EXIT_FAILURE;
    }
    wlmcfg_dict_t *config_dict_ptr = wlmaker_config_load(
        wlmaker_arg_config_file_ptr);
    if (NULL != wlmaker_arg_config_file_ptr) free(wlmaker_arg_config_file_ptr);
    if (NULL == config_dict_ptr) {
        fprintf(stderr, "Failed to load & initialize configuration.\n");
        return EXIT_FAILURE;
    }

    wlmaker_server_t *server_ptr = wlmaker_server_create(
        config_dict_ptr, &wlmaker_server_options);
    wlmcfg_dict_unref(config_dict_ptr);
    if (NULL == server_ptr) return EXIT_FAILURE;

    // TODO: Should be loaded from file, if given in the config. Or on the
    // commandline.
    wlmcfg_dict_t *style_dict_ptr = NULL;
    if (NULL != wlmaker_arg_style_file_ptr) {
        style_dict_ptr = wlmcfg_dict_from_object(
            wlmcfg_create_object_from_plist_file(
                wlmaker_arg_style_file_ptr));
    } else {
        style_dict_ptr = wlmcfg_dict_from_object(
            wlmcfg_create_object_from_plist_data(
                embedded_binary_style_default_data,
                embedded_binary_style_default_size));
    }
    if (NULL == style_dict_ptr) return EXIT_FAILURE;
    BS_ASSERT(wlmcfg_decode_dict(
                  style_dict_ptr,
                  wlmaker_config_style_desc,
                  &server_ptr->style));
    wlmcfg_dict_unref(style_dict_ptr);

    wlmaker_action_handle_t *action_handle_ptr = wlmaker_action_bind_keys(
        server_ptr,
        wlmcfg_dict_get_dict(config_dict_ptr, wlmaker_action_config_dict_key));
    if (NULL == action_handle_ptr) {
        bs_log(BS_ERROR, "Failed to bind keys.");
        return EXIT_FAILURE;
    }

    rv = EXIT_SUCCESS;
    if (wlr_backend_start(server_ptr->wlr_backend_ptr)) {
        bs_log(BS_INFO, "Starting Wayland compositor for server %p at %s ...",
               server_ptr, server_ptr->wl_socket_name_ptr);

        setenv("WAYLAND_DISPLAY", server_ptr->wl_socket_name_ptr, true);

        wlmcfg_array_t *autostarted_ptr = wlmcfg_dict_get_array(
            config_dict_ptr, "Autostart");
        if (NULL != autostarted_ptr) {
            for (size_t i = 0; i < wlmcfg_array_size(autostarted_ptr); ++i) {
                const char *cmd_ptr = wlmcfg_array_string_value_at(
                    autostarted_ptr, i);
                if (!start_subprocess(cmd_ptr)) return EXIT_FAILURE;
            }
        }

        dock_ptr = wlmaker_dock_create(server_ptr, &server_ptr->style);
        clip_ptr = wlmaker_clip_create(server_ptr, &server_ptr->style);
        task_list_ptr = wlmaker_task_list_create(server_ptr, &server_ptr->style);
        if (NULL == dock_ptr || NULL == clip_ptr || NULL == task_list_ptr) {
            bs_log(BS_ERROR, "Failed to create dock, clip or task list.");
        } else {
            wl_display_run(server_ptr->wl_display_ptr);
        }

    } else {
        bs_log(BS_ERROR, "Failed wlmaker_server_start()");
        rv = EXIT_FAILURE;
    }

    if (NULL != task_list_ptr) wlmaker_task_list_destroy(task_list_ptr);
    if (NULL != clip_ptr) wlmaker_clip_destroy(clip_ptr);
    if (NULL != dock_ptr) wlmaker_dock_destroy(dock_ptr);
    wlmaker_action_unbind_keys(action_handle_ptr);
    wlmaker_server_destroy(server_ptr);

    bs_subprocess_t *sp_ptr;
    while (NULL != (sp_ptr = bs_ptr_stack_pop(&wlmaker_subprocess_stack))) {
        bs_subprocess_destroy(sp_ptr);
    }
    bs_ptr_stack_fini(&wlmaker_subprocess_stack);

    regfree(&wlmaker_wlr_log_regex);
    return rv;
}

/* == End of wlmaker.c ===================================================== */
