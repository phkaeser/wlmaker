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

#include <ini.h>
#include <locale.h>
#include <regex.h>
#include <string.h>

/* == Declarations ========================================================= */

/** Encapsulates both parser and entry as argument to the inih handler. */
struct _desktop_parser_handler_arg {
    /** Parser. */
    const struct desktop_parser *parser;
    /** The entry to parse into. */
    struct desktop_entry      *entry_ptr;
};

static int _desktop_parser_handler(
    void *userdata_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr);
static char *_create_locale_key(const char *l, const char *t, const char *m);

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
    "([A-Za-z0-9-]+)"
    "(\\[[a-z]{2,3}[a-zA-Z0-9_@\\.-]*\\])?"
    "$";

/** Parser handle. */
struct desktop_parser {
    /** Describes a key entry, and extracts the optional localization key. */
    regex_t                   key_regex;
    /** Lookup keys for localized strings, ordered in decreasing priority. */
    char                      *localization_key[4];
    /** Length of each of the localized strings. */
    size_t                    localization_key_len[4];
};

static bool _desktop_parser_translate_type(
    const char *value_ptr,
    void *dest_ptr,
    int8_t *prio_ptr);

static bool translate_string(
    const char *value_ptr,
    void *dest_ptr,
    int8_t *prio_ptr)
{
    char **str_ptr_ptr = dest_ptr;
    prio_ptr = prio_ptr;

    if (NULL != *str_ptr_ptr) {
        free(*str_ptr_ptr);
    }
    *str_ptr_ptr = strdup(value_ptr);
    return NULL != *str_ptr_ptr;
}

static void destroy_string(void *dest_ptr)
{
    char **str_ptr_ptr = dest_ptr;
    if (NULL == *str_ptr_ptr) return;
    free(*str_ptr_ptr);
    *str_ptr_ptr = NULL;
}

/** Descriptor for a key. */
struct key_descriptor {
    /** The key's name. */
    const char *key;
    /** Length of the key's name. */
    size_t len;

    /**
     * Offset of the value field, relative to the userdata.
     */
    size_t ofs;
    /**
     * Offset of the locale priority indicator. Iff priority_ofs != ofs, the
     * field is treated as localized.
     */
    size_t priority_ofs;

    /** Translator function. */
    bool (*translate)(const char *value_ptr, void *dest_ptr, int8_t *prio_ptr);
    /** Dtor. */
    void (*destroy)(void *dest_ptr);
};

