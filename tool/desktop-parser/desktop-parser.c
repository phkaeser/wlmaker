/* ========================================================================= */
/**
 * @file desktop-parser.c
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

#include "desktop-parser.h"

#include <ini.h>
#include <libbase/libbase.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

/** Group name for the application details in a `.desktop` file. */
static const char *desktop_entry_group_name = "Desktop Entry";

/** Regular expression describing a (possibly localized) key. */
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
    void *dest_ptr);
static bool _desktop_parser_translate_boolean(
    const char *value_ptr,
    void *dest_ptr);
static bool _desktop_parser_translate_string(
    const char *value_ptr,
    void *dest_ptr);
static bool _desktop_parser_translate_strings(
    const char *value_ptr,
    void *dest_ptr);
static bool _desktop_parser_translate_exec(
    const char *value_ptr,
    void *dest_ptr);
static void _desktop_parser_destroy_string(void *dest_ptr);
static void _desktop_parser_destroy_strings(void *dest_ptr);

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
    bool (*translate)(const char *value_ptr, void *dest_ptr);
    /** Dtor. */
    void (*destroy)(void *dest_ptr);
};

/** Supported keys. */
struct key_descriptor keys[] = {
    {
        .key = "Type",
        .len = strlen("Type"),
        .ofs = offsetof(struct desktop_entry, type),
        .priority_ofs = offsetof(struct desktop_entry, type),
        .destroy = NULL,
        .translate = _desktop_parser_translate_type,
    },
    {
        .key = "Hidden",
        .len = strlen("Hidden"),
        .ofs = offsetof(struct desktop_entry, hidden),
        .priority_ofs = offsetof(struct desktop_entry, hidden),
        .destroy = NULL,
        .translate = _desktop_parser_translate_boolean,
    },
    {
        .key = "NoDisplay",
        .len = strlen("NoDisplay"),
        .ofs = offsetof(struct desktop_entry, no_display),
        .priority_ofs = offsetof(struct desktop_entry, no_display),
        .destroy = NULL,
        .translate = _desktop_parser_translate_boolean,
    },
    {
        .key = "Terminal",
        .len = strlen("Terminal"),
        .ofs = offsetof(struct desktop_entry, terminal),
        .priority_ofs = offsetof(struct desktop_entry, terminal),
        .destroy = NULL,
        .translate = _desktop_parser_translate_boolean,
    },
    {
        .key = "Name",
        .len = strlen("Name"),
        .ofs = offsetof(struct desktop_entry, name_ptr),
        .translate = _desktop_parser_translate_string,
        .destroy = _desktop_parser_destroy_string,
        .priority_ofs = offsetof(struct desktop_entry, name_priority),
    },
    {
        .key = "Exec",
        .len = strlen("Exec"),
        .ofs = offsetof(struct desktop_entry, exec_ptr),
        .priority_ofs = offsetof(struct desktop_entry, exec_ptr),
        .destroy = _desktop_parser_destroy_string,
        .translate = _desktop_parser_translate_exec
    },
    {
        .key = "TryExec",
        .len = strlen("TryExec"),
        .ofs = offsetof(struct desktop_entry, try_exec_ptr),
        .priority_ofs = offsetof(struct desktop_entry, try_exec_ptr),
        .destroy = _desktop_parser_destroy_string,
        .translate = _desktop_parser_translate_string
    },
    {
        .key = "Path",
        .len = strlen("Path"),
        .ofs = offsetof(struct desktop_entry, path_ptr),
        .priority_ofs = offsetof(struct desktop_entry, path_ptr),
        .destroy = _desktop_parser_destroy_string,
        .translate = _desktop_parser_translate_string
    },
    {
        .key = "Categories",
        .len = strlen("Categories"),
        .ofs = offsetof(struct desktop_entry, category_ptrs),
        .priority_ofs = offsetof(struct desktop_entry, category_ptrs),
        .destroy = _desktop_parser_destroy_strings,
        .translate = _desktop_parser_translate_strings
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
int desktop_parser_file_to_entry(
    const struct desktop_parser *parser,
    const char *fname_ptr,
    struct desktop_entry *entry_ptr)
{
    struct _desktop_parser_handler_arg arg = {
        .parser = parser, .entry_ptr = entry_ptr
    };
    return ini_parse(fname_ptr, _desktop_parser_handler, &arg);
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
 * @param userdata_ptr
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
    const char *value_ptr)
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
        }

