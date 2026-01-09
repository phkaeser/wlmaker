/* ========================================================================= */
/**
 * @file output_tracker.c
 *
 * @copyright
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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

#include "output_tracker.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

#include "test.h"  // IWYU pragma: keep
#include "util.h"

struct wl_list;

/* == Declarations ========================================================= */

/** State of the output tracker. */
struct _wlmtk_output_tracker_t {
    /** The output layout tracked here. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;

    /** Callback for when an output is added ("created"). */
    wlmtk_output_tracker_output_create_callback_t create_fn;
    /** Callback for when the layout changes, but the output remains. */
    wlmtk_output_tracker_output_update_callback_t update_fn;
    /** Callback for when the output is removed ("destroyed"). */
    wlmtk_output_tracker_output_destroy_callback_t destroy_fn;
    /** The userdata provided to @ref wlmtk_output_tracker_create. */
    void                      *userdata_ptr;

    /** All outputs, through @ref wlmtk_output_tracker_output_t::avlnode. */
    bs_avltree_t              *output_tree_ptr;
};

/** A tracked output. */
typedef struct {
    /** Tree node, from @ref wlmtk_output_tracker_t::output_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** Back-link to the tracker. */
    wlmtk_output_tracker_t    *tracker_ptr;

    /** The WLR output tracked. */
    struct wlr_output         *wlr_output_ptr;
    /** Holds the return value of @ref wlmtk_output_tracker_t::create_fn. */
    void                      *create_fn_retval_ptr;
} wlmtk_output_tracker_output_t;

/** Arguments to @ref _wlmtk_output_tracker_output_handle_change. */
struct wlmtk_output_tracker_update_arg {
    /** Tracker. */
    wlmtk_output_tracker_t    *tracker_ptr;
    /** The former output tree. */
    bs_avltree_t              *former_output_tree_ptr;
};

static void _wlmtk_output_tracker_tree_node_destroy(
    bs_avltree_node_t *avlnode_ptr);
