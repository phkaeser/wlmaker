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
#include "background.h"
#include "clip.h"
#include "config.h"
#include "dock.h"
#include "server.h"
#include "task_list.h"

#include "../etc/style.h"
#include "../etc/root_menu.h"

/** Will hold the value of --config_file. */
static char *wlmaker_arg_config_file_ptr = NULL;
/** Will hold the value of --state_file. */
static char *wlmaker_arg_state_file_ptr = NULL;
/** Will hold the value of --style_file. */
static char *wlmaker_arg_style_file_ptr = NULL;
/** Will hold the value of --root_menu_file. */
static char *wlmaker_arg_root_menu_file_ptr = NULL;

/** Startup options for the server. */
static wlmaker_server_options_t wlmaker_server_options = {
    .start_xwayland = false,
    .output = { .width = 0, .height = 0 }
    ,
};

/** Log levels. */
static const bs_arg_enum_table_t wlmaker_log_levels[] = {
    { .name_ptr = "DEBUG", BS_DEBUG },
    { .name_ptr = "INFO", BS_INFO },
    { .name_ptr = "WARNING", BS_WARNING },
    { .name_ptr = "ERROR", BS_ERROR },
    { .name_ptr = NULL },
};

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
        "state_file",
        "Optional: Path to a state file, with state of workspaces, dock and "
        "clips configured. If not provided, wlmaker will scan default paths "
        "for a state file, or fall back to a built-in default.",
        NULL,
        &wlmaker_arg_state_file_ptr),
    BS_ARG_STRING(
        "style_file",
        "Optional: Path to a style (\"theme\") file. If not provided, wlmaker "
        "will use a built-in default style.",
        NULL,
        &wlmaker_arg_style_file_ptr),
    BS_ARG_STRING(
        "root_menu_file",
        "Optional: Path to a file describing the root menu. If not provided, "
        "wlmaker will use a built-in definition for the root menu.",
        NULL,
        &wlmaker_arg_root_menu_file_ptr),
    BS_ARG_ENUM(
        "log_level",
        "Log level to apply. One of DEBUG, INFO, WARNING, ERROR.",
        "INFO",
        &wlmaker_log_levels[0],
        (int*)&bs_log_severity),
    BS_ARG_UINT32(
        "height",
        "Desired output height. Applies when running in windowed mode, and "
        "only if --width is set, too. Set to 0 for using the output's "
        "preferred dimensions.",
        0, 0, UINT32_MAX,
        &wlmaker_server_options.output.height),
    BS_ARG_UINT32(
        "width",
        "Desired output width. Applies when running in windowed mode, and "
        "only if --height is set, too. Set to 0 for using the output's "
        "preferred dimensions.",
        0, 0, UINT32_MAX,
        &wlmaker_server_options.output.width),
    BS_ARG_SENTINEL()
};

/** References auto-started subprocesses. */
static bs_ptr_stack_t         wlmaker_subprocess_stack;
/** References to the created backgrounds. */
static bs_ptr_stack_t         wlmaker_background_stack;

/** Compiled regular expression for extracting file & line no. from wlr_log. */
static regex_t                wlmaker_wlr_log_regex;
/** Regular expression string for extracting file & line no. from wlr_log. */
static const char             *wlmaker_wlr_log_regex_string =
    "^\\[([^\\:]+)\\:([0-9]+)\\]\\ ";

/** Contents of the workspace style. */
typedef struct {
    /** Workspace name. */
    char            name[32];
    /** Background color. */
    uint32_t        color;
} wlmaker_workspace_style_t;

