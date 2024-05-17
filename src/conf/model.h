/* ========================================================================= */
/**
 * @file model.h
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
#ifndef __WLMCFG_MODEL_H__
#define __WLMCFG_MODEL_H__

#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Base object type. */
typedef struct _wlmcfg_object_t wlmcfg_object_t;
/** Forward declaration: A string. */
typedef struct _wlmcfg_string_t wlmcfg_string_t;
/** Forward declaration: A dict (key/value store for objects). */
typedef struct _wlmcfg_dict_t wlmcfg_dict_t;

/** Type of the object. */
typedef enum {
    WLMCFG_STRING,
    WLMCFG_DICT
} wlmcfg_type_t;

/**
 * Duplicates the object. Technically, just increases the reference count.
 *
 * @param object_ptr
 *
 * @return The "duplicated" object. The caller should not assume that the
 *     return value is still object_ptr.
 */
wlmcfg_object_t *wlmcfg_object_dup(wlmcfg_object_t *object_ptr);

/**
 * Destroys the object. Calls into the virtual dtor of the implementation.
 *
 * Technically, decreases the reference count and destroys only if there is
 * no further reference held.
 *
 * @param object_ptr
 */
void wlmcfg_object_destroy(wlmcfg_object_t *object_ptr);

/* -- String methods ------------------------------------------------------- */
/**
 * Creates a string object.
 *
 * @param value_ptr
 *
 * @return The string object, or NULL on error.
 */
wlmcfg_string_t *wlmcfg_string_create(const char *value_ptr);

/**
 * Returns the value of the string.
 *
 * @param string_ptr
 *
 * @return Pointer to the string's value.
 */
const char *wlmcfg_string_value(const wlmcfg_string_t *string_ptr);

/** Gets the superclass @ref wlmcfg_object_t from the string. */
wlmcfg_object_t *wlmcfg_object_from_string(wlmcfg_string_t *string_ptr);
/** Gets the @ref wlmcfg_string_t for `object_ptr`. NULL if not a string. */
wlmcfg_string_t *wlmcfg_string_from_object(wlmcfg_object_t *object_ptr);

/* -- Dict methods --------------------------------------------------------- */

/**
 * Creates a dict object.
 *
 * @return The dict object, or NULL on error.
 */
wlmcfg_dict_t *wlmcfg_dict_create(void);

/**
 * Adds an object to the dict.
 *
 * @param dict_ptr
 * @param key_ptr
 * @param object_ptr          The object to add. It will be duplicated by
 *                            calling @ref wlmcfg_object_dup.
 *
 * @return true on success. Adding the object can fail if the key already
 *     exists, or if memory could not get allocated.
 */
bool wlmcfg_dict_add(
    wlmcfg_dict_t *dict_ptr,
    const char *key_ptr,
    wlmcfg_object_t *object_ptr);

/** @return the given object from the dict. */
wlmcfg_object_t *wlmcfg_dict_get(
    wlmcfg_dict_t *dict_ptr,
     const char *key_ptr);

/** Gets the superclass @ref wlmcfg_object_t from the dict. */
wlmcfg_object_t *wlmcfg_object_from_dict(wlmcfg_dict_t *dict_ptr);
/** Gets the @ref wlmcfg_dict_t for `object_ptr`. NULL if not a dict. */
wlmcfg_dict_t *wlmcfg_dict_from_object(wlmcfg_object_t *object_ptr);

/** Unit tests for the config data model. */
extern const bs_test_case_t wlmcfg_model_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_MODEL_H__ */
/* == End of model.h ================================================== */
