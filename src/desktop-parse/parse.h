/* ========================================================================= */
/**
 * @file parse.h
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
#ifndef __WLMAKER_PARSE_H__
#define __WLMAKER_PARSE_H__

#include <stdbool.h>
#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct desktop_parser;

/** Permissible values for `Type=...`. */
enum desktop_entry_type {
    DESKTOP_ENTRY_TYPE_UNKONN = 0,
    DESKTOP_ENTRY_TYPE_APPLICATION = 1,
    DESKTOP_ENTRY_TYPE_LINK = 2,
    DESKTOP_ENTRY_TYPE_DIRECTORY = 3,
};

struct desktop_entry {
    enum desktop_entry_type   type;

    int8_t name_priority;

    char *name_ptr;
    char *exec_ptr;
    char *categories;
};

/**
 * Creates a desktop parser, using the provided locale.
 *
 * @param locale_ptr          Locale set for `LC_MESSAGES`. See setlocale(3).
 *
 * @return Pointer to the desktop parser, or NULL on error. Must be destroyed
 *     by calling @ref desktop_parse_destroy.
 */
struct desktop_parser *desktop_parser_create(const char *locale_ptr);

/**
 * Destroys the desktop parser.
 *
 * @param parser
 */
void desktop_parser_destroy(struct desktop_parser *parser);

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

#endif  // __WLMAKER_PARSE_H__
/* == End of parse.h ======================================================= */