/** Style descriptor for the "Workspace" dict of wlmaker-state.plist. */
static const wlmcfg_desc_t wlmaker_workspace_style_desc[] = {
    WLMCFG_DESC_CHARBUF(
        "Name", true, wlmaker_workspace_style_t, name, 32, NULL),
    WLMCFG_DESC_ARGB32(
        "Color", false, wlmaker_workspace_style_t, color, 0),
    WLMCFG_DESC_SENTINEL()
};

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
        bs_log(BS_ERROR, "Failed bs_subprocess_start for \"%s\"",
               cmdline_ptr);
        return false;
    }
    // Push to stack. Ignore errors: We'll keep it running untracked.
    bs_ptr_stack_push(&wlmaker_subprocess_stack, sp_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Creates workspaces as configured in the state dict. */
bool create_workspaces(
    wlmcfg_dict_t *state_dict_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmcfg_array_t *array_ptr = wlmcfg_dict_get_array(
        state_dict_ptr, "Workspaces");
    if (NULL == array_ptr) return false;

    bool rv = true;
    for (size_t i = 0; i < wlmcfg_array_size(array_ptr); ++i) {
        wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
            wlmcfg_array_at(array_ptr, i));
        if (NULL == dict_ptr) {
            bs_log(BS_ERROR, "Array element in \"Workspaces\" is not a dict");
            rv = false;
            break;
        }

        wlmaker_workspace_style_t s;
        if (!wlmcfg_decode_dict(dict_ptr, wlmaker_workspace_style_desc, &s)) {
            bs_log(BS_ERROR,
                   "Failed to decode dict element %zu in \"Workspace\"",
                   i);
            rv = false;
            break;
        }

        wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
            s.name, &server_ptr->style.tile, server_ptr->env_ptr);
        if (NULL == workspace_ptr) {
            bs_log(BS_ERROR, "Failed wlmtk_workspace_create(\"%s\", %p)",
                   s.name, server_ptr->env_ptr);
            rv = false;
            break;
        }

        if (s.color == 0) {
            s.color = server_ptr->style.background_color;
        }
        wlmaker_background_t *background_ptr = wlmaker_background_create(
            workspace_ptr,
            wlmaker_output_manager_wlr_output_layout(
                server_ptr->output_manager_ptr),
            s.color, server_ptr->env_ptr);
        if (NULL == background_ptr) {
            bs_log(BS_ERROR, "Failed wlmaker_background(%p)",
                   server_ptr->env_ptr);
            rv = false;
            break;
        }
        BS_ASSERT(bs_ptr_stack_push(
                      &wlmaker_background_stack, background_ptr));

        wlmtk_root_add_workspace(server_ptr->root_ptr, workspace_ptr);
    }

    return rv;
}

/** Lookup paths for the root menu config file. */
static const char *_wlmaker_root_menu_fname_ptrs[] = {
    "~/.wlmaker-root-menu.plist",
    "/usr/share/wlmaker/root-menu.plist",
    NULL  // Sentinel.
};