struct key_descriptor keys[] = {
    {
        .key = "Type",
        .len = strlen("Type"),
        .ofs = offsetof(struct desktop_entry, type),
        .destroy = NULL,
        .priority_ofs = offsetof(struct desktop_entry, type),
        .translate = _desktop_parser_translate_type,
    },
    {
        .key = "Name",
        .len = strlen("Name"),
        .ofs = offsetof(struct desktop_entry, name_ptr),
        .translate = translate_string,
        .destroy = destroy_string,
        .priority_ofs = offsetof(struct desktop_entry, name_priority),
    },
    {
        .key = "Exec",
        .len = strlen("Exec"),
        .ofs = offsetof(struct desktop_entry, exec_ptr),
        .priority_ofs = offsetof(struct desktop_entry, exec_ptr),
        .destroy = destroy_string,
        .translate = translate_string
    },
    { .key = NULL }
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct desktop_parser *desktop_parser_create(const char *locale_ptr)
{
    struct desktop_parser *parser = calloc(1, sizeof(struct desktop_parser));
    if (NULL == parser) return NULL;

    if (0 != regcomp(&parser->key_regex, key_regex_str, REG_EXTENDED)) {
        desktop_parser_destroy(parser);
        return NULL;
    }

    if (NULL != locale_ptr && 0 != strcmp("C", locale_ptr)) {
        // Splits the locale, see setlocale(3). The locale name is of the form
        // language[_territory][.codeset][@modifier].
        char *language_ptr = strdup(locale_ptr);
        if (NULL == language_ptr) {
            desktop_parser_destroy(parser);
            return NULL;
        }
        language_ptr = strtok(language_ptr, "@");
        char *modifier_ptr = strtok(NULL, "@");
        language_ptr = strtok(language_ptr, ".");
        strtok(NULL, ".");  // Code set. Not used for desktop file key.
        language_ptr = strtok(language_ptr, "_");
        char *territory_ptr = strtok(NULL, "_");

        // Matching order. See speficication in "Localized values for keys".
        parser->localization_key[3] = _create_locale_key(
            language_ptr, territory_ptr, modifier_ptr);
        parser->localization_key[2] = _create_locale_key(
            language_ptr, territory_ptr, NULL);
        parser->localization_key[1] = _create_locale_key(
            language_ptr, NULL, modifier_ptr);
        parser->localization_key[0] = _create_locale_key(
            language_ptr, NULL, NULL);
        for (int i = 0; i < 4; ++i) {
            char *c = parser->localization_key[i];
            parser->localization_key_len[i] = c ? strlen(c) : 0;
        }
        free(language_ptr);
    }

    return parser;
}

/* ------------------------------------------------------------------------- */
void desktop_parser_destroy(struct desktop_parser *parser)
{
    for (int i = 0; i < 4; ++i) {
        if (NULL != parser->localization_key[i]) {
            free(parser->localization_key[i]);
        }
        parser->localization_key[i] = NULL;
        parser->localization_key_len[i] = 0;
    }

    regfree(&parser->key_regex);
    free(parser);
}

/* ------------------------------------------------------------------------- */
int desktop_parser_string_to_entry(
    const struct desktop_parser *parser,
    const char *string_ptr,
    struct desktop_entry *entry_ptr)
{
    struct _desktop_parser_handler_arg arg = {
        .parser = parser, .entry_ptr = entry_ptr
    };
    return ini_parse_string(string_ptr, _desktop_parser_handler, &arg);
}

/* ------------------------------------------------------------------------- */
void desktop_parser_entry_release(struct desktop_entry *entry_ptr)
{
    for (const struct key_descriptor *key_ptr = &keys[0];
         NULL != key_ptr->key;
         ++key_ptr) {
        if (key_ptr->destroy) {
            key_ptr->destroy((char*)entry_ptr + key_ptr->ofs);
        }
    }
    *entry_ptr = (struct desktop_entry){};
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Callback handler for the inih parse methods. Parses "Desktop Entry".
 *
 * @param section_ptr
 * @param name_ptr
 * @param value_ptr
 *
 * @return 0 on error, and 1 if the name/value was parsed (or skipped for
 *     legitimate reason).
 */
int _desktop_parser_handler(
    void *userdata_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *val_ptr)
{
    struct _desktop_parser_handler_arg *arg = userdata_ptr;
    const struct desktop_parser *parser = arg->parser;
    struct desktop_entry *entry_ptr = arg->entry_ptr;

    // FIXME: Skip groups other than the main entry. Remove type?
    if (0 != strcmp(section_ptr, desktop_entry_group_name)) return 1;

    // Verify that the key is valid, with optional locale index.
    regmatch_t m[3];
    if (0 != regexec(&parser->key_regex, name_ptr, 3, &m[0], 0)) return 0;

    // If there is a LOCALE key, attempt to match it with the configured locale
    // and priority. 0 indicates no LOCALE key, higher value is better match.
    int priority = 0 < m[2].rm_so;
    for (; 0 < priority && priority <= 4; ++priority) {
        size_t len = parser->localization_key_len[priority - 1];
        const char *lk = parser->localization_key[priority - 1];
        if (0 < m[2].rm_eo - m[2].rm_so &&
            len + 2 == (size_t)(m[2].rm_eo - m[2].rm_so) &&
            0 == memcmp(lk, name_ptr + m[2].rm_so + 1, len)) break;
    }
    // 5 means we didn't have a match: Skip this key/value pair.
    if (5 <= priority) return 1;

    // Look for a matching key descriptor, then translate.
    for (const struct key_descriptor *kd = &keys[0]; kd->key; ++kd) {
        if (0 >= m[1].rm_eo - m[1].rm_so ||
            kd->len != (size_t)(m[1].rm_eo - m[1].rm_so) ||
            0 != memcmp(kd->key, name_ptr + m[1].rm_so, kd->len)) continue;

        // Descriptor doesn't expect a LOCALE, but the key had one: Fail.
        if (kd->priority_ofs == kd->ofs && 0 < priority) return 0;

        // Don't overwrite higher priority or earlier values for same priority.
        if (kd->priority_ofs != kd->ofs) {
            int8_t stored_priority = ((int8_t*)entry_ptr)[kd->priority_ofs];
            if (stored_priority & (1 << priority)) return 0;  // Duplicate.
            ((int8_t*)entry_ptr)[kd->priority_ofs] |= 1 << priority;
            if (stored_priority > (1 << priority)) return 1;  // Higher prio.
        } else {
            if (NULL != *(char**)((char*)entry_ptr + kd->ofs)) return 0;
        }

        if (!kd->translate(val_ptr, (char*)entry_ptr+kd->ofs, NULL)) return 0;
        return 1;
    }

    // Unknown key. Skip.
    return 1;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a lookup key for localization.
 *
 * @param l Language ("lang"). If NULL, no string is created.
 * @param t Territory (Freedesktop speficication: "COUNTRY"), may be NULL.
 * @param m Modifier ("MODIFIER"), may be NULL.
 *
 * @return Pointer to an allocated string, holding "lang[_COUNTRY][@MODIFIER]",
 *     or NULL on error, or NULL if `l` was NULL.
 */
char *_create_locale_key(const char *l, const char *t, const char *m)
{
    if (!l) return NULL;
    size_t len = strlen(l)+1 + (t ? strlen(t)+1 : 0) + (m ? strlen(m)+1 : 0);

    char *key_ptr = malloc(len);
    if (NULL == key_ptr) return NULL;

    snprintf(key_ptr, len, "%s%s%s%s%s", l,
             (t ? "_" : ""), (t ? t : ""),
             (m ? "@" : ""), (m ? m : ""));
    return key_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Translates the "Type" key into an enum deskop_entry_type.
 *
 * @param value_ptr
 * @param dest_ptr
 * @param prio_ptr            Unused.
 *
 * @return true on success.
 */
bool _desktop_parser_translate_type(
    const char *value_ptr,
    void *dest_ptr,
    int8_t *prio_ptr)
{
    enum desktop_entry_type *entry_type_ptr = dest_ptr;
    prio_ptr = prio_ptr;

    if (0 == strcmp("Application", value_ptr)) {
        *entry_type_ptr = DESKTOP_ENTRY_TYPE_APPLICATION;
        return true;
    } else if (0 == strcmp("Link", value_ptr)) {
        *entry_type_ptr = DESKTOP_ENTRY_TYPE_LINK;
        return true;
    } else if (0 == strcmp("Directory", value_ptr)) {
        *entry_type_ptr = DESKTOP_ENTRY_TYPE_DIRECTORY;
        return true;
    }
    *entry_type_ptr = DESKTOP_ENTRY_TYPE_UNKNOWN;
    return false;
}

/* == Unit tests =========================================================== */

static void _desktop_parser_test_ini_string(bs_test_t *test_ptr);
static void _desktop_parser_test_locale_string(bs_test_t *test_ptr);

/** Test cases for action items. */
static const bs_test_case_t   _desktop_parser_test_cases[] = {
    { true, "ini_string", _desktop_parser_test_ini_string },
    { true, "locale_string", _desktop_parser_test_locale_string },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t           desktop_parser_test_set = BS_TEST_SET(
    true, "parse", _desktop_parser_test_cases);

/* ------------------------------------------------------------------------- */
void _desktop_parser_test_ini_string(bs_test_t *test_ptr)
{
    struct desktop_parser *p = desktop_parser_create("en_US.UTF-8@euro");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, p);

    struct desktop_entry e = {};
    static const char *i = "\
[Desktop Entry]\n\
Exec=TheBinary\n\
Name[en]=Name1\n\
Name[en_US@euro]=Name2\n\
Name[en_US]=Name3\n\
Name[de]=DerName";
    BS_TEST_VERIFY_EQ(test_ptr, 0, desktop_parser_string_to_entry(p, i, &e));

    BS_TEST_VERIFY_STREQ(test_ptr, "TheBinary", e.exec_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "Name2", e.name_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority & (1 << 4));

    desktop_parser_entry_release(&e);
    desktop_parser_destroy(p);
}

/* ------------------------------------------------------------------------- */
void _desktop_parser_test_locale_string(bs_test_t *test_ptr)
{
    // For convenience.
    const char *gn = desktop_entry_group_name;
    int (*h)(void *, const char *, const char *, const char *) =
        _desktop_parser_handler;

    struct desktop_entry e = {};
    struct _desktop_parser_handler_arg ha = {
        .parser = desktop_parser_create("en_US.UTF-8@euro"),
        .entry_ptr = &e
    };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ha.parser);

    // 'Exec' is not a localestring. Fail.
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Exec[en]", "x"));

    // Name is a localestring. Exercise increasing priority.
    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name", "n0"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n0", e.name_ptr);
    // Cannot set twice.
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name", "n10"));

    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name[en]", "n1"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n1", e.name_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority && (1 << 1));
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name", "n0"));
    e.name_priority = 1 << 1;
    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name", "n0"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n1", e.name_ptr);

    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name[en@euro]", "n2"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n2", e.name_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority & (1 << 2));
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name[en]", "n1"));
    e.name_priority = 1 << 2;
    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name[en]", "n1"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n2", e.name_ptr);

    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name[en_US]", "n3"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n3", e.name_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority & (1 << 3));
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name[en@euro]", "n2"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n3", e.name_ptr);

    BS_TEST_VERIFY_NEQ(test_ptr, 0, h(&ha, gn, "Name[en_US@euro]", "n4"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n4", e.name_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority & (1 << 4));
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name[en_US@euro]", "n4"));
    BS_TEST_VERIFY_EQ(test_ptr, 0, h(&ha, gn, "Name[en_US]", "n3"));
    BS_TEST_VERIFY_STREQ(test_ptr, "n4", e.name_ptr);

    desktop_parser_destroy((struct desktop_parser*)ha.parser);
    desktop_parser_entry_release(&e);
}

/* == End of parse.c ======================================================= */
