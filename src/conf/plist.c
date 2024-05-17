/* ========================================================================= */
/**
 * @file plist.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "plist.h"

#include "grammar.h"
#include "analyzer.h"
#include "parser.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_create_object_from_plist_string(const char *buf_ptr)
{
    wlmcfg_parser_context_t ctx = {};

    yyscan_t scanner;
    yylex_init(&scanner);

    YY_BUFFER_STATE buf_state;
    buf_state = yy_scan_string(buf_ptr, scanner);
    int rv = yyparse(scanner, &ctx);
    yy_delete_buffer(buf_state, scanner);

    yylex_destroy(scanner);

    if (0 != rv) return NULL;
    return ctx.top_object_ptr;
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_create_object_from_plist_file(const char *fname_ptr)
{
    wlmcfg_parser_context_t ctx = {};

    FILE *file_ptr = fopen(fname_ptr, "r");
    if (NULL == file_ptr) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed fopen(%s, 'r')", fname_ptr);
        return NULL;
    }

    yyscan_t scanner;
    yylex_init(&scanner);

    YY_BUFFER_STATE buf_state;
    buf_state = yy_create_buffer(file_ptr, YY_BUF_SIZE, scanner);
    yy_switch_to_buffer(buf_state, scanner);
    int rv = yyparse(scanner, &ctx);
    yy_delete_buffer(buf_state, scanner);

    yylex_destroy(scanner);
    fclose(file_ptr);

    if (0 != rv) return NULL;
    return ctx.top_object_ptr;
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_from_string(bs_test_t *test_ptr);
static void test_from_file(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_plist_test_cases[] = {
    { 1, "from_string", test_from_string },
    { 1, "from_file", test_from_file },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests plist object creation from string. */
void test_from_string(bs_test_t *test_ptr)
{
    wlmcfg_object_t *object_ptr;

    object_ptr = wlmcfg_create_object_from_plist_string("value");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "value",
        wlmcfg_string_value(wlmcfg_string_from_object(object_ptr)));

    wlmcfg_object_destroy(object_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests plist object creation from string. */
void test_from_file(bs_test_t *test_ptr)
{
    wlmcfg_object_t *object_ptr;

    object_ptr = wlmcfg_create_object_from_plist_file(
        bs_test_resolve_path("conf/string.plist"));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "file_value",
        wlmcfg_string_value(wlmcfg_string_from_object(object_ptr)));

    wlmcfg_object_destroy(object_ptr);
}

/* == End of plist.c ======================================================= */