static int _wlmtk_output_tracker_tree_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr);
static void _wlmtk_output_tracker_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static bool _wlmtk_output_tracker_output_handle_change(
    struct wl_list *link_ptr,
    void *ud_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_output_tracker_t *wlmtk_output_tracker_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    void *userdata_ptr,
    wlmtk_output_tracker_output_create_callback_t create_fn,
    wlmtk_output_tracker_output_update_callback_t update_fn,
    wlmtk_output_tracker_output_destroy_callback_t destroy_fn)
{
    wlmtk_output_tracker_t *tracker_ptr = logged_calloc(
        1, sizeof(wlmtk_output_tracker_t));
    if (NULL == tracker_ptr) return NULL;
    tracker_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    tracker_ptr->userdata_ptr = userdata_ptr;
    tracker_ptr->create_fn = create_fn;
    tracker_ptr->update_fn = update_fn;
    tracker_ptr->destroy_fn = destroy_fn;

    tracker_ptr->output_tree_ptr = bs_avltree_create(
        _wlmtk_output_tracker_tree_node_cmp,
        _wlmtk_output_tracker_tree_node_destroy);
    if (NULL == tracker_ptr->output_tree_ptr) {
        wlmtk_output_tracker_destroy(tracker_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &tracker_ptr->wlr_output_layout_ptr->events.change,
        &tracker_ptr->output_layout_change_listener,
        _wlmtk_output_tracker_handle_output_layout_change);
    _wlmtk_output_tracker_handle_output_layout_change(
        &tracker_ptr->output_layout_change_listener,
        tracker_ptr->wlr_output_layout_ptr);

    return tracker_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_output_tracker_destroy(wlmtk_output_tracker_t *tracker_ptr)
{
    wlmtk_util_disconnect_listener(
        &tracker_ptr->output_layout_change_listener);

    if (NULL != tracker_ptr->output_tree_ptr) {
        bs_avltree_destroy(tracker_ptr->output_tree_ptr);
        tracker_ptr->output_tree_ptr = NULL;
    }

    free(tracker_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Creates state for tracking an output. */
wlmtk_output_tracker_output_t *wlmtk_output_tracker_output_create(
    wlmtk_output_tracker_t *tracker_ptr,
    struct wlr_output *wlr_output_ptr)
{
    wlmtk_output_tracker_output_t *output_ptr = logged_calloc(
        1, sizeof(wlmtk_output_tracker_output_t));
    if (NULL == output_ptr) return NULL;
    output_ptr->wlr_output_ptr = wlr_output_ptr;
    output_ptr->tracker_ptr = tracker_ptr;

    if (NULL != tracker_ptr->create_fn) {
        output_ptr->create_fn_retval_ptr = tracker_ptr->create_fn(
            wlr_output_ptr,
            tracker_ptr->userdata_ptr);
        if (NULL == output_ptr->create_fn_retval_ptr) {
            _wlmtk_output_tracker_tree_node_destroy(
                &output_ptr->avlnode);
            return NULL;
        }
    }

    return output_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the tracker state for an output, by the tree node. */
void _wlmtk_output_tracker_tree_node_destroy(
    bs_avltree_node_t *avlnode_ptr)
{
    wlmtk_output_tracker_output_t *output_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_output_tracker_output_t, avlnode);

    if (NULL != output_ptr->tracker_ptr->destroy_fn &&
        NULL != output_ptr->create_fn_retval_ptr) {
        output_ptr->tracker_ptr->destroy_fn(
            output_ptr->wlr_output_ptr,
            output_ptr->tracker_ptr->userdata_ptr,
            output_ptr->create_fn_retval_ptr);
        output_ptr->create_fn_retval_ptr = NULL;
    }

    free(output_ptr);
}

/* ------------------------------------------------------------------------- */
/** Compares @ref wlmtk_output_tracker_output_t::wlr_output_ptr. */
int _wlmtk_output_tracker_tree_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr)
{
    const wlmtk_output_tracker_output_t *output_ptr = BS_CONTAINER_OF(
        avlnode_ptr, const wlmtk_output_tracker_output_t, avlnode);
    return bs_avltree_cmp_ptr(output_ptr->wlr_output_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Output layout changed: Iterate over all outputs. */
void _wlmtk_output_tracker_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_output_tracker_t *tracker_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_output_tracker_t, output_layout_change_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    struct wlmtk_output_tracker_update_arg arg = {
        .tracker_ptr = tracker_ptr,
        .former_output_tree_ptr = tracker_ptr->output_tree_ptr
    };

    tracker_ptr->output_tree_ptr = BS_ASSERT_NOTNULL(
        bs_avltree_create(
            _wlmtk_output_tracker_tree_node_cmp,
            _wlmtk_output_tracker_tree_node_destroy));
    BS_ASSERT(wlmtk_util_wl_list_for_each(
                  &wlr_output_layout_ptr->outputs,
                  _wlmtk_output_tracker_output_handle_change,
                  &arg));

    // This will destroy any output no longer in the layout.
    bs_avltree_destroy(arg.former_output_tree_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles change for one output of the layout: Calls create or update. */
bool _wlmtk_output_tracker_output_handle_change(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_ptr->output;
    struct wlmtk_output_tracker_update_arg *arg_ptr = ud_ptr;

    wlmtk_output_tracker_output_t *output_ptr = NULL;
    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        arg_ptr->former_output_tree_ptr, wlr_output_ptr);
    if (NULL != avlnode_ptr) {
        output_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmtk_output_tracker_output_t, avlnode);
        if (NULL != arg_ptr->tracker_ptr->update_fn) {
            arg_ptr->tracker_ptr->update_fn(
                wlr_output_ptr,
                arg_ptr->tracker_ptr->userdata_ptr,
                output_ptr->create_fn_retval_ptr);
        }
    } else {
        output_ptr = wlmtk_output_tracker_output_create(
            arg_ptr->tracker_ptr, wlr_output_ptr);
        if (NULL == output_ptr) return false;
    }

    return bs_avltree_insert(
        arg_ptr->tracker_ptr->output_tree_ptr,
        wlr_output_ptr,
        &output_ptr->avlnode,
        false);
}

/* == Unit Tests =========================================================== */

static void _wlmtk_output_tracker_test(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_output_tracker_test_cases[] = {
    { 1, "test", _wlmtk_output_tracker_test },
    { 0, NULL, NULL }
};

/** For tests: State of tracked outputs. */
struct _wlmtk_output_tracker_test_state {
    /** Test. */
    bs_test_t                 *test_ptr;
    /** Stores the outputs */
    struct wlr_output         *outputs[3];
    /** Counts @ref wlmtk_output_tracker_output_create_callback_t calls. */
    intptr_t                  create_calls;
    /** Counts @ref wlmtk_output_tracker_output_destroy_callback_t calls. */
    intptr_t                  destroy_calls;
    /** Counts @ref wlmtk_output_tracker_output_update_callback_t calls. */
    intptr_t                  update_calls;
};

/** For tests: Creates a tracked output. */
static void *_wlmtk_output_tracker_test_create_output(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    struct _wlmtk_output_tracker_test_state *state_ptr = ud_ptr;
    BS_ASSERT(state_ptr->create_calls < 3);
    state_ptr->outputs[state_ptr->create_calls++] = wlr_output_ptr;
    return (void*)(state_ptr->create_calls);
}

/** For tests: Update to a tracked output. */
static void _wlmtk_output_tracker_test_update_output(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    struct _wlmtk_output_tracker_test_state *state_ptr = ud_ptr;
    BS_TEST_VERIFY_EQ(
        state_ptr->test_ptr,
        state_ptr->outputs[(intptr_t)(output_ptr) - 1],
        wlr_output_ptr);
    state_ptr->update_calls++;
}

/** For tests: Destroys a tracked output. */
static void _wlmtk_output_tracker_test_destroy_output(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr)
{
    struct _wlmtk_output_tracker_test_state *state_ptr = ud_ptr;
    BS_TEST_VERIFY_EQ(
        state_ptr->test_ptr,
        state_ptr->outputs[(intptr_t)(output_ptr) - 1],
        wlr_output_ptr);
    state_ptr->destroy_calls++;
}

/* ------------------------------------------------------------------------- */
/** Tests output tracker. */
void _wlmtk_output_tracker_test(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);

    // Setup layout, initially with o1 and o2.
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    struct wlr_output o2 = o1;
    struct wlr_output o3 = o1;
    wlmtk_test_wlr_output_init(&o1);
    wlmtk_test_wlr_output_init(&o2);
    wlmtk_test_wlr_output_init(&o3);
    wlr_output_layout_add(wlr_output_layout_ptr, &o1, 0, 0);
    wlr_output_layout_add(wlr_output_layout_ptr, &o2, 0, 0);

    struct _wlmtk_output_tracker_test_state state = { .test_ptr = test_ptr };
    wlmtk_output_tracker_t *t = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        &state,
        _wlmtk_output_tracker_test_create_output,
        _wlmtk_output_tracker_test_update_output,
        _wlmtk_output_tracker_test_destroy_output);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, t);
    BS_TEST_VERIFY_EQ(test_ptr, 2, state.create_calls);
    BS_TEST_VERIFY_EQ(test_ptr, 0, state.update_calls);

    wlr_output_layout_add(wlr_output_layout_ptr, &o3, 0, 0);
    BS_TEST_VERIFY_EQ(test_ptr, 3, state.create_calls);
    BS_TEST_VERIFY_EQ(test_ptr, 2, state.update_calls);

    wl_signal_emit(
        &wlr_output_layout_ptr->events.change,
        wlr_output_layout_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 5, state.update_calls);

    wlr_output_layout_remove(wlr_output_layout_ptr, &o2);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state.destroy_calls);

    wlmtk_output_tracker_destroy(t);
    BS_TEST_VERIFY_EQ(test_ptr, 3, state.destroy_calls);

    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(display_ptr);
}

/* == End of output_tracker.c ============================================== */
