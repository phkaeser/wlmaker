/* ========================================================================= */
/**
 * @file decode.c
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
 *
 * Configurables for wlmaker. Currently, this file lists hardcoded entities,
 * and mainly serves as a catalog about which entities should be dynamically
 * configurable.
 */

#include "decode.h"

#include "plist.h"

/* == Declarations ========================================================= */

/** A pointer of type `value_type`, at `offset` behind `base_ptr`. */
#define BS_VALUE_AT(_value_type, _base_ptr, _offset) \
    ((_value_type*)((uint8_t*)(_base_ptr) + (_offset)))

static bool _wlmcfg_init_defaults(
    const wlmcfg_desc_t *desc_ptr,
    void *dest_ptr);

static bool _wlmcfg_decode_uint64(
    wlmcfg_object_t *obj_ptr,
    uint64_t *uint64_ptr);
static bool _wlmcfg_decode_int64(
    wlmcfg_object_t *obj_ptr,
    int64_t *int64_ptr);
static bool _wlmcfg_decode_argb32(
    wlmcfg_object_t *obj_ptr,
    uint32_t *argb32_ptr);
static bool _wlmcfg_decode_bool(
    wlmcfg_object_t *obj_ptr,
    bool *bool_ptr);
static bool _wlmcfg_decode_enum(
    wlmcfg_object_t *obj_ptr,
    const wlmcfg_enum_desc_t *enum_desc_ptr,
    int *enum_value_ptr);
static bool _wlmcfg_decode_string(
    wlmcfg_object_t *obj_ptr,
    char **str_ptr_ptr);

