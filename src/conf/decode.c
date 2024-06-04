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

static bool _wlmcfg_init_defaults(
    const wlmcfg_desc_t *desc_ptr,
    void *dest_ptr);

static bool _wlmcfg_decode_int64(
    wlmcfg_object_t *obj_ptr,
    int64_t *int64_ptr);
static bool _wlmcfg_decode_uint64(
    wlmcfg_object_t *obj_ptr,
    uint64_t *uint64_ptr);
static bool _wlmcfg_decode_argb32(
    wlmcfg_object_t *obj_ptr,
    uint32_t *argb32_ptr);
static bool _wlmcfg_decode_enum(
    wlmcfg_object_t *obj_ptr,
    const wlmcfg_enum_desc_t *enum_desc_ptr,
    int *enum_value_ptr);

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
        if (NULL == obj_ptr) {
            bs_log(BS_ERROR, "Key \"%s\" not found in dict %p.",
                   iter_desc_ptr->key_ptr, dict_ptr);
            return false;
        }

        switch (iter_desc_ptr->type) {
        default:
            bs_log(BS_ERROR, "Unsupported type %d.", iter_desc_ptr->type);
            return false;
        }
    }
    return true;
}

/* == Local (static) methods =============================================== */

/** A pointer of type `value_type`, at `offset` behind `base_ptr`. */
#define BS_VALUE_AT(_value_type, _base_ptr, _offset) \
    ((_value_type*)((uint8_t*)(_base_ptr) + (_offset)))

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

        case WLMCFG_TYPE_ENUM:
            *BS_VALUE_AT(int, dest_ptr, iter_desc_ptr->field_offset) =
                iter_desc_ptr->v.v_enum.default_value;
            break;

        default:
            bs_log(BS_ERROR, "Unsupported type %d.", iter_desc_ptr->type);
            return false;
        }
    }
    return true;
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
/** Translates a enum value from the string, using the provided decsriptor. */
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

/* == Unit tests =========================================================== */

static void test_init_defaults(bs_test_t *test_ptr);
static void test_decode_number(bs_test_t *test_ptr);
static void test_decode_argb32(bs_test_t *test_ptr);
static void test_decode_enum(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_decode_test_cases[] = {
    { 1, "init_defaults", test_init_defaults },
    { 1, "number", test_decode_number },
    { 1, "argb32", test_decode_argb32 },
    { 1, "enum", test_decode_enum },
    { 0, NULL, NULL },
};

/** Structure with test values. */
typedef struct {
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    uint64_t                  v_uint64;
    int64_t                   v_int64;
    uint32_t                  v_argb32;
    int                       v_enum;
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
    WLMCFG_DESC_ENUM("enum", _test_value_t, v_enum, 3, _test_enum_desc),
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
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_int64(obj_ptr, &i64));
    BS_TEST_VERIFY_EQ(test_ptr, 42, i64);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("\"-1234\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_int64(obj_ptr, &i64));
    BS_TEST_VERIFY_EQ(test_ptr, -1234, i64);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("42");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmcfg_decode_uint64(obj_ptr, &u64));
    BS_TEST_VERIFY_EQ(test_ptr, 42, u64);
    wlmcfg_object_unref(obj_ptr);

    obj_ptr = wlmcfg_create_object_from_plist_string("\"-1234\"");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, obj_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmcfg_decode_uint64(obj_ptr, &u64));
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

/* == End of decode.c ====================================================== */
