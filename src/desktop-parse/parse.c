/* ========================================================================= */
/**
 * @file parse.c
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

#include "parse.h"

#include <errno.h>
#include <locale.h>
#include <regex.h>
#include <string.h>

/* == Declarations ========================================================= */

/* == Data ================================================================= */

static const char *desktop_entry_group_name = "Desktop Entry";

// -> overall: initialize parser
// -> build regexp for key with optional language modifier
// -> build locale search string(s) with priority. Split, use strtok.
//    (C/NULL == none, ...)
// -> then, build it.

// https://wiki.archlinux.org/title/Locale - l_terr.codeset@modifier
// (man setlocale)
//
// lang: two- or 3 letter codes [a-z]{2,3}
// (https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes)
//
// country (territory): [A-Z]{2}, [A-Z][3], [0-9]{3}
// (https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes)
//
// encoding (codeset): (eg. UTF-8)  [a-zA-Z0-9-]+ ?
//
// modifier (??) - [a-zA-Z0-9_-]+
//
//
// Order:
// - if LC_MESSAGES is l_C.E@m => l_C@m / l_C / l@m / l
// - if LC_MESSAGES is l_C.E => l_C / l
//
// locale.h >= setlocale(LC_MESSAGES, NULL)
//
static const char *key_regex_str =
    "^"
    "[A-Za-z0-9-]+"
    "(\\[[a-z]{2,3}[a-zA-Z0-9_@\\.-]*\\])?"
    "$";

struct desktop_parser {
    regex_t                   key_regex;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct desktop_parser *desktop_parse_create(void)
{
    struct desktop_parser *parser = calloc(1, sizeof(struct desktop_parser));
    if (NULL == parser) return NULL;

    if (0 != regcomp(&parser->key_regex, key_regex_str, REG_EXTENDED)) {
        bs_log(BS_ERROR, "FIXME: Failed regcomp");
        desktop_parse_destroy(parser);
        errno = EINVAL;
        return 0;
    }

    char *locale_ptr = setlocale(LC_MESSAGES, NULL);
    if (NULL != locale_ptr && 0 != strcmp("C", locale_ptr)) {
        // Splits the locale, see setlocale(3). The locale name is of the form
        // language[_territory][.codeset][@modifier].
        char *copied_ptr = strdup(locale_ptr);
        if (NULL == copied_ptr) {
            desktop_parse_destroy(parser);
            errno = ENOMEM;
            return NULL;
        }

        strtok(copied_ptr, "@");
        char *modifier_ptr = strtok(NULL, "@");
        strtok(copied_ptr, ".");
        char *codeset_ptr = strtok(NULL, ".");
        char *language_ptr = strtok(copied_ptr, "_");
        char *territory_ptr = strtok(NULL, "_");

        // Now, build lookup priorities:
        // highest: l_c@m
        // next: l_c
        // then: l@m
        // then: l
        bs_log(BS_ERROR, "FIXME: %s - %s - %s - %s",
               language_ptr, territory_ptr, codeset_ptr, modifier_ptr);

        free(copied_ptr);
    }


    return parser;
}

/* ------------------------------------------------------------------------- */
void desktop_parse_destroy(struct desktop_parser *parser)

{
    regfree(&parser->key_regex);
    free(parser);
}

/* ------------------------------------------------------------------------- */
int handle_desktop_file(
    void *userdata_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr)
{
    //struct desktop_entry *entry_ptr = userdata_ptr;
    struct desktop_parser *parser = userdata_ptr;

    // Skip groups other than the main entry.
    if (0 != strcmp(section_ptr, desktop_entry_group_name)) return 1;

    // key names: A-Za-z0-9-  [locale]

    regmatch_t matches[4];
    if (0 != regexec(&parser->key_regex, name_ptr, 4, &matches[0], 0)) {
        bs_log(BS_ERROR, "FIXME: no match for %s", name_ptr);
        return 0;
    } else {
        bs_log(BS_WARNING, "FIXME: match for %s", name_ptr);
    }

    name_ptr = name_ptr;
    value_ptr = value_ptr;

    // FIXME
    return 1;
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == Unit tests =========================================================== */

static void _parse_test_key(bs_test_t *test_ptr);

/** Test cases for action items. */
static const bs_test_case_t   parse_test_cases[] = {
    { true, "key", _parse_test_key },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t           parse_test_set = BS_TEST_SET(
    true, "parse", parse_test_cases);

void _parse_test_key(bs_test_t *test_ptr)
{
    bs_log(BS_ERROR, "FIXME: locale %s", setlocale(LC_MESSAGES, "en_US.UTF-8@euro")); //_US.UTF-8@euro"));
    bs_log(BS_ERROR, "FIXME: locale %s", setlocale(LC_MESSAGES, NULL));

    struct desktop_parser *p = desktop_parse_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, p);

    const char *gn = desktop_entry_group_name;  // For convenience.
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de_DE]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de@d]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de_DE.UTF-8]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de@euro]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de_DE@euro]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de.UTF-8@euro]", "V"));
    BS_TEST_VERIFY_NEQ(test_ptr, 0, handle_desktop_file(p, gn, "K[de_DE.UTF-8@euro]", "V"));

    desktop_parse_destroy(p);
}

/* == End of parse.c ======================================================= */
