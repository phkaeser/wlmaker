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

#include "clip.h"
#include "config.h"
#include "dock.h"
#include "server.h"
#include "task_list.h"

#include "default_style.h"

/** Will hold the value of --config_file. */
static char *wlmaker_arg_config_file_ptr = NULL;

/** Definition of commandline arguments. */
static const bs_arg_t wlmaker_args[] = {
    BS_ARG_STRING(
        "config_file",
        "Optional: Path to a configuration file. If not provided, wlmaker "
        "will scan default paths for a configuration file, or fall back to "
        "a built-in configuration.",
        NULL,
        &wlmaker_arg_config_file_ptr),
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

/* ------------------------------------------------------------------------- */
/** Quits the server. */
void handle_quit(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wl_display_terminate(server_ptr->wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Invokes a locking program. */
void lock(__UNUSED__ wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    if (NULL != server_ptr->idle_monitor_ptr) {
        wlmaker_idle_monitor_lock(server_ptr->idle_monitor_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Creates a new terminal. */
void new_terminal(__UNUSED__ wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    if (0 == fork()) {
        execl("/bin/sh", "/bin/sh", "-c", "/usr/bin/foot", (void *)NULL);
    }
}

/* ------------------------------------------------------------------------- */
/** Switches to previous workspace. */
void prev_workspace(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_server_switch_to_previous_workspace(server_ptr);
}

/* ------------------------------------------------------------------------- */
/** Switches to next workspace. */
void next_workspace(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_server_switch_to_next_workspace(server_ptr);
}

/* ------------------------------------------------------------------------- */
/** Lowers the currently-active view, if any. */
void lower_view(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_view_t *view_ptr = wlmaker_workspace_get_activated_view(
        server_ptr->current_workspace_ptr);
    wlmaker_workspace_lower_view(server_ptr->current_workspace_ptr, view_ptr);
}

/* ------------------------------------------------------------------------- */
/** Raises the currently-active view, if any. */
void raise_view(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_view_t *view_ptr = wlmaker_workspace_get_activated_view(
        server_ptr->current_workspace_ptr);
    wlmaker_workspace_raise_view(server_ptr->current_workspace_ptr, view_ptr);
}

/* ------------------------------------------------------------------------- */
/** Toggles fullscreen view for the activated view. */
void toggle_fullscreen(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        server_ptr);

    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        workspace_ptr);
    if (NULL != wlmtk_workspace_ptr) {

        wlmtk_window_t *window_ptr = wlmtk_workspace_get_activated_window(
            wlmtk_workspace_ptr);
        if (NULL == window_ptr) return;
        wlmtk_window_request_fullscreen(
            window_ptr, !wlmtk_window_is_fullscreen(window_ptr));

    } else {
        wlmaker_view_t *view_ptr = wlmaker_workspace_get_activated_view(
            workspace_ptr);
        if (NULL == view_ptr) return;  // No activated view, nothing to do.
        wlmaker_view_set_fullscreen(view_ptr, !view_ptr->fullscreen);
    }
}

/* ------------------------------------------------------------------------- */
/** Toggles maximization for the activated view. */
void toggle_maximize(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        server_ptr);

    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        workspace_ptr);
    if (NULL != wlmtk_workspace_ptr) {

        wlmtk_window_t *window_ptr = wlmtk_workspace_get_activated_window(
            wlmtk_workspace_ptr);
        if (NULL == window_ptr) return;
        wlmtk_window_request_maximized(
            window_ptr, !wlmtk_window_is_maximized(window_ptr));

    } else {
        wlmaker_view_t *view_ptr = wlmaker_workspace_get_activated_view(
            workspace_ptr);
        if (NULL == view_ptr) return;  // No activated view, nothing to do.
        wlmaker_view_set_maximized(view_ptr, !view_ptr->maximized);
    }
}

bool cb(const wlmaker_keybinding_t *binding_ptr)
{
    bs_log(BS_ERROR, "FIXME called %p", binding_ptr);
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

    if (!bs_arg_parse(wlmaker_args, BS_ARG_MODE_NO_EXTRA, &argc, argv)) {
        fprintf(stderr, "Failed to parse commandline arguments.\n");
        return EXIT_FAILURE;
    }
    wlmcfg_dict_t *config_dict_ptr = wlmaker_config_load(
        wlmaker_arg_config_file_ptr);
    if (NULL != wlmaker_arg_config_file_ptr) free(wlmaker_arg_config_file_ptr);
    if (NULL == config_dict_ptr) {
        fprintf(stderr, "Failed to load & initialize configuration.\n");
        return EXIT_FAILURE;
    }

    // TODO: Should be loaded from file, if given in the config. Or on the
    // commandline. And, Maybe store this in server?
    wlmaker_config_style_t style = {};
    wlmcfg_dict_t *style_dict_ptr = wlmcfg_dict_from_object(
        wlmcfg_create_object_from_plist_data(
            embedded_binary_default_style_data,
            embedded_binary_default_style_size));
    BS_ASSERT(wlmcfg_decode_dict(
                  style_dict_ptr,
                  wlmaker_config_style_desc, &style));

    BS_ASSERT(bs_ptr_stack_init(&wlmaker_subprocess_stack));

    wlmaker_server_t *server_ptr = wlmaker_server_create(config_dict_ptr);
    wlmcfg_dict_unref(config_dict_ptr);
    if (NULL == server_ptr) return EXIT_FAILURE;

    wlmaker_keybinding_t binding = {
        .modifiers = WLR_MODIFIER_CTRL,
        .keysym = XKB_KEY_p,
        .ignore_case = true
    };
    wlmaker_server_keyboard_bind(server_ptr, &binding, cb);

    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Q,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        handle_quit,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_L,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        lock,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_T,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        new_terminal,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Left,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        prev_workspace,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Right,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        next_workspace,
        NULL);

    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Up,
        WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        raise_view,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Down,
        WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        lower_view,
        NULL);

    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_F,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        toggle_fullscreen,
        NULL);
    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_M,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        toggle_maximize,
        NULL);

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

        dock_ptr = wlmaker_dock_create(server_ptr, &style);
        clip_ptr = wlmaker_clip_create(server_ptr, &style);
        task_list_ptr = wlmaker_task_list_create(server_ptr);
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