/** Enum descriptor for decoding bool. */
static const wlmcfg_enum_desc_t _wlmcfg_bool_desc[] = {
    WLMCFG_ENUM("True", true),
    WLMCFG_ENUM("False", false),
    WLMCFG_ENUM("Yes", true),
    WLMCFG_ENUM("No", false),
    WLMCFG_ENUM("Enabled", true),
    WLMCFG_ENUM("Disabled", false),
    WLMCFG_ENUM("On", true),
    WLMCFG_ENUM("Off", false),
    WLMCFG_ENUM_SENTINEL()
};
/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmcfg_decode_dict(
    __UNUSED__ wlmcfg_dict_t *dict_ptr,
    const wlmcfg_desc_t *desc_ptr,
    void *dest_ptr)
{
    if (!_wlmcfg_init_defaults(desc_ptr, dest_ptr)) return false;

    for (const wlmcfg_desc_t *iter_desc_ptr = desc_ptr;
         iter_desc_ptr->key_ptr != NULL;
         ++iter_desc_ptr) {

        wlmcfg_object_t *obj_ptr = wlmcfg_dict_get(
            dict_ptr, iter_desc_ptr->key_ptr);
        // No such value. We currently don't test for required entries.
        if (NULL == obj_ptr) continue;

        bool rv = false;
        switch (iter_desc_ptr->type) {
        case WLMCFG_TYPE_UINT64:
            rv = _wlmcfg_decode_uint64(
                obj_ptr,
                BS_VALUE_AT(uint64_t, dest_ptr, iter_desc_ptr->field_offset));
            break;
        case WLMCFG_TYPE_INT64:
            rv = _wlmcfg_decode_int64(
                obj_ptr,
                BS_VALUE_AT(int64_t, dest_ptr, iter_desc_ptr->field_offset));
            break;
        case WLMCFG_TYPE_ARGB32:
            rv = _wlmcfg_decode_argb32(
                obj_ptr,
                BS_VALUE_AT(uint32_t, dest_ptr, iter_desc_ptr->field_offset));
            break;
        case WLMCFG_TYPE_BOOL:
            rv = _wlmcfg_decode_bool(
                obj_ptr,
                BS_VALUE_AT(bool, dest_ptr, iter_desc_ptr->field_offset));
            break;
        case WLMCFG_TYPE_ENUM:
            rv = _wlmcfg_decode_enum(
                obj_ptr,
                iter_desc_ptr->v.v_enum.desc_ptr,
                BS_VALUE_AT(int, dest_ptr, iter_desc_ptr->field_offset));
            break;
        case WLMCFG_TYPE_STRING:
            rv = _wlmcfg_decode_string(
                obj_ptr,
                BS_VALUE_AT(char*, dest_ptr, iter_desc_ptr->field_offset));
            break;
        default:
            bs_log(BS_ERROR, "Unsupported type %d.", iter_desc_ptr->type);
            return false;
        }
    }
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes default values at the destination, as described.
 *
 * @param desc_ptr
 * @param dest_ptr
 */
bool _wlmcfg_init_defaults(const wlmcfg_desc_t *desc_ptr,
                           void *dest_ptr)
{
    for (const wlmcfg_desc_t *iter_desc_ptr = desc_ptr;
         iter_desc_ptr->key_ptr != NULL;
         ++iter_desc_ptr) {
        switch (iter_desc_ptr->type) {
        case WLMCFG_TYPE_UINT64:
            *BS_VALUE_AT(uint64_t, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_uint64.default_value;
            break;

        case WLMCFG_TYPE_INT64:
            *BS_VALUE_AT(int64_t, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_int64.default_value;
            break;

        case WLMCFG_TYPE_ARGB32:
            *BS_VALUE_AT(uint32_t, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_argb32.default_value;
            break;

        case WLMCFG_TYPE_BOOL:
            *BS_VALUE_AT(bool, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_bool.default_value;
            break;

        case WLMCFG_TYPE_ENUM:
            *BS_VALUE_AT(int, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_enum.default_value;
            break;

        case WLMCFG_TYPE_STRING:
            char **str_ptr = BS_VALUE_AT(
                char*, dest_ptr, iter_desc_ptr->field_offset);
            if (NULL != *str_ptr) free(*str_ptr);
            *str_ptr = logged_strdup(
                iter_desc_ptr->v.v_string.default_value_ptr);
            if (NULL == *str_ptr) return false;
            break;

        default:
            bs_log(BS_ERROR, "Unsupported type %d.", iter_desc_ptr->type);
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/** Decodes an unsigned number, using uint64_t as carry-all. */
bool _wlmcfg_decode_uint64(wlmcfg_object_t *obj_ptr, uint64_t *uint64_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(obj_ptr);
    if (NULL == string_ptr) return false;
    const char *value_ptr = wlmcfg_string_value(string_ptr);
    if (NULL == value_ptr) return false;
    return bs_strconvert_uint64(value_ptr, uint64_ptr, 10);
}

/* ------------------------------------------------------------------------- */
/** Decodes a signed number, using int64_t as carry-all. */
bool _wlmcfg_decode_int64(wlmcfg_object_t *obj_ptr, int64_t *int64_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(obj_ptr);
    if (NULL == string_ptr) return false;
    const char *value_ptr = wlmcfg_string_value(string_ptr);
    if (NULL == value_ptr) return false;
    return bs_strconvert_int64(value_ptr, int64_ptr, 10);
}

/* ------------------------------------------------------------------------- */
/** Deocdes an ARGB32 value from the config object. */
bool _wlmcfg_decode_argb32(wlmcfg_object_t *obj_ptr, uint32_t *argb32_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(obj_ptr);
    if (NULL == string_ptr) return false;

    const char *value_ptr = wlmcfg_string_value(string_ptr);
    if (NULL == value_ptr) return false;
    int rv = sscanf(value_ptr, "argb32:%"PRIx32, argb32_ptr);
    if (1 != rv) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed sscanf(\"%s\", \"argb32:%%"PRIx32", %p)",
               value_ptr, argb32_ptr);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Translates a bool value from the string. */
bool _wlmcfg_decode_bool(
    wlmcfg_object_t *obj_ptr,
    bool *bool_ptr)
{
    int bool_value;
    bool rv = _wlmcfg_decode_enum(obj_ptr, _wlmcfg_bool_desc, &bool_value);
    if (rv) *bool_ptr = bool_value;
    return rv;
}

/* ------------------------------------------------------------------------- */
/** Translates a enum value from the string, using the provided descriptor. */
bool _wlmcfg_decode_enum(
    wlmcfg_object_t *obj_ptr,
    const wlmcfg_enum_desc_t *enum_desc_ptr,
    int *enum_value_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(obj_ptr);
    if (NULL == string_ptr) return false;
    const char *value_ptr = wlmcfg_string_value(string_ptr);
    if (NULL == value_ptr) return false;

    for (; NULL != enum_desc_ptr->name_ptr; ++enum_desc_ptr) {
        if (0 == strcmp(enum_desc_ptr->name_ptr, value_ptr)) {
            *enum_value_ptr = enum_desc_ptr->value;
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------------- */
/** Translates (ie. duplicates) a string value from the plist string. */
bool _wlmcfg_decode_string(
    wlmcfg_object_t *obj_ptr,
    char **str_ptr_ptr)
{
    wlmcfg_string_t *string_ptr = wlmcfg_string_from_object(obj_ptr);
    if (NULL == string_ptr) return false;
    const char *value_ptr = wlmcfg_string_value(string_ptr);
    if (NULL == value_ptr) return false;

    if (NULL != *str_ptr_ptr) free(*str_ptr_ptr);
    *str_ptr_ptr = logged_strdup(value_ptr);
    return (NULL != *str_ptr_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_defaults(bs_test_t *test_ptr);
static void test_decode_dict(bs_test_t *test_ptr);
static void test_decode_number(bs_test_t *test_ptr);
static void test_decode_argb32(bs_test_t *test_ptr);
static void test_decode_bool(bs_test_t *test_ptr);
static void test_decode_enum(bs_test_t *test_ptr);
static void test_decode_string(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_decode_test_cases[] = {
    { 1, "init_defaults", test_init_defaults },
    { 1, "dict", test_decode_dict },
    { 1, "number", test_decode_number },
    { 1, "argb32", test_decode_argb32 },
    { 1, "bool", test_decode_bool },
    { 1, "enum", test_decode_enum },
    { 1, "string", test_decode_string },
    { 0, NULL, NULL },
};

/** Structure with test values. */
typedef struct {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    uint64_t                  v_uint64;
    int64_t                   v_int64;
    uint32_t                  v_argb32;
    bool                      v_bool;
    int                       v_enum;
    char                      *v_string;
#endif  // DOXYGEN_SHOULD_SKIP_THIS
} _test_value_t;

/** An enum descriptor. */
static const wlmcfg_enum_desc_t _test_enum_desc[] = {
    WLMCFG_ENUM("enum1", 1),
    WLMCFG_ENUM("enum2", 2),
    WLMCFG_ENUM_SENTINEL()
};

/** Test descriptor. */
static const wlmcfg_desc_t _wlmcfg_decode_test_desc[] = {
    WLMCFG_DESC_UINT64("u64", _test_value_t, v_uint64, 1234),
    WLMCFG_DESC_INT64("i64", _test_value_t, v_int64, -1234),
    WLMCFG_DESC_ARGB32("argb32", _test_value_t, v_argb32, 0x01020304),
    WLMCFG_DESC_BOOL("bool", _test_value_t, v_bool, true),
    WLMCFG_DESC_ENUM("enum", _test_value_t, v_enum, 3, _test_enum_desc),
    WLMCFG_DESC_STRING("string", _test_value_t, v_string, "The String"),
    WLMCFG_DESC_SENTINEL(),
};

/* ------------------------------------------------------------------------- */
/** Tests initialization of default values. */
void test_init_defaults(bs_test_t *test_ptr)
{
    _test_value_t val = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmcfg_init_defaults(_wlmcfg_decode_test_desc, &val));
    BS_TEST_VERIFY_EQ(test_ptr, 1234, val.v_uint64);
    BS_TEST_VERIFY_EQ(test_ptr, -1234, val.v_int64);
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, val.v_argb32);
    BS_TEST_VERIFY_EQ(test_ptr, true, val.v_bool);
    BS_TEST_VERIFY_EQ(test_ptr, 3, val.v_enum);
    BS_TEST_VERIFY_STREQ(test_ptr, "The String", val.v_string);
    free(val.v_string);
}

/* ------------------------------------------------------------------------- */
/** Tests dict decoding. */
void test_decode_dict(bs_test_t *test_ptr)
{
    _test_value_t val = {};
    const char *plist_string_ptr = ("{"
                                    "u64 = \"100\";"
                                    "i64 = \"-101\";"
                                    "argb32 = \"argb32:0204080c\";"
                                    "bool = Disabled;"
                                    "enum = enum1;"
                                    "string = TestString"
                                    "}");

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
        wlmcfg_create_object_from_plist_string(plist_string_ptr));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_decode_dict(dict_ptr, _wlmcfg_decode_test_desc, &val));
    BS_TEST_VERIFY_EQ(test_ptr, 100, val.v_uint64);
    BS_TEST_VERIFY_EQ(test_ptr, -101, val.v_int64);
    BS_TEST_VERIFY_EQ(test_ptr, 0x0204080c, val.v_argb32);
    BS_TEST_VERIFY_EQ(test_ptr, false, val.v_bool);
    BS_TEST_VERIFY_EQ(test_ptr, 1, val.v_enum);
    BS_TEST_VERIFY_STREQ(test_ptr, "TestString", val.v_string);
    wlmcfg_dict_unref(dict_ptr);
    free(val.v_string);
}

/* ------------------------------------------------------------------------- */
/** Tests number decoding. */
void test_decode_number(bs_test_t *test_ptr)
{
    wlmcfg_object_t *obj_ptr;
    int64_t i64;
    uint64_t u64;

    obj_ptr = wlmcfg_create_object_from_plist_string("42");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_uint64(obj_ptr, &u64));
    BS_TEST_VERIFY_EQ(test_ptr, 42, u64);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("\"-1234\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmcfg_decode_uint64(obj_ptr, &u64));
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("42");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_int64(obj_ptr, &i64));
    BS_TEST_VERIFY_EQ(test_ptr, 42, i64);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("\"-1234\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_int64(obj_ptr, &i64));
    BS_TEST_VERIFY_EQ(test_ptr, -1234, i64);
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests argb32 decoding. */
void test_decode_argb32(bs_test_t *test_ptr)
{
    wlmcfg_object_t *obj_ptr = wlmcfg_create_object_from_plist_string(
        "\"argb32:01020304\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);

    uint32_t argb32;
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_argb32(obj_ptr, &argb32));
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, argb32);
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests bool decoding. */
void test_decode_bool(bs_test_t *test_ptr)
{
    bool value;
    wlmcfg_object_t *obj_ptr;

    obj_ptr = wlmcfg_create_object_from_plist_string("Yes");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_bool(obj_ptr, &value));
    BS_TEST_VERIFY_TRUE(test_ptr, value);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("Disabled");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_bool(obj_ptr, &value));
    BS_TEST_VERIFY_FALSE(test_ptr, value);
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests enum decoding. */
void test_decode_enum(bs_test_t *test_ptr)
{
    int value;
    wlmcfg_object_t *obj_ptr;

    obj_ptr = wlmcfg_create_object_from_plist_string("enum2");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmcfg_decode_enum(obj_ptr, _test_enum_desc, &value));
    BS_TEST_VERIFY_EQ(test_ptr, 2, value);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("\"enum2\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmcfg_decode_enum(obj_ptr, _test_enum_desc, &value));
    BS_TEST_VERIFY_EQ(test_ptr, 2, value);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("INVALID");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        _wlmcfg_decode_enum(obj_ptr, _test_enum_desc, &value));
    wlmcfg_object_unref(obj_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests string decoding. */
void test_decode_string(bs_test_t *test_ptr)
{
    char *v_ptr = NULL;
    wlmcfg_object_t *obj_ptr;

    obj_ptr = wlmcfg_create_object_from_plist_string("TheString");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_string(obj_ptr, &v_ptr));
    BS_TEST_VERIFY_STREQ(test_ptr, "TheString", v_ptr);
    wlmcfg_object_unref(obj_ptr);
    free(v_ptr);
    v_ptr = NULL;

    obj_ptr = wlmcfg_create_object_from_plist_string("1234");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_string(obj_ptr, &v_ptr));
    BS_TEST_VERIFY_STREQ(test_ptr, "1234", v_ptr);
    wlmcfg_object_unref(obj_ptr);
    // Not free-ing v_ptr => the next 'decode' call has to do that.

    obj_ptr = wlmcfg_create_object_from_plist_string("\"quoted string\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_string(obj_ptr, &v_ptr));
    BS_TEST_VERIFY_STREQ(test_ptr, "quoted string", v_ptr);
    wlmcfg_object_unref(obj_ptr);
    free(v_ptr);
}

/* == End of decode.c ====================================================== */
