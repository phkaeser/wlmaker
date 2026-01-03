/* ========================================================================= */
/**
 * @file gen_menu.c
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

#include "gen_menu.h"

#include <desktop-parser/desktop-parser.h>
#include <locale.h>
#include <sys/stat.h>

/* == Declarations ========================================================= */

/* == Data ================================================================= */

bspl_array_t *array_from_entry(const struct desktop_entry *entry_ptr)
{
    bspl_array_t *array_ptr = bspl_array_create();
    if (NULL == array_ptr) return NULL;

    bspl_string_t *n = bspl_string_create(entry_ptr->name_ptr);
    bspl_string_t *a = bspl_string_create("ShellExecute");
    bspl_string_t *e = bspl_string_create(entry_ptr->exec_ptr);

    if (!bspl_array_push_back(array_ptr, bspl_object_from_string(n)) ||
        !bspl_array_push_back(array_ptr, bspl_object_from_string(a)) ||
        !bspl_array_push_back(array_ptr, bspl_object_from_string(e)) ||
        3 != bspl_array_size(array_ptr)) {
        // Failed to build array. Zap it, return error.
        bspl_array_unref(array_ptr);
        array_ptr = NULL;
    }

    bspl_string_unref(e);
    bspl_string_unref(a);
    bspl_string_unref(n);
    return array_ptr;
}

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bspl_array_t *wlmaker_menu_generate(const char *path_ptr)
{
    struct stat statbuf;
    if (0 != stat(path_ptr, &statbuf)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed stat(\"%s\", %p)",
               path_ptr, &statbuf);
        return NULL;
    }

    struct desktop_parser *parser = desktop_parser_create(
        setlocale(LC_MESSAGES, NULL));
    if (NULL == parser) {
        bs_log(BS_ERROR, "Failed desktop_parser_create(\"%s\")",
               setlocale(LC_MESSAGES, NULL));
        return NULL;
    }

    bspl_array_t *array_ptr = NULL;
    if (S_ISREG(statbuf.st_mode)) {
        struct desktop_entry entry = {};
        int rv = desktop_parser_file_to_entry(parser, path_ptr, &entry);
        if (0 != rv) {
            bs_log(BS_ERROR, "Failed desktop_parser_file_to_entry"
                   "(%p, \"%s\", %p) at line %d",
                   parser, path_ptr, &entry, rv);
        } else {
            array_ptr = array_from_entry(&entry);
        }
        desktop_parser_entry_release(&entry);
    } else if (S_ISDIR(statbuf.st_mode)) {
        // FIXME: Walk the file tree.
    } else {
        bs_log(BS_ERROR, "Not a file nor directory: \"%s\"", path_ptr);
    }

    desktop_parser_destroy(parser);
    return array_ptr;
}


/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == End of gen_menu.c ==================================================== */
