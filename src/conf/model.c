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
    /** References held to this object. */
    int references;
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
    /** The dict's superclass: An object. */
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

/** Array object. A vector of pointers to the objects. */
struct _wlmcfg_array_t {
    /** The array's superclass: An object. */
    wlmcfg_object_t           super_object;
    /** Vector holding pointers to each object. */
    bs_ptr_vector_t           object_vector;
};

static bool _wlmcfg_object_init(
    wlmcfg_object_t *object_ptr,
    wlmcfg_type_t type,
    void (*destroy_fn)(wlmcfg_object_t *object_ptr));

static void _wlmcfg_string_object_destroy(wlmcfg_object_t *object_ptr);
static void _wlmcfg_dict_object_destroy(wlmcfg_object_t *object_ptr);
static void _wlmcfg_array_object_destroy(wlmcfg_object_t *object_ptr);

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
wlmcfg_object_t *wlmcfg_object_dup(wlmcfg_object_t *object_ptr)
{
    ++object_ptr->references;
    return object_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmcfg_object_destroy(wlmcfg_object_t *object_ptr)
{
    if (NULL == object_ptr) return;

    // Check references. Warn on potential double-free.
    BS_ASSERT(0 < object_ptr->references);
    if (0 < --object_ptr->references) return;

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
        free(string_ptr);
        return NULL;
    }

    string_ptr->value_ptr = logged_strdup(value_ptr);
    if (NULL == string_ptr->value_ptr) {
        _wlmcfg_string_object_destroy(wlmcfg_object_from_string(string_ptr));
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
        free(dict_ptr);
        return NULL;
    }

    dict_ptr->tree_ptr = bs_avltree_create(
        _wlmcfg_dict_item_node_cmp,
        _wlmcfg_dict_item_node_destroy);
    if (NULL == dict_ptr->tree_ptr) {
        _wlmcfg_dict_object_destroy(wlmcfg_object_from_dict(dict_ptr));
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


/* == Array methods ======================================================== */

/* ------------------------------------------------------------------------- */
wlmcfg_array_t *wlmcfg_array_create(void)
{
    wlmcfg_array_t *array_ptr = logged_calloc(1, sizeof(wlmcfg_array_t));
    if (NULL == array_ptr) return NULL;

    if (!_wlmcfg_object_init(&array_ptr->super_object,
                             WLMCFG_ARRAY,
                             _wlmcfg_array_object_destroy)) {
        free(array_ptr);
        return NULL;
    }

    if (!bs_ptr_vector_init(&array_ptr->object_vector)) {
        _wlmcfg_array_object_destroy(wlmcfg_object_from_array(array_ptr));
        return NULL;
    }
    return array_ptr;
}

/* ------------------------------------------------------------------------- */
bool wlmcfg_array_push_back(
    wlmcfg_array_t *array_ptr,
    wlmcfg_object_t *object_ptr)
{
    wlmcfg_object_t *new_object_ptr = wlmcfg_object_dup(object_ptr);
    if (NULL == new_object_ptr) return false;

    return bs_ptr_vector_push_back(&array_ptr->object_vector, new_object_ptr);
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_array_at(
    wlmcfg_array_t *array_ptr,
    size_t index)
{
    return bs_ptr_vector_at(&array_ptr->object_vector, index);
}

/* ------------------------------------------------------------------------- */
wlmcfg_object_t *wlmcfg_object_from_array(wlmcfg_array_t *array_ptr)
{
    return &array_ptr->super_object;
}

/* ------------------------------------------------------------------------- */
wlmcfg_array_t *wlmcfg_array_from_object(wlmcfg_object_t *object_ptr)
{
    return BS_CONTAINER_OF(object_ptr, wlmcfg_array_t, super_object);
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
    object_ptr->references = 1;
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmcfg_object_t::destroy_fn. Destroys the string.
 *
 * @param object_ptr
 */
void _wlmcfg_string_object_destroy(wlmcfg_object_t *object_ptr)
{
    wlmcfg_string_t *string_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_string_from_object(object_ptr));

    if (NULL != string_ptr->value_ptr) {
        free(string_ptr->value_ptr);
        string_ptr->value_ptr = NULL;
    }

    free(string_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmcfg_object_t::destroy_fn. Destroys the dict,
 * including all contained elements.
 *
 * @param object_ptr
 */
void _wlmcfg_dict_object_destroy(wlmcfg_object_t *object_ptr)
{
    wlmcfg_dict_t *dict_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_dict_from_object(object_ptr));

    if (NULL != dict_ptr->tree_ptr) {
        bs_avltree_destroy(dict_ptr->tree_ptr);
        dict_ptr->tree_ptr = NULL;
    }
    free(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Ctor: Creates a dict item. Copies the key, and duplicates the object. */
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

    item_ptr->value_object_ptr = wlmcfg_object_dup(object_ptr);
    if (NULL == item_ptr->value_object_ptr) {
        _wlmcfg_dict_item_destroy(item_ptr);
        return NULL;
    }
    return item_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor: Destroys the dict item. */
void _wlmcfg_dict_item_destroy(wlmcfg_dict_item_t *item_ptr)
{
    if (NULL != item_ptr->value_object_ptr) {
        wlmcfg_object_destroy(item_ptr->value_object_ptr);
        item_ptr->value_object_ptr = NULL;
    }
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

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmcfg_object_t::destroy_fn. Destroys the array,
 * including all contained elements.
 *
 * @param object_ptr
 */
void _wlmcfg_array_object_destroy(wlmcfg_object_t *object_ptr)
{
    wlmcfg_array_t *array_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_array_from_object(object_ptr));

    while (0 < bs_ptr_vector_size(&array_ptr->object_vector)) {
        wlmcfg_object_t *object_ptr = bs_ptr_vector_at(
            &array_ptr->object_vector,
            bs_ptr_vector_size(&array_ptr->object_vector) - 1);
        bs_ptr_vector_erase(
            &array_ptr->object_vector,
            bs_ptr_vector_size(&array_ptr->object_vector) - 1);
        wlmcfg_object_destroy(object_ptr);
    }

    bs_ptr_vector_fini(&array_ptr->object_vector);

    free(array_ptr);
}

/* == Unit tests =========================================================== */

static void test_string(bs_test_t *test_ptr);
static void test_dict(bs_test_t *test_ptr);
static void test_array(bs_test_t *test_ptr);

const bs_test_case_t wlmcfg_model_test_cases[] = {
    { 1, "string", test_string },
    { 1, "dict", test_dict },
    { 1, "array", test_array },
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
    wlmcfg_object_destroy(obj0_ptr);

    wlmcfg_object_t *obj1_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_object_from_string(wlmcfg_string_create("val1")));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmcfg_dict_add(dict_ptr, "key0", obj1_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_dict_add(dict_ptr, "key1", obj1_ptr));
    wlmcfg_object_destroy(obj1_ptr);

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

/* ------------------------------------------------------------------------- */
/** Tests the wlmcfg_array_t methods. */
void test_array(bs_test_t *test_ptr)
{
    wlmcfg_array_t *array_ptr = wlmcfg_array_create();

    wlmcfg_object_t *obj0_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_object_from_string(wlmcfg_string_create("val0")));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_array_push_back(array_ptr, obj0_ptr));
    wlmcfg_object_destroy(obj0_ptr);

    wlmcfg_object_t *obj1_ptr = BS_ASSERT_NOTNULL(
        wlmcfg_object_from_string(wlmcfg_string_create("val1")));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmcfg_array_push_back(array_ptr, obj1_ptr));
    wlmcfg_object_destroy(obj1_ptr);

    BS_TEST_VERIFY_EQ(test_ptr, obj0_ptr, wlmcfg_array_at(array_ptr, 0));
    BS_TEST_VERIFY_EQ(test_ptr, obj1_ptr, wlmcfg_array_at(array_ptr, 1));

    wlmcfg_object_destroy(wlmcfg_object_from_array(array_ptr));
}

/* == End of model.c ======================================================= */
