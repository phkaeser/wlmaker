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

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmcfg_decode_dict(
    __UNUSED__ wlmcfg_dict_t *dict_ptr,
    const wlmcfg_desc_t *desc_ptr,
    __UNUSED__ void *dest_ptr)
{
    for (const wlmcfg_desc_t *iter_desc_ptr = desc_ptr;
         iter_desc_ptr->key_ptr != NULL;
         ++iter_desc_ptr) {
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

/* == Unit tests =========================================================== */

static void test_defaults(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_decode_test_cases[] = {
    { 1, "defaults", test_defaults },
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
void test_defaults(bs_test_t *test_ptr)
{
    _test_value_t val = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        _wlmcfg_init_defaults(_wlmcfg_decode_test_desc, &val));
    BS_TEST_VERIFY_EQ(test_ptr, 1234, val.v_uint64);
    BS_TEST_VERIFY_EQ(test_ptr, -1234, val.v_int64);
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, val.v_argb32);
}

/* == End of decode.c ====================================================== */
