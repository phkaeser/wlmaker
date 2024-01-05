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

#include "clip.h"
#include "dock.h"
#include "server.h"
#include "task_list.h"

/** Set of commands to be executed on startup. */
static const char *autostarted_commands[] = {
    "/usr/bin/foot",
    NULL  // sentinel.
};

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
/** Quits the server. */
void handle_quit(wlmaker_server_t *server_ptr, __UNUSED__ void *arg_ptr)
{
    wl_display_terminate(server_ptr->wl_display_ptr);
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
    wlmaker_view_t *view_ptr = wlmaker_workspace_get_activated_view(
        workspace_ptr);
    if (NULL == view_ptr) return;  // No activated view, nothing to do.
    wlmaker_view_set_maximized(view_ptr, !view_ptr->maximized);
}

/* == Main program ========================================================= */
/** The main program. */
int main(__UNUSED__ int argc, __UNUSED__ char *argv[])
{
    wlmaker_dock_t            *dock_ptr = NULL;
    wlmaker_clip_t            *clip_ptr = NULL;
    wlmaker_task_list_t       *task_list_ptr = NULL;
    bs_ptr_stack_t            subprocess_stack;
    bs_subprocess_t           *subprocess_ptr;
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

    BS_ASSERT(bs_ptr_stack_init(&subprocess_stack));

    wlmaker_server_t *server_ptr = wlmaker_server_create();
    if (NULL == server_ptr) return EXIT_FAILURE;

    wlmaker_server_bind_key(
        server_ptr,
        XKB_KEY_Q,
        WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO,
        handle_quit,
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

        for (const char **cmd_ptr = &autostarted_commands[0];
             NULL != *cmd_ptr;
             ++cmd_ptr) {
            subprocess_ptr = bs_subprocess_create_cmdline(*cmd_ptr);
            if (NULL == subprocess_ptr) {
                bs_log(BS_ERROR, "Failed bs_subprocess_create_cmdline(\"%s\")",
                       *cmd_ptr);
                return EXIT_FAILURE;
            }
            if (!bs_subprocess_start(subprocess_ptr)) {
                bs_log(BS_ERROR, "Failed bs_subprocess_start for \"%s\".",
                       *cmd_ptr);
                return EXIT_FAILURE;
            }
            bs_ptr_stack_push(&subprocess_stack, subprocess_ptr);
        }

        dock_ptr = wlmaker_dock_create(server_ptr);
        clip_ptr = wlmaker_clip_create(server_ptr);
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

    while (NULL != (subprocess_ptr = bs_ptr_stack_pop(&subprocess_stack))) {
        bs_subprocess_destroy(subprocess_ptr);
    }

    bs_ptr_stack_fini(&subprocess_stack);
    regfree(&wlmaker_wlr_log_regex);
    return rv;
}

/* == End of wlmaker.c ===================================================== */
