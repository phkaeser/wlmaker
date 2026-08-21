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
#include <libbase/signal.h>
#include <stdlib.h>
#include <string.h>

#include "wlclient/wlclient.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

struct ext_foreign_toplevel_handle_v1;
struct ext_foreign_toplevel_list_v1;

/* == Declarations ========================================================= */

/** Tracker for toplevels. */
struct wlmdock_toplevel_tracker {
    /** Double-linked list of @ref wlmdock_toplevel_handle::node. */
    bs_dllist_t               toplevel_handles;

    /** The bound interface to `ext_foreign_toplevel_list_v1`. */
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1_ptr;
    /** toplevel by app_id */
    bs_avltree_t              *app_id_tree_ptr;
    /** Events of the tracker. */
    struct wlmdock_toplevel_tracker_events events;
};

/** Representation of a ext_foreign_toplevel_handle_v1. */
struct wlmdock_toplevel_handle {
    /** Node of @ref wlmdock_toplevel_tracker::toplevel_handles. */
    bs_dllist_node_t          node;

    /** Node of @ref wlmdock_toplevel_app_ids::toplevel_handles. */
    bs_dllist_node_t          app_id_node;
    /** Optional: app id of the toplevel. The app ID may change. */
    char                      *app_id_ptr;
    /** Optional: Title of the toplevel. The title may change. */
    char                      *title_ptr;
    /** Identifier. If set, it must be unique per toplevel. Must not change. */
    char                      *identifier_ptr;

    /** Back-link to tracker. */
    struct wlmdock_toplevel_tracker *tracker_ptr;
    /** Bound interface to `ext_foreign_toplevel_handle_v1`. */
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1_ptr;
};

/** Toplevel handles that share a given application ID. */
struct wlmdock_toplevel_app_ids {
    /** Referred from @ref wlmdock_toplevel_tracker::app_id_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** Double-linked list of @ref wlmdock_toplevel_handle::app_id_node. */
    bs_dllist_t               toplevel_handles;
};

static void _wlmdock_toplevel_setup_foreign_toplevel_list(
    void *bound_interface_ptr,
    void *userdata_ptr);
static void _wlmdock_toplevel_handle_destroy_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static int _wlmdock_toplevel_app_id_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);

static void _wlmdock_toplevel_tracker_foreign_toplevel_list_toplevel(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
    struct ext_foreign_toplevel_handle_v1 *toplevel);
static void _wlmdock_toplevel_tracker_foreign_toplevel_list_finished(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1);

