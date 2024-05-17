/* ========================================================================= */
/**
 * @file model.c
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

#include "model.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** Base type of a config object. */
struct _wlmcfg_object_t {
    /** Abstract virtual method: Destroys the object. */
    void (*destroy_fn)(wlmcfg_object_t *object_ptr);
};

/** Config object: A string. */
struct _wlmcfg_string_t {
    /** The string's superclass: An object. */
    wlmcfg_object_t           super_object;
    /** Value of the string. */
    char                      *value_ptr;
};

static bool _wlmcfg_object_init(
    wlmcfg_object_t *object_ptr,
    void (*destroy_fn)(wlmcfg_object_t *object_ptr));
static void _wlmcfg_string_destroy(wlmcfg_string_t *string_ptr);
static void _wlmcfg_string_object_destroy(wlmcfg_object_t *object_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmcfg_object_destroy(wlmcfg_object_t *object_ptr)
{
    if (NULL == object_ptr) return;
    object_ptr->destroy_fn(object_ptr);
}

/* ------------------------------------------------------------------------- */
wlmcfg_string_t *wlmcfg_string_create(const char *value_ptr)
{
    BS_ASSERT(NULL != value_ptr);
    wlmcfg_string_t *string_ptr = logged_calloc(1, sizeof(wlmcfg_string_t));
    if (NULL == string_ptr) return NULL;

    if (!_wlmcfg_object_init(&string_ptr->super_object,
                             _wlmcfg_string_object_destroy)) {
        _wlmcfg_string_destroy(string_ptr);
        return NULL;
    }

    string_ptr->value_ptr = logged_strdup(value_ptr);
    if (NULL == string_ptr->value_ptr) {
        _wlmcfg_string_destroy(string_ptr);
        return NULL;
    }

    return string_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes the object base class.
 *
 * @param object_ptr
 * @param destroy_fn
 *
 * @return true on success.
 */
bool _wlmcfg_object_init(wlmcfg_object_t *object_ptr,
                         void (*destroy_fn)(wlmcfg_object_t *object_ptr))
{
    BS_ASSERT(NULL != object_ptr);
    BS_ASSERT(NULL != destroy_fn);
    object_ptr->destroy_fn = destroy_fn;
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Dtor for the string object.
 *
 * @param string_ptr
 */
void _wlmcfg_string_destroy(wlmcfg_string_t *string_ptr)
{
    if (NULL == string_ptr) return;

    if (NULL != string_ptr->value_ptr) {
        free(string_ptr->value_ptr);
        string_ptr->value_ptr = NULL;
    }

    free(string_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implements the string's "object" superclass virtual dtor. Calls into
 * @ref _wlmcfg_string_destroy.
 *
 * @param object_ptr
 */
void _wlmcfg_string_object_destroy(wlmcfg_object_t *object_ptr)
{
    wlmcfg_string_t *string_ptr = BS_CONTAINER_OF(
        object_ptr, wlmcfg_string_t, super_object);
    _wlmcfg_string_destroy(string_ptr);
}

/* == End of model.c ======================================================= */
