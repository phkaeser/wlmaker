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
    /** Type of this object. */
    wlmcfg_type_t type;
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

/** Config object: A dict. */
struct _wlmcfg_dict_t {
    /** The string's superclass: An object. */
    wlmcfg_object_t           super_object;
    /** Implemented as a tree. Benefit: Keeps sorting order. */
    bs_avltree_t              *tree_ptr;
};

/** An item in the dict. Internal class. */
typedef struct {
    /** Node in the tree. */
    bs_avltree_node_t         avlnode;
    /** The lookup key. */
    char                      *key_ptr;
    /** The value, is an object. */
    wlmcfg_object_t           *value_object_ptr;
} wlmcfg_dict_item_t;

static bool _wlmcfg_object_init(
    wlmcfg_object_t *object_ptr,
    wlmcfg_type_t type,
    void (*destroy_fn)(wlmcfg_object_t *object_ptr));
static void _wlmcfg_string_destroy(wlmcfg_string_t *string_ptr);
static void _wlmcfg_string_object_destroy(wlmcfg_object_t *object_ptr);


static void _wlmcfg_dict_object_destroy(wlmcfg_object_t *object_ptr);
static void _wlmcfg_dict_destroy(wlmcfg_dict_t *dict_ptr);

static wlmcfg_dict_item_t *_wlmcfg_dict_item_create(
    const char *key_ptr,
    wlmcfg_object_t *object_ptr);
static void _wlmcfg_dict_item_destroy(
    wlmcfg_dict_item_t *item_ptr);

static int _wlmcfg_dict_item_node_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);
static void _wlmcfg_dict_item_node_destroy(
    bs_avltree_node_t *node_ptr);

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
                             WLMCFG_STRING,
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