static void _wlmdock_toplevel_tracker_foreign_toplevel_handle_closed(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
static void _wlmdock_toplevel_tracker_foreign_toplevel_handle_done(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
static void _wlmdock_toplevel_tracker_foreign_toplevel_handle_title(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title);
static void _wlmdock_toplevel_tracker_foreign_toplevel_handle_app_id(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *app_id);
static void _wlmdock_toplevel_tracker_foreign_toplevel_handle_identifier(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier);

/* == Data ================================================================= */

/** Handlers for `ext_foreign_toplevel_list_v1` */
static const struct ext_foreign_toplevel_list_v1_listener
_wlmdock_toplevel_tracker_ext_foreign_toplevel_list_v1_listener = {
    _wlmdock_toplevel_tracker_foreign_toplevel_list_toplevel,
    _wlmdock_toplevel_tracker_foreign_toplevel_list_finished
};

/** Handlers for `ext_foreign_toplevel_handle_v1` */
static const struct ext_foreign_toplevel_handle_v1_listener
_wlmdock_toplevel_tracker_ext_foreign_toplevel_handle_v1_listener = {
    .closed = _wlmdock_toplevel_tracker_foreign_toplevel_handle_closed,
    .done = _wlmdock_toplevel_tracker_foreign_toplevel_handle_done,
    .title = _wlmdock_toplevel_tracker_foreign_toplevel_handle_title,
    .app_id = _wlmdock_toplevel_tracker_foreign_toplevel_handle_app_id,
    .identifier = _wlmdock_toplevel_tracker_foreign_toplevel_handle_identifier
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmdock_toplevel_tracker *wlmdock_toplevel_tracker_create(void)
{
    struct wlmdock_toplevel_tracker *tracker_ptr = logged_calloc(
        1, sizeof(*tracker_ptr));
    if (NULL == tracker_ptr) return NULL;

    tracker_ptr->app_id_tree_ptr = bs_avltree_create(
        _wlmdock_toplevel_app_id_cmp, NULL);
    if (NULL == tracker_ptr->app_id_tree_ptr) {
        bs_log(BS_ERROR, "Failed bs_avltree_create(%p, NULL)",
               _wlmdock_toplevel_app_id_cmp);
        wlmdock_toplevel_tracker_destroy(tracker_ptr);
        return NULL;
    }

    return tracker_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_toplevel_tracker_destroy(
    struct wlmdock_toplevel_tracker *tracker_ptr)
{
    if (NULL != tracker_ptr->ext_foreign_toplevel_list_v1_ptr) {
        ext_foreign_toplevel_list_v1_destroy(
            tracker_ptr->ext_foreign_toplevel_list_v1_ptr);
        tracker_ptr->ext_foreign_toplevel_list_v1_ptr = NULL;
    }

    bs_dllist_for_each(
        &tracker_ptr->toplevel_handles,
        _wlmdock_toplevel_handle_destroy_dlnode,
        NULL);

    if (NULL != tracker_ptr->app_id_tree_ptr) {
        BS_ASSERT(0 == bs_avltree_size(tracker_ptr->app_id_tree_ptr));
        bs_avltree_destroy(tracker_ptr->app_id_tree_ptr);
        tracker_ptr->app_id_tree_ptr = NULL;
    }

    free(tracker_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_register_ext_foreign_toplevel_list(
    wlmcl_client_t *client_ptr,
    struct wlmdock_toplevel_tracker *tracker_ptr)
{
    return wlmcl_client_register_interface(
        client_ptr,
        &ext_foreign_toplevel_list_v1_interface,
        /* desired_version */ 1,
        /* required */ true,
        _wlmdock_toplevel_setup_foreign_toplevel_list,
        tracker_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlmdock_toplevel_tracker_events *wlmdock_toplevel_tracker_events(
    struct wlmdock_toplevel_tracker *tracker_ptr)
{
    return &tracker_ptr->events;
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_tracker_register_app_id(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    struct wlmdock_toplevel_handle *handle_ptr)
{
    BS_ASSERT_NOTNULL(handle_ptr->app_id_ptr);

    struct wlmdock_toplevel_app_ids *app_ids_ptr;
    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        tracker_ptr->app_id_tree_ptr, handle_ptr->app_id_ptr);
    if (NULL == avlnode_ptr) {
        app_ids_ptr = logged_calloc(1, sizeof(*app_ids_ptr));
        if (NULL == app_ids_ptr) return false;
        BS_ASSERT(bs_avltree_insert(
                      tracker_ptr->app_id_tree_ptr,
                      handle_ptr->app_id_ptr,
                      &app_ids_ptr->avlnode,
                      false));
    } else {
        app_ids_ptr = BS_CONTAINER_OF(
            avlnode_ptr, struct wlmdock_toplevel_app_ids, avlnode);
    }

    bs_dllist_push_back(
        &app_ids_ptr->toplevel_handles,
        &handle_ptr->app_id_node);
    bs_signal_emit(
        &tracker_ptr->events.app_id_changed,
        handle_ptr->app_id_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmdock_toplevel_tracker_unregister_app_id(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    struct wlmdock_toplevel_handle *handle_ptr)
{
    BS_ASSERT_NOTNULL(handle_ptr->app_id_ptr);

    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        tracker_ptr->app_id_tree_ptr, handle_ptr->app_id_ptr);
    BS_ASSERT_NOTNULL(avlnode_ptr);

    struct wlmdock_toplevel_app_ids *app_ids_ptr = BS_CONTAINER_OF(
        avlnode_ptr, struct wlmdock_toplevel_app_ids, avlnode);
    bs_dllist_remove(&app_ids_ptr->toplevel_handles, &handle_ptr->app_id_node);

    if (bs_dllist_empty(&app_ids_ptr->toplevel_handles)) {
        bs_avltree_node_delete(
            tracker_ptr->app_id_tree_ptr,
            avlnode_ptr);
        free(app_ids_ptr);
    }

    bs_signal_emit(
        &tracker_ptr->events.app_id_changed,
        handle_ptr->app_id_ptr);
}

/* ------------------------------------------------------------------------- */
bs_dllist_t *wlmdock_toplevel_tracker_app_id_handles(
    struct wlmdock_toplevel_tracker *tracker_ptr,
    const char *app_id_ptr)
{
    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        tracker_ptr->app_id_tree_ptr, app_id_ptr);
    if (NULL == avlnode_ptr) return NULL;

    struct wlmdock_toplevel_app_ids *app_ids_ptr = BS_CONTAINER_OF(
        avlnode_ptr, struct wlmdock_toplevel_app_ids, avlnode);
    return &app_ids_ptr->toplevel_handles;
}

/* ------------------------------------------------------------------------- */
struct wlmdock_toplevel_handle *wlmdock_toplevel_handle_create(
    struct wlmdock_toplevel_tracker *tracker_ptr)
{
    struct wlmdock_toplevel_handle *handle_ptr = logged_calloc(
        1, sizeof(*handle_ptr));
    if (NULL == handle_ptr) return NULL;
    handle_ptr->tracker_ptr = BS_ASSERT_NOTNULL(tracker_ptr);
    bs_dllist_push_back(
        &tracker_ptr->toplevel_handles,
        &handle_ptr->node);

    return handle_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_toplevel_handle_destroy(
    struct wlmdock_toplevel_handle *handle_ptr)
{
    if (NULL != handle_ptr->tracker_ptr) {
        bs_dllist_remove(
            &handle_ptr->tracker_ptr->toplevel_handles,
            &handle_ptr->node);
    }

    if (NULL != handle_ptr->app_id_ptr) {
        wlmdock_toplevel_handle_set_app_id(handle_ptr, NULL);
    }
    if (NULL != handle_ptr->title_ptr) {
        free(handle_ptr->title_ptr);
        handle_ptr->title_ptr = NULL;
    }
    if (NULL != handle_ptr->identifier_ptr) {
        free(handle_ptr->identifier_ptr);
        handle_ptr->identifier_ptr = NULL;
    }

    if (NULL != handle_ptr->ext_foreign_toplevel_handle_v1_ptr) {
        ext_foreign_toplevel_handle_v1_destroy(
            handle_ptr->ext_foreign_toplevel_handle_v1_ptr);
        handle_ptr->ext_foreign_toplevel_handle_v1_ptr = NULL;
    }

    free(handle_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmdock_toplevel_handle_set_app_id(
    struct wlmdock_toplevel_handle *handle_ptr,
    const char *app_id_ptr)
{
    if (NULL != handle_ptr->app_id_ptr) {
        wlmdock_toplevel_tracker_unregister_app_id(
            handle_ptr->tracker_ptr,
            handle_ptr);
        free(handle_ptr->app_id_ptr);
        handle_ptr->app_id_ptr = NULL;
    }
    if (NULL == app_id_ptr) return true;

    handle_ptr->app_id_ptr = logged_strdup(app_id_ptr);
    if (NULL != handle_ptr->app_id_ptr) {
        if (wlmdock_toplevel_tracker_register_app_id(
                handle_ptr->tracker_ptr,
                handle_ptr)) {
            return true;
        }
        free(handle_ptr->app_id_ptr);
        handle_ptr->app_id_ptr = NULL;
    }
    return false;
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

/* ------------------------------------------------------------------------- */
/** Registration callback for @ref wlmcl_client_register_interface. */
void _wlmdock_toplevel_setup_foreign_toplevel_list(
    void *bound_interface_ptr,
    void *userdata_ptr)
{
    struct wlmdock_toplevel_tracker *tracker_ptr = userdata_ptr;

    tracker_ptr->ext_foreign_toplevel_list_v1_ptr = bound_interface_ptr;
    ext_foreign_toplevel_list_v1_add_listener(
        tracker_ptr->ext_foreign_toplevel_list_v1_ptr,
        &_wlmdock_toplevel_tracker_ext_foreign_toplevel_list_v1_listener,
        tracker_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor for toplevel handle. Callback to `bs_dllist_for_each`. */
void _wlmdock_toplevel_handle_destroy_dlnode(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct wlmdock_toplevel_handle *handle_ptr = BS_CONTAINER_OF(
        dlnode_ptr, struct wlmdock_toplevel_handle, node);
    wlmdock_toplevel_handle_destroy(handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for @ref wlmdock_toplevel_tracker::app_id_tree_ptr. */
int _wlmdock_toplevel_app_id_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr)
{
    const struct wlmdock_toplevel_app_ids *app_ids_ptr = BS_CONTAINER_OF(
        node_ptr, const struct wlmdock_toplevel_app_ids, avlnode);

    // To save some bytes: use app_id_ptr of the first toplevel_handles.
    // This works because the node will be deleted when the list becomes
    // empty.
    const struct wlmdock_toplevel_handle *head_ptr = BS_CONTAINER_OF(
        app_ids_ptr->toplevel_handles.head_ptr,
        const struct wlmdock_toplevel_handle,
        app_id_node);
    BS_ASSERT_NOTNULL(head_ptr);
    return strcmp(head_ptr->app_id_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** A new toplevel is created. */
void _wlmdock_toplevel_tracker_foreign_toplevel_list_toplevel(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
    struct ext_foreign_toplevel_handle_v1 *toplevel)
{
    struct wlmdock_toplevel_tracker *tracker_ptr = data;

    struct wlmdock_toplevel_handle *handle_ptr =
        wlmdock_toplevel_handle_create(tracker_ptr);
    if (NULL == handle_ptr) {
        ext_foreign_toplevel_handle_v1_destroy(toplevel);
        return;
    }
    handle_ptr->ext_foreign_toplevel_handle_v1_ptr = toplevel;

    ext_foreign_toplevel_handle_v1_add_listener(
        toplevel,
        &_wlmdock_toplevel_tracker_ext_foreign_toplevel_handle_v1_listener,
        handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Compositor is finished with the toplevel list. Good to destroy. */
void _wlmdock_toplevel_tracker_foreign_toplevel_list_finished(
    __UNUSED__ void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
    ext_foreign_toplevel_list_v1_destroy(ext_foreign_toplevel_list_v1);
}

/* ------------------------------------------------------------------------- */
/** The toplevel has been closed. Good to destroy. */
void _wlmdock_toplevel_tracker_foreign_toplevel_handle_closed(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    struct wlmdock_toplevel_handle *handle_ptr = data;

    wlmdock_toplevel_handle_destroy(handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** All information for this toplevel was sent. Until it changes again. */
void _wlmdock_toplevel_tracker_foreign_toplevel_handle_done(
    __UNUSED__ void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    // Ignored.
}

/* ------------------------------------------------------------------------- */
/** Sets the title for this toplevel. */
void _wlmdock_toplevel_tracker_foreign_toplevel_handle_title(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title)
{
    struct wlmdock_toplevel_handle *handle_ptr = data;

    wlmdock_toplevel_handle_set_title(handle_ptr, title);
}

/* ------------------------------------------------------------------------- */
/** Sets the app ID for this toplevel. */
void _wlmdock_toplevel_tracker_foreign_toplevel_handle_app_id(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *app_id)
{
    struct wlmdock_toplevel_handle *handle_ptr = data;

    wlmdock_toplevel_handle_set_app_id(handle_ptr, app_id);
}

/* ------------------------------------------------------------------------- */
/** Sets the identifier for this toplevel. */
void _wlmdock_toplevel_tracker_foreign_toplevel_handle_identifier(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier)
{
    struct wlmdock_toplevel_handle *handle_ptr = data;

    wlmdock_toplevel_handle_set_identifier(handle_ptr, identifier);
}

/* == Unit Tests =========================================================== */

static void _wlmdock_toplevel_test_tracker(bs_test_t *test_ptr);
static void _wlmdock_toplevel_test_handle(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t _wlmdock_toplevel_test_cases[] = {
    { true, "tracker", _wlmdock_toplevel_test_tracker },
    { true, "handle", _wlmdock_toplevel_test_handle },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmdock_toplevel_test_set = BS_TEST_SET(
    true, "toplevel", _wlmdock_toplevel_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests tracker. */
void _wlmdock_toplevel_test_tracker(bs_test_t *t)
{
    struct wlmdock_toplevel_tracker *trk = wlmdock_toplevel_tracker_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(t, NULL, trk);

    BS_TEST_VERIFY_EQ(
        t,
        &trk->events,
        wlmdock_toplevel_tracker_events(trk));

    wlmdock_toplevel_tracker_destroy(trk);
}

/* ------------------------------------------------------------------------- */
/** Tests handle setup and teardown. */
void _wlmdock_toplevel_test_handle(bs_test_t *t)
{
    struct bs_test_listener tl = {};

    struct wlmdock_toplevel_tracker *trk = wlmdock_toplevel_tracker_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(t, NULL, trk);
    bs_test_listener_connect(
        &wlmdock_toplevel_tracker_events(trk)->app_id_changed,
        &tl);

    struct wlmdock_toplevel_handle *h = wlmdock_toplevel_handle_create(trk);
    BS_TEST_VERIFY_NEQ_OR_RETURN(t, NULL, h);
    BS_TEST_VERIFY_EQ(t, NULL, wlmdock_toplevel_tracker_app_id_handles(
                          trk, "appid0"));
    BS_TEST_VERIFY_EQ(t, 0, tl.calls);

    // Set app_id. Must be tracked and raise the signal.
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, "appid0"));
    BS_TEST_VERIFY_NEQ(t, NULL, wlmdock_toplevel_tracker_app_id_handles(
                           trk, "appid0"));
    BS_TEST_VERIFY_EQ(t, 1, tl.calls);
    BS_TEST_VERIFY_STREQ(t, "appid0", tl.last_data_ptr);

    // Clear the app_id. Must be tracked, and clear the signal.
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, NULL));
    BS_TEST_VERIFY_EQ(t, NULL, wlmdock_toplevel_tracker_app_id_handles(
                          trk, "appid0"));
    BS_TEST_VERIFY_EQ(t, 2, tl.calls);

    // Set a new app ID.
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_app_id(h, "appid1"));
    BS_TEST_VERIFY_EQ(t, 3, tl.calls);
    BS_TEST_VERIFY_STREQ(t, "appid1", tl.last_data_ptr);

    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, "title0"));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, NULL));
    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_title(h, "title1"));

    BS_TEST_VERIFY_TRUE(t, wlmdock_toplevel_handle_set_identifier(h, "id0"));
    BS_TEST_VERIFY_FALSE(t, wlmdock_toplevel_handle_set_identifier(h, "id1"));

    // No further calls seen.
    BS_TEST_VERIFY_EQ(t, 3, tl.calls);

    wlmdock_toplevel_handle_destroy(h);
    bs_test_listener_disconnect(
        &wlmdock_toplevel_tracker_events(trk)->app_id_changed,
        &tl);
    wlmdock_toplevel_tracker_destroy(trk);
}

/* == End of toplevel.c ==================================================== */
