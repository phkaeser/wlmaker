/* ========================================================================= */
/**
 * @file wlmtool.c
 *
 * Command-line tool for Wayland Maker.
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen_menu.h"

/* == Declarations ========================================================= */

static bool print_version(int argc, const char **argv);
static bool print_help(int argc, const char **argv);
static bool generate_menu(int argc, const char **argv);

#if !defined(WLMAKER_VERSION_MAJOR) || !defined(WLMAKER_VERSION_MINOR) || !defined(WLMAKER_VERSION_FULL)
#eror "WLMAKER_VERSION_... not defined!"
#else
static const char *wlmaker_version_major = WLMAKER_VERSION_MAJOR;
static const char *wlmaker_version_minor = WLMAKER_VERSION_MINOR;
static const char *wlmaker_version_full = WLMAKER_VERSION_FULL;
#endif

/** Command descriptor. */
struct command_desc {
    /** Command that can be invoked. */
    const char *command_ptr;
    /** Description of the command. */
    const char *description_ptr;
    /** The operation executed by the command. */
    bool (*op)(int argc, const char **argv);
};

/** Locale, when specified as `--locale` argument to the commandline. */
static char *wlmtool_locale_ptr = NULL;

/* == Data ================================================================= */

/** Definition of commandline arguments. */
static const bs_arg_t wlmtool_args[] = {
    BS_ARG_STRING(
        "locale",
        "Optional: Override the locale for generating the menu. Uses the "
        "environment's setting for LC_MESSAGES, if not set.",
        NULL,
        &wlmtool_locale_ptr),
    BS_ARG_SENTINEL()
};

/** List of available commands. */
static const struct command_desc commands[] = {
    {
        .command_ptr = "genmenu",
        .description_ptr =
        "Generates a root menu for Wayland Maker, in .plist text format.",
        .op = generate_menu
    },
    {
        .command_ptr = "--help",
        .description_ptr = "Prints usage information.",
        .op = print_help
    },
    {
        .command_ptr = "--version",
        .description_ptr = "Prints version information.",
        .op = print_version
    },
    {}  // Sentinel.
};

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Prints version information. */
bool print_version(__UNUSED__ int argc, __UNUSED__ const char **argv)
{
    fprintf(stdout, "Wayland Maker wlmtool version %s.%s (%s)\n",
            wlmaker_version_major,
            wlmaker_version_minor,
            wlmaker_version_full);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Prints help. */
bool print_help(__UNUSED__ int argc, __UNUSED__ const char **argv)
{
    fprintf(stdout, "Wayland Maker wlmtool version %s.%s (%s)\n",
            wlmaker_version_major,
            wlmaker_version_minor,
            wlmaker_version_full);
    fprintf(stderr, "\nAvailable commands and options:\n");
    for (const struct command_desc *d = commands; d->command_ptr; ++d) {
        fprintf(stdout, "%s: %s\n", d->command_ptr, d->description_ptr);
    }

    bs_arg_print_usage(stdout, wlmtool_args);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Generates the plist menu. */
bool generate_menu(int argc, const char **argv)
{
    const char *path_ptr = NULL;
    if (1 > argc) {
        fprintf(stderr, "Usage: wlmtool genmenu [PATH]\n");
        return false;
    } else if (2 <= argc) {
        path_ptr = argv[1];
    }

    bspl_array_t *menu_array_ptr = wlmaker_menu_generate(
        wlmtool_locale_ptr, path_ptr);
    if (NULL == menu_array_ptr) return false;

    bs_dynbuf_t buf = {};
    bool rv = bs_dynbuf_init(&buf, 1024, SIZE_MAX);
    if (rv) {
        rv = bspl_object_write(bspl_object_from_array(menu_array_ptr), &buf);
        if (rv) fprintf(stdout, "%.*s", (int)buf.length, (char*)buf.data_ptr);
    }
    bspl_array_unref(menu_array_ptr);
    bs_dynbuf_fini(&buf);
    return rv;
}

/* == Main program ========================================================= */
/** The main program. */
int main(int argc, const char **argv)
{
    if (!bs_arg_parse(wlmtool_args, BS_ARG_MODE_EXTRA_ARGS, &argc, argv)) {
        fprintf(stderr, "Failed to parse commandline arguments.\n");
        bs_arg_print_usage(stderr, wlmtool_args);
        return EXIT_FAILURE;
    }

    if (1 < argc) {
        for (const struct command_desc *d = commands; d->command_ptr; ++d) {
            if (0 == strcmp(argv[1], d->command_ptr)) {
                return d->op(argc - 1, argv + 1) ? EXIT_SUCCESS : EXIT_FAILURE;
            }
        }
        fprintf(stderr, "Unknown command: %s.\n", argv[1]);
    } else {
        fprintf(stderr, "Missing command.\n");
    }
    fprintf(stderr, "\nAvailable commands:\n");
    for (const struct command_desc *d = commands; d->command_ptr; ++d) {
        fprintf(stderr, "%s: %s\n", d->command_ptr, d->description_ptr);
    }
    bs_arg_print_usage(stderr, wlmtool_args);
    return EXIT_FAILURE;
}

/* == End of wlmtool.c ===================================================== */
