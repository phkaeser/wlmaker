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

/** Type of the object. */
typedef enum {
    WLMCFG_STRING,
} wlmcfg_type_t;

/**
 * Destroys the object. Calls into the virtual dtor of the implementation.
 *
 * @param object_ptr
 */
void wlmcfg_object_destroy(wlmcfg_object_t *object_ptr);

/**
 * Creates a string object.
 *
 * @param value_ptr
 *
 * @return The string object, or NULL on error.
 */
wlmcfg_string_t *wlmcfg_string_create(const char *value_ptr);

/** Gets the superclass @ref wlmcfg_object_t from the string. */
wlmcfg_object_t *wlmcfg_object_from_string(wlmcfg_string_t *string_ptr);
/** Gets the @ref wlmcfg_string_t for `object_ptr`. NULL if not a string. */
wlmcfg_string_t *wlmcfg_string_from_object(wlmcfg_object_t *object_ptr);

/** Unit tests for the config data model. */
extern const bs_test_case_t wlmcfg_model_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_MODEL_H__ */
/* == End of model.h ================================================== */
