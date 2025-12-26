/* ========================================================================= */
/**
 * @file desktop-parser.h
 *
 * Simple parser for FreeDesktop `.desktop` files, to provide application-
 * specific information within a compositor and for constructing application
 * menus. Depends on `libinih-dev`.
 *
 * Reference:
 * * http://specifications.freedesktop.org/desktop-entry/1.5/
 * * https://github.com/benhoyt/inih
 *
 * Currently built to support the necessary keys for building the root menu
 * for Wayland Maker. Specifically, that includes:
 * * [*] Type
 * * [*] NoDisplay
 * * [*] Hidden
 * * [*] Terminal
 * * [*] Exec
 * * [*] Name
 * * [*] Categories
 * * [*] TryExec
 * * [*] Path
 *
 * Further improvements:
 * * Handle the %f, %u, ... specifiers.
 * * Add support for "numeric" type. Though, it's currently unused for .desktop.
 * * Use the "Terminal" flag and construct a command that executes in terminal.
 *
 * @copyright
 * Copyright (c) 2025 Google LLC and Philipp Kaeser
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
#ifndef __WLMAKER_DESKTOP_PARSER_H__
#define __WLMAKER_DESKTOP_PARSER_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct desktop_parser;

/** Permissible values for `Type=...`. */
enum desktop_entry_type {
    DESKTOP_ENTRY_TYPE_UNKNOWN = 0,
    DESKTOP_ENTRY_TYPE_APPLICATION = 1,
    DESKTOP_ENTRY_TYPE_LINK = 2,
    DESKTOP_ENTRY_TYPE_DIRECTORY = 3,
};

/** Holds information for one desktop entry. */
struct desktop_entry {
    /** Type of the desktop entry. */
    enum desktop_entry_type   type;

    /** Whether this desktop entry had been deleted (to be ignored). */
    bool                      hidden;
    /** Whether to exclude this entry from the menus. */
    bool                      no_display;
    /** Whether the program runs in a terminal window. */
    bool                      terminal;

    /** Helper for localized "Name". */
    int8_t                    name_priority;

    /** Localized specific name of the application. */
    char                      *name_ptr;
    /** Program to execute, possibly with arguments. */
    char                      *exec_ptr;
    /** Path to executable, used to determine if the program is installed. */
    char                      *try_exec_ptr;
    /** The working directory to run the program in. */
    char                      *path_ptr;
    /** An array of strings, each indicating a category. NULL-terminated. */
    char                      **category_ptrs;
};

/**
 * Creates a desktop parser, using the provided locale.
 *
 * @param locale_ptr          Locale set for `LC_MESSAGES`. See setlocale(3).
 *
 * @return Pointer to the desktop parser, or NULL on error. Must be destroyed
 *     by calling @ref desktop_parser_destroy.
 */
struct desktop_parser *desktop_parser_create(const char *locale_ptr);

/**
 * Destroys the desktop parser.
 *
 * @param parser
 */
void desktop_parser_destroy(struct desktop_parser *parser);

/**
 * Parses a file into the provided entry.
 *
 * @param parser
 * @param fname_ptr
 * @param entry_ptr
 *
 * @return 0 on success, or the line number where the parser failed.
 */
int desktop_parser_file_to_entry(
    const struct desktop_parser *parser,
    const char *fname_ptr,
    struct desktop_entry *entry_ptr);

/**
 * Parses an in-memory string into the provided entry.
 *
 * @param parser
 * @param string_ptr
 * @param entry_ptr
 *
 * @return 0 on success, or the line number where the parser failed.
 */
int desktop_parser_string_to_entry(
    const struct desktop_parser *parser,
    const char *string_ptr,
    struct desktop_entry *entry_ptr);

/**
 * Releases the resources associated to the entry.
 *
 * @param entry_ptr
 */
void desktop_parser_entry_release(struct desktop_entry *entry_ptr);

/** Unit test set. */
extern const bs_test_set_t    desktop_parser_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_DESKTOP_PARSER_H__
/* == End of desktop-parser.h ============================================== */
