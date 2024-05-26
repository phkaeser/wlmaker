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

static wlmcfg_object_t *_wlmcfg_create_object_from_plist_scanner(
    yyscan_t scanner);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_create_object_from_plist_string(const char *buf_ptr)
{
    yyscan_t scanner;
    yylex_init(&scanner);

    YY_BUFFER_STATE buf_state;
    buf_state = yy_scan_string(buf_ptr, scanner);
    yy_switch_to_buffer(buf_state, scanner);

    wlmcfg_object_t *obj = _wlmcfg_create_object_from_plist_scanner(scanner);

    yy_delete_buffer(buf_state, scanner);
    yylex_destroy(scanner);

    return obj;
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_create_object_from_plist_file(const char *fname_ptr)
{
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

    wlmcfg_object_t *obj = _wlmcfg_create_object_from_plist_scanner(scanner);
    yy_delete_buffer(buf_state, scanner);
    yylex_destroy(scanner);
    fclose(file_ptr);

    return obj;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Does a parser run, from the initialized scanner. */
wlmcfg_object_t *_wlmcfg_create_object_from_plist_scanner(yyscan_t scanner)
{
    wlmcfg_parser_context_t ctx = {};
    if (!bs_ptr_stack_init(&ctx.object_stack)) return NULL;
    // TODO(kaeser@gubbe.ch): Clean up stack on error!
    int rv = yyparse(scanner, &ctx);
    wlmcfg_object_t *object_ptr = bs_ptr_stack_pop(&ctx.object_stack);
    bs_ptr_stack_fini(&ctx.object_stack);

    if (0 != rv) {
        wlmcfg_object_unref(object_ptr);
        object_ptr = NULL;
    }
    return object_ptr;
}

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
    wlmcfg_object_t *object_ptr, *v_ptr;

    // A string.
    object_ptr = wlmcfg_create_object_from_plist_string("value");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "value",
        wlmcfg_string_value(wlmcfg_string_from_object(object_ptr)));
    wlmcfg_object_unref(object_ptr);

    // A dict.
    object_ptr = wlmcfg_create_object_from_plist_string(
        "{key1=dict_value1;key2=dict_value2}");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    v_ptr = wlmcfg_dict_get(dict_ptr, "key1");
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "dict_value1",
        wlmcfg_string_value(wlmcfg_string_from_object(v_ptr)));
    v_ptr = wlmcfg_dict_get(dict_ptr, "key2");
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "dict_value2",
        wlmcfg_string_value(wlmcfg_string_from_object(v_ptr)));
    wlmcfg_object_unref(object_ptr);

    // A dict with a duplicate key. Will return NULL, no need to unref.
    object_ptr = wlmcfg_create_object_from_plist_string(
        "{key1=dict_value1;key1=dict_value2}");
    BS_TEST_VERIFY_EQ(test_ptr, NULL, object_ptr);

    // An array.
    object_ptr = wlmcfg_create_object_from_plist_string(
        "(elem0,elem1)");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    wlmcfg_array_t *array_ptr = wlmcfg_array_from_object(object_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "elem0",
        wlmcfg_string_value(wlmcfg_string_from_object(
                                wlmcfg_array_at(array_ptr, 0))));
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "elem1",
        wlmcfg_string_value(wlmcfg_string_from_object(
                                wlmcfg_array_at(array_ptr, 1))));
    wlmcfg_object_unref(object_ptr);

}

/* ------------------------------------------------------------------------- */
/** Tests plist object creation from string. */
void test_from_file(bs_test_t *test_ptr)
{
    wlmcfg_object_t *object_ptr, *v_ptr;

    object_ptr = wlmcfg_create_object_from_plist_file(
        bs_test_resolve_path("conf/string.plist"));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "file_value",
        wlmcfg_string_value(wlmcfg_string_from_object(object_ptr)));
    wlmcfg_object_unref(object_ptr);

    object_ptr = wlmcfg_create_object_from_plist_file(
        bs_test_resolve_path("conf/dict.plist"));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);

    v_ptr = BS_ASSERT_NOTNULL(wlmcfg_dict_get(dict_ptr, "key0"));
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "value0",
        wlmcfg_string_value(wlmcfg_string_from_object(v_ptr)));

    wlmcfg_object_unref(object_ptr);

    object_ptr = wlmcfg_create_object_from_plist_file(
        bs_test_resolve_path("conf/array.plist"));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, object_ptr);
    wlmcfg_array_t *array_ptr = wlmcfg_array_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, array_ptr);
    wlmcfg_object_unref(object_ptr);
}

/* == End of plist.c ======================================================= */
