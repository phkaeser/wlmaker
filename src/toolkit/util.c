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

#include "util.h"

/* == Exported methods ===================================================== */

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

/* == Unit tests =========================================================== */

static void test_listener(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_util_test_cases[] = {
    { 1, "listener", test_listener },
    { 0, NULL, NULL }
};

/** Struct for testing listener code. */
typedef struct {
    /** Listener. */
    struct wl_listener        listener;
    /** Data. */
    int                       data;
} _wlmtk_util_listener;

static void _wlmtk_util_listener_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* ------------------------------------------------------------------------- */
/** A test to verify listener handlers are called in order of subscription. */
static void test_listener(bs_test_t *test_ptr)
{
    struct wl_signal signal;
    _wlmtk_util_listener l1 = {}, l2 = {};
    int i = 0;

    wl_signal_init(&signal);

    // First test: disconnect must not crash.
    wlmtk_util_disconnect_listener(&l1.listener);

    // Second test: Connect, and verify signal is emitted and handled.
    wlmtk_util_connect_listener_signal(
        &signal, &l1.listener, _wlmtk_util_listener_handler);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l1.data);
    wl_signal_emit(&signal, &i);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l1.data);

    // Third test: One more listener, and verify both handlers aacted.
    wlmtk_util_connect_listener_signal(
        &signal, &l2.listener, _wlmtk_util_listener_handler);
    wl_signal_emit(&signal, &i);
    BS_TEST_VERIFY_EQ(test_ptr, 2, l1.data);
    BS_TEST_VERIFY_EQ(test_ptr, 3, l2.data);

    // Cleanup.
    wlmtk_util_disconnect_listener(&l2.listener);
    wlmtk_util_disconnect_listener(&l1.listener);
}

/** Test handler for the listener. */
void _wlmtk_util_listener_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    _wlmtk_util_listener *wlmtk_util_listener_ptr = BS_CONTAINER_OF(
        listener_ptr, _wlmtk_util_listener, listener);
    int *i_ptr = data_ptr;
    wlmtk_util_listener_ptr->data = ++(*i_ptr);
}

/* == End of util.c ======================================================== */
