/* ========================================================================= */
/**
 * @file toplevel.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "toplevel.h"

#include <libbase/libbase.h>
#include <stdlib.h>

/* == Declarations ========================================================= */

/** Representation of a ext_foreign_toplevel_handle_v1. */
struct wlmdock_toplevel_handle {
    /** Optional: app id of the toplevel. The app ID may change. */
    char                      *app_id_ptr;
    /** Optional: Title of the toplevel. The title may change. */
    char                      *title_ptr;
    /** Identifier. If set, it must be unique per toplevel. Must not change. */
    char                      *identifier_ptr;
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmdock_toplevel_handle *wlmdock_toplevel_handle_create(void)
{
    struct wlmdock_toplevel_handle *handle_ptr = logged_calloc(
        1, sizeof(*handle_ptr));
    if (NULL == handle_ptr) return NULL;

    return handle_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_toplevel_handle_destroy(
    struct wlmdock_toplevel_handle *handle_ptr)
{
    if (NULL != handle_ptr->app_id_ptr) {
        free(handle_ptr->app_id_ptr);
        handle_ptr->app_id_ptr = NULL;
    }
    if (NULL != handle_ptr->title_ptr) {
        free(handle_ptr->title_ptr);
        handle_ptr->title_ptr = NULL;
    }
    if (NULL != handle_ptr->identifier_ptr) {
        free(handle_ptr->identifier_ptr);
        handle_ptr->identifier_ptr = NULL;
    }

    free(handle_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_handle_set_app_id(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *app_id_ptr)
{
    if (NULL != handle_ptr->app_id_ptr) {
        free(handle_ptr->app_id_ptr);
        handle_ptr->app_id_ptr = NULL;
    }
    if (NULL == app_id_ptr) return true;

    handle_ptr->app_id_ptr = logged_strdup(app_id_ptr);
    return (NULL != handle_ptr->app_id_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_handle_set_title(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *title_ptr)
{
    if (NULL != handle_ptr->title_ptr) {
        free(handle_ptr->title_ptr);
        handle_ptr->title_ptr = NULL;
    }
    if (NULL == title_ptr) return true;

    handle_ptr->title_ptr = logged_strdup(title_ptr);
    return (NULL != handle_ptr->title_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_handle_set_identifier(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *identifier_ptr)
{
    BS_ASSERT(NULL != identifier_ptr);
    if (NULL != handle_ptr->identifier_ptr) return false;

    handle_ptr->identifier_ptr = logged_strdup(identifier_ptr);
    return NULL != handle_ptr->identifier_ptr;
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

static void _wlmdock_toplevel_test_handle(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t _wlmdock_toplevel_test_cases[] = {
    { true, "handle", _wlmdock_toplevel_test_handle },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmdock_toplevel_test_set = BS_TEST_SET(
    true, "toplevel", _wlmdock_toplevel_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests handle setup and teardown. */
void _wlmdock_toplevel_test_handle(bs_test_t *t)
{
    struct wlmdock_toplevel_handle *h = wlmdock_toplevel_handle_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(t, NULL, h);

    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, "appid0"));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, NULL));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, "appid1"));

    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, "title0"));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, NULL));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, "title1"));

    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_identifier(h, "id0"));
    BS_TEST_VERIFY_FALSE(t, wlmdock_toplevel_handle_set_identifier(h, "id1"));

    wlmdock_toplevel_handle_destroy(h);
}

/* == End of toplevel.c ==================================================== */
