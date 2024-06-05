/* ========================================================================= */
/**
 * @file decode.h
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
#ifndef __WLMCFG_DECODE_H__
#define __WLMCFG_DECODE_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Descriptor. */
typedef struct _wlmcfg_desc_t wlmcfg_desc_t;

/** Enum descriptor. */
typedef struct {
    /** The string representation of the enum. */
    const char                *name_ptr;
    /** The corresponding numeric value. */
    int                       value;
} wlmcfg_enum_desc_t;

/** Sentinel for an enum descriptor sequence. */
#define WLMCFG_ENUM_SENTINEL() { .name_ptr = NULL }

/** Helper to define an enum descriptor. */
#define WLMCFG_ENUM(_name, _value) { .name_ptr = (_name), .value = (_value) }

/** Supported types to decode from a plist dict. */
typedef enum {
    WLMCFG_TYPE_UINT64,
    WLMCFG_TYPE_INT64,
    WLMCFG_TYPE_ARGB32,
    WLMCFG_TYPE_BOOL,
    WLMCFG_TYPE_ENUM,
    WLMCFG_TYPE_STRING,
    WLMCFG_TYPE_DICT,
    WLMCFG_TYPE_CUSTOM,
} wlmcfg_decode_type_t;

/** A signed 64-bit integer. */
typedef struct {
    /** The default value, if not in the dict. */
    int64_t                   default_value;
} wlmcfg_desc_int64_t;

/** An unsigned 64-bit integer. */
typedef struct {
    /** The default value, if not in the dict. */
    uint64_t                   default_value;
} wlmcfg_desc_uint64_t;

/** A color, encoded as string in format 'argb32:aarrggbb'. */
typedef struct {
    /** The default value, if not in the dict. */
    uint32_t                   default_value;
} wlmcfg_desc_argb32_t;

/** A boolean value. */
typedef struct {
    /** The default value, if not in the dict. */
    bool                      default_value;
} wlmcfg_desc_bool_t;

/** An enum. */
typedef struct {
    /** The default value, if not in the dict. */
    int                        default_value;
    /** The enum descriptor. */
    const wlmcfg_enum_desc_t   *desc_ptr;
} wlmcfg_desc_enum_t;

/** A string. Will be (re)created and must be free'd. */
typedef struct {
    /** The default value, if not in the dict. */
    const char                *default_value_ptr;
} wlmcfg_desc_string_t;

/** A custom decoder. */
typedef struct {
    /** Decoding method: From obhect into `dest_ptr`. */
    bool (*decode)(wlmcfg_object_t *obj_ptr, void *dest_ptr);
    /** Initializer method: Allocate or prepare `dest_ptr`. May be NULL. */
    bool (*init)(void *dest_ptr);
    /** Cleanup method: Frees `dest_ptr`. May be NULL.. */
    void (*fini)(void *dest_ptr);
} wlmcfg_desc_custom_t;

/** Descriptor to decode a plist dict. */
struct _wlmcfg_desc_t {
    /** Type of the value. */
    wlmcfg_decode_type_t      type;
    /** The key used for the described value in the plist dict. */
    const char                *key_ptr;
    /** Whether the field is required. */
    bool                      required;
    /** Offset of the field where to store the value. */
    size_t                    field_offset;
    /** And the descriptor of the value. */
    union {
        wlmcfg_desc_int64_t   v_int64;
        wlmcfg_desc_uint64_t  v_uint64;
        wlmcfg_desc_argb32_t  v_argb32;
        wlmcfg_desc_bool_t    v_bool;
        wlmcfg_desc_enum_t    v_enum;
        wlmcfg_desc_string_t  v_string;
        const wlmcfg_desc_t   *v_dict_desc_ptr;
        wlmcfg_desc_custom_t  v_custom;
    } v;
};

/** Descriptor sentinel. Put at end of a @ref wlmcfg_desc_t sequence. */
#define WLMCFG_DESC_SENTINEL() { .key_ptr = NULL }

/** Descriptor for an unsigned int64. */
#define WLMCFG_DESC_UINT64(_key, _required, _base, _field, _default) {  \
        .type = WLMCFG_TYPE_UINT64,                                     \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_uint64.default_value = _default                            \
    }

/** Descriptor for an signed int64. */
#define WLMCFG_DESC_INT64(_key, _required, _base, _field, _default) {   \
        .type = WLMCFG_TYPE_INT64,                                      \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_int64.default_value = _default                             \
    }

/** Descriptor for an ARGB32 value. */
#define WLMCFG_DESC_ARGB32(_key, _required, _base, _field, _default) {  \
        .type = WLMCFG_TYPE_ARGB32,                                     \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_argb32.default_value = _default                            \
    }

/** Descriptor for a bool value. */
#define WLMCFG_DESC_BOOL(_key, _required, _base, _field, _default) {    \
        .type = WLMCFG_TYPE_BOOL,                                       \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_bool.default_value = _default                              \
    }

/** Descriptor for an enum value. */
#define WLMCFG_DESC_ENUM(_key, _required, _base, _field, _default, _desc_ptr) \
    {                                                                   \
        .type = WLMCFG_TYPE_ENUM,                                       \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_enum.default_value = _default,                             \
        .v.v_enum.desc_ptr = _desc_ptr                                  \
    }

/** Descriptor for a string value value. */
#define WLMCFG_DESC_STRING(_key, _required, _base, _field, _default) {  \
        .type = WLMCFG_TYPE_STRING,                                     \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_string.default_value_ptr = _default,                       \
    }

/** Descriptor for a dict sub-value. */
#define WLMCFG_DESC_DICT(_key, _required, _base, _field, _desc) {       \
        .type = WLMCFG_TYPE_DICT,                                       \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_dict_desc_ptr = _desc                                      \
    }

/** Descriptor for a custom object decoder. */
#define WLMCFG_DESC_CUSTOM(_key, _required, _base, _field, _d ,_i, _f) { \
        .type = WLMCFG_TYPE_CUSTOM,                                     \
        .key_ptr = (_key),                                              \
        .required = _required,                                          \
        .field_offset = offsetof(_base, _field),                        \
        .v.v_custom.decode = _d,                                        \
        .v.v_custom.init = _i,                                          \
        .v.v_custom.fini = _f,                                          \
    }

/**
 * Decodes the plist `dict_ptr` into `dest_ptr` as described.
 *
 * @param dict_ptr
 * @param desc_ptr
 * @param dest_ptr
 *
 * @return true on success.
 */
bool wlmcfg_decode_dict(
    wlmcfg_dict_t *dict_ptr,
    const wlmcfg_desc_t *desc_ptr,
    void *dest_ptr);

/**
 * Destroys resources that were allocated during @ref wlmcfg_decode_dict.
 *
 * @param desc_ptr
 * @param dest_ptr
 */
void wlmcfg_decoded_destroy(
    const wlmcfg_desc_t *desc_ptr,
    void *dest_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmcfg_decode_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_DECODE_H__ */
/* == End of decode.h ====================================================== */
