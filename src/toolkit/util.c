/* ========================================================================= */
/**
 * @file util.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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

#include <toolkit/util.h>

/* == Declarations ========================================================= */

static void _wlmtk_util_test_listener_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_util_wl_list_for_each(
    struct wl_list *list_ptr,
    bool (*func)(struct wl_list *link_ptr, void *ud_ptr),
    void *ud_ptr)
{
    bool rv = true;

    if (NULL != list_ptr && NULL != list_ptr->next) {
        struct wl_list *link_ptr, *next_link_ptr;
        for (link_ptr = list_ptr->next, next_link_ptr = link_ptr->next;
             link_ptr != list_ptr;
             link_ptr = next_link_ptr, next_link_ptr = next_link_ptr->next) {
            if (!func(link_ptr, ud_ptr)) rv = false;
        }
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
void wlmtk_util_connect_listener_signal(
    struct wl_signal *signal_ptr,
    struct wl_listener *listener_ptr,
    void (*notifier_func)(struct wl_listener *, void *))
{
    listener_ptr->notify = notifier_func;
    wl_signal_add(signal_ptr, listener_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_util_disconnect_listener(
    struct wl_listener *listener_ptr)
{
    // Guard clause: No disconnect if it hadn't been connected.
    if (NULL == listener_ptr || NULL == listener_ptr->link.prev) return;

    wl_list_remove(&listener_ptr->link);
}

/* ------------------------------------------------------------------------- */
void wlmtk_util_connect_test_listener(
    struct wl_signal *signal_ptr,
    wlmtk_util_test_listener_t *test_listener_ptr)
{
    wlmtk_util_connect_listener_signal(
        signal_ptr,
        &test_listener_ptr->listener,
        _wlmtk_util_test_listener_handler);
    wlmtk_util_clear_test_listener(test_listener_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_util_disconnect_test_listener(
    wlmtk_util_test_listener_t *test_listener_ptr)
{
    wlmtk_util_disconnect_listener(&test_listener_ptr->listener);
}

/* ------------------------------------------------------------------------- */
void wlmtk_util_clear_test_listener(
    wlmtk_util_test_listener_t *test_listener_ptr)
{
    test_listener_ptr->calls = 0;
    test_listener_ptr->last_data_ptr = NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler to record a signal call into the @ref wlmtk_util_test_listener_t.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_util_test_listener_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_util_test_listener_t *test_listener_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_util_test_listener_t, listener);
    ++test_listener_ptr->calls;
    test_listener_ptr->last_data_ptr = data_ptr;
}

/* == Unit tests =========================================================== */

static void test_wl_list_for_each(bs_test_t *test_ptr);
static void test_listener(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_util_test_cases[] = {
    { 1, "wl_list_for_each", test_wl_list_for_each },
    { 1, "listener", test_listener },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** For tests: a list item. */
typedef struct {
    struct wl_list link;  /*!< list node. */
    bool called;  /*!< reports calls by @ref _test_wl_list_callback. */
    bool rv;  /*!< what to return. */
} test_wl_list_it_t;

/** Callback for testing @ref wlmtk_util_wl_list_for_each. */
static bool _test_wl_list_callback(
    struct wl_list *link_ptr, void *ud_ptr)
{
    test_wl_list_it_t *t = BS_CONTAINER_OF(link_ptr, test_wl_list_it_t, link);
    t->called = true;
    *((size_t*)ud_ptr) += 1;
    return t->rv;
}

/** Tests @ref wlmtk_util_wl_list_for_each. */
static void test_wl_list_for_each(bs_test_t *test_ptr)
{
    test_wl_list_it_t t1 = { .rv = true }, t2 = { .rv = true };
    size_t calls = 0;

    struct wl_list l = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 0, calls);

    wl_list_init(&l);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 0, calls);

    wl_list_insert(&l, &t1.link);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 1, calls);
    calls = 0;
    BS_TEST_VERIFY_TRUE(test_ptr, t1.called);
    t1.called = false;

    wl_list_insert(&l, &t2.link);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 2, calls);
    calls = 0;
    BS_TEST_VERIFY_TRUE(test_ptr, t1.called);
    t1.called = false;
    BS_TEST_VERIFY_TRUE(test_ptr, t2.called);
    t2.called = false;

    t1.rv = false;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 2, calls);
    calls = 0;

    t2.rv = false;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_util_wl_list_for_each(&l, _test_wl_list_callback, &calls));
    BS_TEST_VERIFY_EQ(test_ptr, 2, calls);
}

/* ------------------------------------------------------------------------- */
/** A test to verify listener handlers are called in order of subscription. */
static void test_listener(bs_test_t *test_ptr)
{
    struct wl_signal signal;
    wlmtk_util_test_listener_t l1 = {}, l2 = {};

    wl_signal_init(&signal);

    // First test: disconnect must not crash.
    wlmtk_util_disconnect_listener(&l1.listener);

    // Second test: Connect, and verify signal is emitted and handled.
    wlmtk_util_connect_test_listener(&signal, &l1);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l1.calls);
    wl_signal_emit(&signal, test_listener);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l1.calls);
    BS_TEST_VERIFY_EQ(test_ptr, test_listener, l1.last_data_ptr);

    // Third test: One more listener, and verify both handlers aacted.
    wlmtk_util_connect_test_listener(&signal, &l2);
    wl_signal_emit(&signal, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 2, l1.calls);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l2.calls);

    // Cleanup.
    wlmtk_util_disconnect_test_listener(&l2);
    wlmtk_util_disconnect_test_listener(&l1);
}

/* == End of util.c ======================================================== */