/** Lookup paths for the style config file. */
static const char *_wlmaker_style_fname_ptrs[] = {
    "~/.wlmaker-style.plist",
    "/usr/share/wlmaker/style.plist",
    NULL  // Sentinel.
};

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
    bs_log_severity = BS_INFO;  // Will be overwritten in bs_arg_parse().
    BS_ASSERT(bs_ptr_stack_init(&wlmaker_subprocess_stack));
    BS_ASSERT(bs_ptr_stack_init(&wlmaker_background_stack));

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

    wlmcfg_dict_t *state_dict_ptr = wlmaker_state_load(
        wlmaker_arg_state_file_ptr);
    if (NULL != wlmaker_arg_state_file_ptr) free(wlmaker_arg_state_file_ptr);
    if (NULL == state_dict_ptr) {
        fprintf(stderr, "Failed to load & initialize state.\n");
        return EXIT_FAILURE;
    }

    wlmaker_server_t *server_ptr = wlmaker_server_create(
        config_dict_ptr, &wlmaker_server_options);
    if (NULL == server_ptr) return EXIT_FAILURE;

    wlmcfg_dict_t *style_dict_ptr = wlmcfg_dict_from_object(
        wlmaker_plist_load(
            "style",
            wlmaker_arg_style_file_ptr,
            _wlmaker_style_fname_ptrs,
            embedded_binary_style_data,
            embedded_binary_style_size));
    if (NULL == style_dict_ptr) return EXIT_FAILURE;
    if (!wlmcfg_decode_dict(
            style_dict_ptr,
            wlmaker_config_style_desc,
            &server_ptr->style)) return EXIT_FAILURE;
    wlmcfg_dict_unref(style_dict_ptr);

    server_ptr->root_menu_array_ptr = wlmcfg_array_from_object(
        wlmaker_plist_load(
            "root menu",
            wlmaker_arg_root_menu_file_ptr,
            _wlmaker_root_menu_fname_ptrs,
            embedded_binary_root_menu_data,
            embedded_binary_root_menu_size));
    if (NULL == server_ptr->root_menu_array_ptr) return EXIT_FAILURE;
    // TODO(kaeser@gubbe.ch): Uh, that's ugly...
    server_ptr->root_menu_ptr = wlmaker_root_menu_create(
        server_ptr,
        &server_ptr->style.window,
        &server_ptr->style.menu,
        server_ptr->env_ptr);
    if (NULL == server_ptr->root_menu_ptr) {
        return EXIT_FAILURE;
    }
    wlmtk_menu_set_open(
        wlmaker_root_menu_menu(server_ptr->root_menu_ptr),
        false);

    wlmaker_action_handle_t *action_handle_ptr = wlmaker_action_bind_keys(
        server_ptr,
        wlmcfg_dict_get_dict(config_dict_ptr, wlmaker_action_config_dict_key));
    if (NULL == action_handle_ptr) {
        bs_log(BS_ERROR, "Failed to bind keys.");
        return EXIT_FAILURE;
    }

    if (!create_workspaces(state_dict_ptr, server_ptr)) {
        return EXIT_FAILURE;
    }

    rv = EXIT_SUCCESS;
    if (wlr_backend_start(server_ptr->wlr_backend_ptr)) {

        if (0 >= wlmaker_output_manager_outputs(
                server_ptr->output_manager_ptr)) {
            bs_log(BS_ERROR, "No outputs available!");
            return EXIT_FAILURE;
        }

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

        dock_ptr = wlmaker_dock_create(
            server_ptr, state_dict_ptr, &server_ptr->style);
        clip_ptr = wlmaker_clip_create(
            server_ptr, state_dict_ptr, &server_ptr->style);
        task_list_ptr = wlmaker_task_list_create(
            server_ptr, &server_ptr->style);
        if (NULL == dock_ptr || NULL == clip_ptr || NULL == task_list_ptr) {
            bs_log(BS_ERROR, "Failed to create dock, clip or task list.");
        } else {
            wl_display_run(server_ptr->wl_display_ptr);
        }

    } else {
        bs_log(BS_ERROR, "Failed wlmaker_server_start()");
        rv = EXIT_FAILURE;
    }

    wlmaker_background_t *bg_ptr;
    while (NULL != (bg_ptr = bs_ptr_stack_pop(&wlmaker_background_stack))) {
        wlmaker_background_destroy(bg_ptr);
    }
    bs_ptr_stack_fini(&wlmaker_background_stack);

    if (NULL != task_list_ptr) wlmaker_task_list_destroy(task_list_ptr);
    if (NULL != clip_ptr) wlmaker_clip_destroy(clip_ptr);
    if (NULL != dock_ptr) wlmaker_dock_destroy(dock_ptr);
    wlmaker_action_unbind_keys(action_handle_ptr);
    wlmcfg_array_unref(server_ptr->root_menu_array_ptr);
    wlmaker_server_destroy(server_ptr);

    bs_subprocess_t *sp_ptr;
    while (NULL != (sp_ptr = bs_ptr_stack_pop(&wlmaker_subprocess_stack))) {
        bs_subprocess_destroy(sp_ptr);
    }
    bs_ptr_stack_fini(&wlmaker_subprocess_stack);

    wlmcfg_dict_unref(config_dict_ptr);
    wlmcfg_dict_unref(state_dict_ptr);
    regfree(&wlmaker_wlr_log_regex);
    return rv;
}

/* == End of wlmaker.c ===================================================== */