        if (!kd->translate(value_ptr, (char*)entry_ptr+kd->ofs)) return 0;
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
 *
 * @return true on success.
 */
bool _desktop_parser_translate_type(
    const char *value_ptr,
    void *dest_ptr)
{
    enum desktop_entry_type *entry_type_ptr = dest_ptr;

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

/* ------------------------------------------------------------------------- */
/**
 * Translates a boolean-typed value into a bool at *dest_ptr.
 *
 * @param value_ptr           Must either be "false" or "true".
 * @param dest_ptr            Pointer to a bool.
 *
 * @return true on success.
 */
bool _desktop_parser_translate_boolean(
    const char *value_ptr,
    void *dest_ptr)
{
    bool *bool_ptr = dest_ptr;

    if (0 == strcmp("true", value_ptr)) {
        *bool_ptr = true;
        return true;
    } else if (0 == strcmp("false", value_ptr)) {
        *bool_ptr = false;
        return true;
    }
    *bool_ptr = false;
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Translates a string, while un-escaping supported escape codes (s, n, t, r).
 *
 * @param value_ptr
 * @param dest_ptr
 *
 * @return true on success.
 */
bool _desktop_parser_translate_string(
    const char *value_ptr,
    void *dest_ptr)
{
    char **str_ptr_ptr = dest_ptr;

    if (NULL != *str_ptr_ptr) {
        free(*str_ptr_ptr);
    }

    char *d = malloc(strlen(value_ptr) + 1);
    if (NULL == d) return false;
    *str_ptr_ptr = d;
    for (const char *s = value_ptr; *s != '\0'; ++s) {
        if (*s == '\\') {
            switch (*++s) {
            case 's': *d++ = ' '; break;
            case 'n': *d++ = '\n'; break;
            case 't': *d++ = '\t'; break;
            case 'r': *d++ = '\r'; break;
            case '\\': *d++ = '\\'; break;
            default: /* Escaped code not specified. Skip. */ break;
            }
        } else {
            *d++ = *s;
        }
    }
    *d = '\0';
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Translates multiple strings, separated by semicolon; as for "Categories".
 *
 * @param value_ptr
 * @param dest_ptr
 *
 * @return true on success.
 */
bool _desktop_parser_translate_strings(
    const char *value_ptr,
    void *dest_ptr)
{
    char *translated_ptr = NULL;
    bool rv = _desktop_parser_translate_string(value_ptr, &translated_ptr);
    if (!rv) goto error;

    char ***category_ptrs_ptr = dest_ptr;
    *category_ptrs_ptr = calloc( 1, sizeof(char*));
    if (NULL == *category_ptrs_ptr) goto error;

    size_t cat_idx = 0;
    for (char *cat_ptr = translated_ptr; *cat_ptr != '\0'; ++cat_ptr) {
        // Unescape the semicolon and terminate this category string.
        char *s = cat_ptr, *d = cat_ptr;
        for (; *s != ';' && *s != '\0'; ++s, ++d) {
            if (*s == '\\') ++s;
            *d = *s;
        }
        *d = '\0';

        *category_ptrs_ptr = realloc(
            *category_ptrs_ptr,
            (cat_idx + 2) * sizeof(char*));
        if (NULL == *category_ptrs_ptr) goto error;
        (*category_ptrs_ptr)[cat_idx + 1] = NULL;

        (*category_ptrs_ptr)[cat_idx] = strdup(cat_ptr);
        if (NULL == (*category_ptrs_ptr)[cat_idx]) goto error;
        cat_ptr = s;
        ++cat_idx;
    }
    free(translated_ptr);
    return true;

error:
    if (translated_ptr) free(translated_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Translates an exec key value, and un-escapes the specific escape codes.
 *
 * https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
 *
 * TODO(kaeser@gubbe.ch): This is... lossy. When un-escaping the arguments, the
 * result should be stored as separate strings, such as usable for execve(2).
 *
 * @param value_ptr
 * @param dest_ptr
 *
 * @return true on success
 */
bool _desktop_parser_translate_exec(
    const char *value_ptr,
    void *dest_ptr)
{
    char **str_ptr_ptr = dest_ptr;

    if (NULL != *str_ptr_ptr) {
        free(*str_ptr_ptr);
    }

    char *d = calloc(1, (strlen(value_ptr) + 1));
    if (NULL == d) return false;
    *str_ptr_ptr = d;
    bool quoted_arg = false;
    for (const char *s = value_ptr; *s != '\0'; ++s) {
        if (quoted_arg) {
            if (*s == '"') {
                quoted_arg = false;
            } else if (*s == '\\') {
                switch (*++s) {
                case '"': *d++ = '"'; break;
                case '`': *d++ = '`'; break;
                case '$': *d++ = '$'; break;
                case '\\': *d++ = '\\'; break;
                default: /* Invalid escape code. */ return false;
                }
            } else {
                *d++ = *s;
            }
        } else {
            if (*s == '"') {
                quoted_arg = true;
            } else if (*s == '%') {
                if (!strchr("fFuUdDnNickvm", *(s + 1))) return false;
                ++s;  // For now: Skip all (valid) field codes.
            } else {
                *d++ = *s;
            }
        }
    }
    *d = '\0';
    return !quoted_arg;  // All quotes must be closed.
}

/* ------------------------------------------------------------------------- */
/** Frees the memory associated with the string. */
void _desktop_parser_destroy_string(void *dest_ptr)
{
    char **str_ptr_ptr = dest_ptr;
    if (NULL == *str_ptr_ptr) return;
    free(*str_ptr_ptr);
    *str_ptr_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Frees the memory associated with the NULL-terminated string array. */
void _desktop_parser_destroy_strings(void *dest_ptr)
{
    char ***str_ptr_ptr = (char***)dest_ptr;
    if (NULL == *str_ptr_ptr) return;

    for (char **s = *str_ptr_ptr; *s != NULL; ++s) {
        free(*s);
    }

    free(*str_ptr_ptr);
    *(void**)dest_ptr = NULL;
}

/* == Unit tests =========================================================== */

static void _desktop_parser_test_ini_string(bs_test_t *test_ptr);
static void _desktop_parser_test_ini_file(bs_test_t *test_ptr);
static void _desktop_parser_test_locale_string(bs_test_t *test_ptr);
static void _desktop_parser_test_translate(bs_test_t *test_ptr);

/** Test cases for action items. */
static const bs_test_case_t   _desktop_parser_test_cases[] = {
    { true, "ini_string", _desktop_parser_test_ini_string },
    { true, "ini_file", _desktop_parser_test_ini_file },
    { true, "locale_string", _desktop_parser_test_locale_string },
    { true, "translate", _desktop_parser_test_translate },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t           desktop_parser_test_set = BS_TEST_SET(
    true, "desktop-parser", _desktop_parser_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests parsing INI content, but from a string. */
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
/** Tests parsing a sample .desktop file. */
void _desktop_parser_test_ini_file(bs_test_t *test_ptr)
{
    struct desktop_parser *p = desktop_parser_create("en_US.UTF-8@euro");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, p);

    struct desktop_entry e = {};
    const char *f = bs_test_data_path(test_ptr, "wlmaker.desktop");
    BS_TEST_VERIFY_EQ_OR_RETURN(
        test_ptr, 0, desktop_parser_file_to_entry(p, f, &e));

    BS_TEST_VERIFY_EQ(test_ptr, DESKTOP_ENTRY_TYPE_APPLICATION, e.type);
    BS_TEST_VERIFY_STREQ(test_ptr, "WaylandMaker", e.name_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "/usr/local/bin/wlmaker", e.exec_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "./wlmaker", e.try_exec_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "/usr/local", e.path_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, e.hidden);
    BS_TEST_VERIFY_FALSE(test_ptr, e.no_display);
    BS_TEST_VERIFY_TRUE(test_ptr, e.terminal);

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, e.category_ptrs);
    BS_TEST_VERIFY_STREQ(test_ptr, "System", e.category_ptrs[0]);
    BS_TEST_VERIFY_STREQ(test_ptr, "Compositor", e.category_ptrs[1]);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, e.category_ptrs[2]);

    desktop_parser_entry_release(&e);
    desktop_parser_destroy(p);
}

/* ------------------------------------------------------------------------- */
/** Tests parsing localized strings. */
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
    BS_TEST_VERIFY_NEQ(test_ptr, 0, e.name_priority & (1 << 1));
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

/* ------------------------------------------------------------------------- */
/** Tests translators: Escaped string. */
void _desktop_parser_test_translate(bs_test_t *test_ptr)
{
    struct desktop_parser *p = desktop_parser_create("en_US.UTF-8@euro");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, p);
    struct desktop_entry e = {};

    const char *i = "[Desktop Entry]\nName=A\\sB\\nC\\tD\\rE\\\\F\\xG";
    BS_TEST_VERIFY_EQ(test_ptr, 0, desktop_parser_string_to_entry(p, i, &e));
    BS_TEST_VERIFY_STREQ(test_ptr, "A B\nC\tD\rE\\FG", e.name_ptr);

    i = "[Desktop Entry]\nExec=a %f %U \"a \\` \\\" \\$ \\\\ \"";
    BS_TEST_VERIFY_EQ(test_ptr, 0, desktop_parser_string_to_entry(p, i, &e));
    BS_TEST_VERIFY_STREQ(test_ptr, "a   a ` \" $ \\ ", e.exec_ptr);

    desktop_parser_entry_release(&e);
    desktop_parser_destroy(p);
}

/* == End of parse.c ======================================================= */
