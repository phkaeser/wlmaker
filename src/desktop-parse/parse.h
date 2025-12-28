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

/** Permissible values for `Type=...`. */
enum desktop_entry_type {
    DESKTOP_ENTRY_TYPE_UNKONN = 0,
    DESKTOP_ENTRY_TYPE_APPLICATION = 1,
    DESKTOP_ENTRY_TYPE_LINK = 2,
    DESKTOP_ENTRY_TYPE_DIRECTORY = 3,
};

struct desktop_entry {
    enum desktop_entry_type   type;

    char *name_ptr;
    char *exec_ptr;
    char *categories;

};

struct desktop_parser;

struct desktop_parser *desktop_parse_create(void);
void desktop_parse_destroy(struct desktop_parser *parser_ptr);

int handle_desktop_file(
    void *userdata_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr);

/** Unit test set. */
extern const bs_test_set_t    parse_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_PARSE_H__
/* == End of parse.h ======================================================= */