/* ------------------------------------------------------------------------- */
const char *wlmcfg_string_value(const wlmcfg_string_t *string_ptr)
{
    return string_ptr->value_ptr;
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_object_from_string(wlmcfg_string_t *string_ptr)
{
    return &string_ptr->super_object;
}

/* ------------------------------------------------------------------------- */
wlmcfg_string_t *wlmcfg_string_from_object(wlmcfg_object_t *object_ptr)
{
    if (WLMCFG_STRING != object_ptr->type) return NULL;
    return BS_CONTAINER_OF(object_ptr, wlmcfg_string_t, super_object);
}

/* ------------------------------------------------------------------------- */
wlmcfg_dict_t *wlmcfg_dict_create(void)
{
    wlmcfg_dict_t *dict_ptr = logged_calloc(1, sizeof(wlmcfg_dict_t));
    if (NULL == dict_ptr) return NULL;

    if (!_wlmcfg_object_init(&dict_ptr->super_object,
                             WLMCFG_DICT,
                             _wlmcfg_dict_object_destroy)) {
        _wlmcfg_dict_destroy(dict_ptr);
        return NULL;
    }

    dict_ptr->tree_ptr = bs_avltree_create(
        _wlmcfg_dict_item_node_cmp,
        _wlmcfg_dict_item_node_destroy);
    if (NULL == dict_ptr->tree_ptr) {
        _wlmcfg_dict_destroy(dict_ptr);
        return NULL;
    }

    return dict_ptr;
}

/* ------------------------------------------------------------------------- */
bool wlmcfg_dict_add(
    wlmcfg_dict_t *dict_ptr,
    const char *key_ptr,
    wlmcfg_object_t *object_ptr)
{
    wlmcfg_dict_item_t *item_ptr = _wlmcfg_dict_item_create(
        key_ptr, object_ptr);
    if (NULL == item_ptr) return false;

    bool rv = bs_avltree_insert(
        dict_ptr->tree_ptr, item_ptr->key_ptr, &item_ptr->avlnode, false);
    if (!rv) _wlmcfg_dict_item_destroy(item_ptr);
    return rv;
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_dict_get(
    wlmcfg_dict_t *dict_ptr,
    const char *key_ptr)
{
    bs_avltree_node_t *node_ptr = bs_avltree_lookup(
        dict_ptr->tree_ptr, key_ptr);
    if (NULL == node_ptr) return NULL;

    wlmcfg_dict_item_t *item_ptr = BS_CONTAINER_OF(
        node_ptr, wlmcfg_dict_item_t, avlnode);
    return item_ptr->value_object_ptr;
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_object_from_dict(wlmcfg_dict_t *dict_ptr)
{
    return &dict_ptr->super_object;
}

/* ------------------------------------------------------------------------- */
wlmcfg_dict_t *wlmcfg_dict_from_object(wlmcfg_object_t *object_ptr)
{
    return BS_CONTAINER_OF(object_ptr, wlmcfg_dict_t, super_object);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes the object base class.
 *
 * @param object_ptr
 * @param type
 * @param destroy_fn
 *
 * @return true on success.
 */
bool _wlmcfg_object_init(
    wlmcfg_object_t *object_ptr,
    wlmcfg_type_t type,
    void (*destroy_fn)(wlmcfg_object_t *object_ptr))
{
    BS_ASSERT(NULL != object_ptr);
    BS_ASSERT(NULL != destroy_fn);
    object_ptr->type = type;
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
    wlmcfg_string_t *string_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_string_from_object(object_ptr));
    _wlmcfg_string_destroy(string_ptr);
}

/* == Dict methods ========================================================= */

/* ------------------------------------------------------------------------- */
/** Implementation of the superclass' virtual dtor: Calls into dtor. */
void _wlmcfg_dict_object_destroy(wlmcfg_object_t *object_ptr)
{
    _wlmcfg_dict_destroy(wlmcfg_dict_from_object(object_ptr));
}

/* ------------------------------------------------------------------------- */
/** Dtor: Destroys the dict and all still-contained items. */
void _wlmcfg_dict_destroy(wlmcfg_dict_t *dict_ptr)
{
    if (NULL != dict_ptr->tree_ptr) {
        bs_avltree_destroy(dict_ptr->tree_ptr);
        dict_ptr->tree_ptr = NULL;
    }
    free(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Ctor: Creates a dict item. Copies the key, but not the object. */
wlmcfg_dict_item_t *_wlmcfg_dict_item_create(
    const char *key_ptr,
    wlmcfg_object_t *object_ptr)
{
    wlmcfg_dict_item_t *item_ptr = logged_calloc(
        1, sizeof(wlmcfg_dict_item_t));
    if (NULL == item_ptr) return NULL;

    item_ptr->key_ptr = logged_strdup(key_ptr);
    if (NULL == item_ptr->key_ptr) {
        _wlmcfg_dict_item_destroy(item_ptr);
        return NULL;
    }

    item_ptr->value_object_ptr = object_ptr;
    return item_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor: Destroys the dict item. */
void _wlmcfg_dict_item_destroy(wlmcfg_dict_item_t *item_ptr)
{
    if (NULL != item_ptr->key_ptr) {
        free(item_ptr->key_ptr);
        item_ptr->key_ptr = NULL;
    }
    free(item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for the dictionary item with another key. */
int _wlmcfg_dict_item_node_cmp(const bs_avltree_node_t *node_ptr,
                               const void *key_ptr)
{
    wlmcfg_dict_item_t *item_ptr = BS_CONTAINER_OF(
        node_ptr, wlmcfg_dict_item_t, avlnode);
    return strcmp(item_ptr->key_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destroy dict item by avlnode. Forward to @ref _wlmcfg_dict_item_destroy. */
void _wlmcfg_dict_item_node_destroy(bs_avltree_node_t *node_ptr)
{
    wlmcfg_dict_item_t *item_ptr = BS_CONTAINER_OF(
        node_ptr, wlmcfg_dict_item_t, avlnode);
    _wlmcfg_dict_item_destroy(item_ptr);
}

/* == Unit tests =========================================================== */

static void test_string(bs_test_t *test_ptr);
static void test_dict(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_model_test_cases[] = {
    { 1, "string", test_string },
    { 1, "dict", test_dict },
    { 0, NULL, NULL }
};


/* ------------------------------------------------------------------------- */
/** Tests the wlmcfg_string_t methods. */
void test_string(bs_test_t *test_ptr)
{
    wlmcfg_string_t *string_ptr;

    string_ptr = wlmcfg_string_create("a test");
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, string_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "a test", string_ptr->value_ptr);

    wlmcfg_object_t *object_ptr = wlmcfg_object_from_string(string_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        string_ptr,
        wlmcfg_string_from_object(object_ptr));

    wlmcfg_object_destroy(object_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the wlmcfg_dict_t methods. */
void test_dict(bs_test_t *test_ptr)
{
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_create();

    wlmcfg_object_t *obj0_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_object_from_string(wlmcfg_string_create("val0")));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_dict_add(dict_ptr, "key0", obj0_ptr));

    wlmcfg_object_t *obj1_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_object_from_string(wlmcfg_string_create("val1")));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmcfg_dict_add(dict_ptr, "key0", obj1_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_dict_add(dict_ptr, "key1", obj1_ptr));

    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "val0",
        wlmcfg_string_value(wlmcfg_string_from_object(
                                wlmcfg_dict_get(dict_ptr, "key0"))));
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "val1",
        wlmcfg_string_value(wlmcfg_string_from_object(
                                wlmcfg_dict_get(dict_ptr, "key1"))));

    wlmcfg_object_destroy(wlmcfg_object_from_dict(dict_ptr));
}

/* == End of model.c ======================================================= */
