/* ========================================================================= */
/**
 * @file output.c
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

#include "output.h"

#include <libbase/libbase.h>
#include <libbase/signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include "wlclient.h"

struct wl_output;

/* == Declarations ========================================================= */

/** Output interface. */
struct wlmcl_output_interface {
    /** All registered outputs. Holds @ref wlmcl_output::dlnode. */
    bs_dllist_t               outputs;
    /** Signal that will be raised when outputs changed. */
    struct bs_signal          *changed_signal_ptr;
    /** The registered client interface. */
    struct wlmcl_client_interface *registered_interface_ptr;
};

/** Holds information about one `struct wl_output`. */
struct wlmcl_output {
    /** List node. Element of @ref wlmcl_output_interface::outputs. */
    bs_dllist_node_t          dlnode;

    /** Back-link to the output interface. */
    struct wlmcl_output_interface *output_interface_ptr;
    /** Points to the output. */
    struct wl_output          *wl_output_ptr;
    /** The "name" of the Wayland client globalk. */
    uint32_t                  global_name;
    /** The output's metadata. */
    struct wlmcl_output_metadata metadata;
};

static void _wlmcl_output_setup(
    void *userdata_ptr,
    void *bound_interface_ptr,
    uint32_t name);
static void _wlmcl_output_destroy(struct wlmcl_output *output_ptr);

static void _wlmcl_output_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
bool _wlmcl_output_dlnode_equals_name(
    bs_dllist_node_t *dlnode_ptr,
    void *name_ptr);

static void _wlmcl_output_handle_geometry(
    void *data,
    struct wl_output *wl_output,
    int32_t x,
    int32_t y,
    int32_t physical_width,
    int32_t physical_height,
    int32_t subpixel,
    const char *make,
    const char *model,
    int32_t transform);
static void _wlmcl_output_handle_mode(
    void *data,
    struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh);
static void _wlmcl_output_handle_done(
    void *data,
    struct wl_output *wl_output);
static void _wlmcl_output_handle_scale(
    void *data,
    struct wl_output *wl_output,
    int32_t factor);
static void _wlmcl_output_handle_name(
    void *data,
    struct wl_output *wl_output,
    const char *name);
static void _wlmcl_output_handle_description(
    void *data,
    struct wl_output *wl_output,
    const char *description);

static void _wlmcl_output_replace_str(char **dest_ptr, const char *src_ptr);

/* == Data ================================================================= */

/** Listener for Wayland's `wl_output` interface. */
static const struct wl_output_listener _wlmcl_output_listener = {
    .geometry = _wlmcl_output_handle_geometry,
    .mode = _wlmcl_output_handle_mode,
    .done = _wlmcl_output_handle_done,
    .scale = _wlmcl_output_handle_scale,
    .name = _wlmcl_output_handle_name,
    .description = _wlmcl_output_handle_description
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmcl_output_interface *wlmcl_output_interface_register(
    wlmcl_client_t *wlclient_ptr)
{
    struct wlmcl_output_interface *if_ptr = logged_calloc(1, sizeof(*if_ptr));
    if (NULL == if_ptr) return NULL;
    if_ptr->changed_signal_ptr =
        &wlmcl_client_events(wlclient_ptr)->outputs_changed;

    if_ptr->registered_interface_ptr = wlmcl_client_register_interface(
        wlclient_ptr,
        &wl_output_interface,
        WL_OUTPUT_NAME_SINCE_VERSION,
        false,  /* not strictly needing an output */
        _wlmcl_output_setup,
        if_ptr);
    if (NULL == if_ptr->registered_interface_ptr) {
        wlmcl_output_interface_unregister(if_ptr);
        return NULL;
    }

    return if_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmcl_output_interface_unregister(struct wlmcl_output_interface *if_ptr)
{
    bs_dllist_for_each(
        &if_ptr->outputs,
        _wlmcl_output_dlnode_destroy,
        NULL);

    free(if_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmcl_output_teardown(
    void *userdata_ptr,
    uint32_t name)
{
    struct wlmcl_output_interface *output_interface_ptr = userdata_ptr;
    bs_dllist_node_t *dlnode_ptr = bs_dllist_find(
        &output_interface_ptr->outputs,
        _wlmcl_output_dlnode_equals_name,
        &name);
    if (NULL == dlnode_ptr) return;

    bs_dllist_remove(
        &output_interface_ptr->outputs,
        dlnode_ptr);
    bs_signal_emit(
        output_interface_ptr->changed_signal_ptr,
        &output_interface_ptr->outputs);
    _wlmcl_output_dlnode_destroy(dlnode_ptr, NULL);
}

/* ------------------------------------------------------------------------- */
bs_dllist_t *wlmcl_outputs(struct wlmcl_output_interface *if_ptr)
{
    if (NULL == if_ptr) return NULL;
    return &if_ptr->outputs;
}

/* ------------------------------------------------------------------------- */
const struct wlmcl_output_metadata *wlmcl_output_metadata(
    struct wlmcl_output *output_ptr)
{
    return &output_ptr->metadata;
}

/* ------------------------------------------------------------------------- */
struct wlmcl_output *wlmcl_output_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, struct wlmcl_output, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Sets up an output, and adds it to the list.
 *
 * @param userdata_ptr
 * @param bound_interface_ptr A wl_output. Will take ownership of it.
 * @param name
 **/
void _wlmcl_output_setup(
    void *userdata_ptr,
    void *bound_interface_ptr,
    uint32_t name)
{
    struct wlmcl_output_interface *output_interface_ptr = userdata_ptr;
    struct wl_output *wl_output_ptr = bound_interface_ptr;

    struct wlmcl_output *output_ptr = logged_calloc(1, sizeof(*output_ptr));
    if (NULL == output_ptr) {
        wl_output_release(output_ptr->wl_output_ptr);
        return;
    }
    output_ptr->wl_output_ptr = wl_output_ptr;
    output_ptr->global_name = name;
    output_ptr->output_interface_ptr = output_interface_ptr;

    if (NULL != wl_output_ptr) {
        wl_output_set_user_data(wl_output_ptr, output_ptr);
        if (0 != wl_output_add_listener(
                wl_output_ptr,
                &_wlmcl_output_listener,
                output_ptr)) {
            _wlmcl_output_destroy(output_ptr);
            return;
        }
    }

    bs_dllist_push_back(
        &output_interface_ptr->outputs,
        &output_ptr->dlnode);
    bs_signal_emit(
        output_interface_ptr->changed_signal_ptr,
        &output_interface_ptr->outputs);
}

/* ------------------------------------------------------------------------- */
/** Destroys the output. */
void _wlmcl_output_destroy(struct wlmcl_output *output_ptr)
{
    if (NULL != output_ptr->wl_output_ptr) {
        wl_output_release(output_ptr->wl_output_ptr);
        output_ptr->wl_output_ptr = NULL;
    }

    if (NULL != output_ptr->metadata.model_ptr) free(output_ptr->metadata.model_ptr);
    if (NULL != output_ptr->metadata.make_ptr) free(output_ptr->metadata.make_ptr);
    if (NULL != output_ptr->metadata.description_ptr) free(output_ptr->metadata.description_ptr);
    if (NULL != output_ptr->metadata.name_ptr) free(output_ptr->metadata.name_ptr);

    free(output_ptr);
}

/* ------------------------------------------------------------------------- */
/** Returns whether the name equals `*name_ptr`. */
bool _wlmcl_output_dlnode_equals_name(
    bs_dllist_node_t *dlnode_ptr,
    void *name_ptr)
{
    uint32_t *global_name_ptr = name_ptr;

    struct wlmcl_output *output_ptr = wlmcl_output_from_dlnode(dlnode_ptr);
    return output_ptr->global_name == *global_name_ptr;
}

/* ------------------------------------------------------------------------- */
/** Iterator for bs_dllist_for_each. */
void _wlmcl_output_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    _wlmcl_output_destroy(wlmcl_output_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::geometry` event. */
void _wlmcl_output_handle_geometry(
    void *data_ptr,
    __UNUSED__ struct wl_output *wl_output,
    __UNUSED__ int32_t x,
    __UNUSED__ int32_t y,
    __UNUSED__ int32_t physical_width,
    __UNUSED__ int32_t physical_height,
    __UNUSED__ int32_t subpixel,
    const char *make,
    const char *model,
    __UNUSED__ int32_t transform)
{
    struct wlmcl_output *output_ptr = data_ptr;
    _wlmcl_output_replace_str(&output_ptr->metadata.make_ptr, make);
    _wlmcl_output_replace_str(&output_ptr->metadata.model_ptr, model);
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::mode` event. */
void _wlmcl_output_handle_mode(
    void *data_ptr,
    __UNUSED__ struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    __UNUSED__ int32_t refresh)
{
    struct wlmcl_output *output_ptr = data_ptr;

    if ((flags & WL_OUTPUT_MODE_CURRENT) != WL_OUTPUT_MODE_CURRENT) return;

    output_ptr->metadata.width = width;
    output_ptr->metadata.height = height;
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::done` event. */
void _wlmcl_output_handle_done(
    void *data_ptr,
    __UNUSED__ struct wl_output *wl_output)
{
    struct wlmcl_output *output_ptr = data_ptr;

    bs_signal_emit(
        output_ptr->output_interface_ptr->changed_signal_ptr,
        &output_ptr->output_interface_ptr->outputs);
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::scale` event. */
void _wlmcl_output_handle_scale(
    __UNUSED__ void *data_ptr,
    __UNUSED__ struct wl_output *wl_output,
    __UNUSED__ int32_t factor)
{
    /* Intentionally left blank. No handling. */
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::name` event. */
void _wlmcl_output_handle_name(
    void *data_ptr,
    __UNUSED__ struct wl_output *wl_output,
    const char *name)
{
    struct wlmcl_output *output_ptr = data_ptr;
    _wlmcl_output_replace_str(&output_ptr->metadata.name_ptr, name);
}

/* ------------------------------------------------------------------------- */
/** Handles the `wl_output::description` event. */
void _wlmcl_output_handle_description(
    void *data_ptr,
    __UNUSED__ struct wl_output *wl_output,
    const char *description)
{
    struct wlmcl_output *output_ptr = data_ptr;
    _wlmcl_output_replace_str(
        &output_ptr->metadata.description_ptr,
        description);
}

/* ------------------------------------------------------------------------- */
/** Replaces the string at `*dest_ptr` with a copy of `src_ptr`. */
void _wlmcl_output_replace_str(char **dest_ptr, const char *src_ptr)
{
    char *new_ptr = NULL;
    if (NULL != src_ptr) new_ptr = logged_strdup(src_ptr);

    if (NULL != *dest_ptr) free(*dest_ptr);
    *dest_ptr = new_ptr;
}

/* == Unit Tests =========================================================== */

static void _wlmcl_output_test_signal(bs_test_t *test_ptr);

/** Output's unit test cases. */
static const bs_test_case_t _wlmcl_output_test_cases[] = {
    { true, "signal", _wlmcl_output_test_signal },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmcl_output_test_set = BS_TEST_SET(
    true, "output", _wlmcl_output_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests that signal is emitted when adding, removing, or updating output. */
void _wlmcl_output_test_signal(bs_test_t *test_ptr)
{
    struct bs_signal signal = {};
    struct wlmcl_output_interface oif = { .changed_signal_ptr = &signal};

    struct bs_test_listener tl = {};
    bs_test_listener_connect(&signal, &tl);

    _wlmcl_output_setup(&oif, NULL, 4321);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&oif.outputs));
    BS_TEST_VERIFY_EQ(test_ptr, 1, tl.calls);
    BS_TEST_VERIFY_EQ(test_ptr, &oif.outputs, tl.last_data_ptr);

    struct wlmcl_output *o = wlmcl_output_from_dlnode(oif.outputs.head_ptr);
    _wlmcl_output_handle_done(o, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, 2, tl.calls);
    BS_TEST_VERIFY_EQ(test_ptr, &oif.outputs, tl.last_data_ptr);

    wlmcl_output_teardown(&oif, 1);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&oif.outputs));
    BS_TEST_VERIFY_EQ(test_ptr, 2, tl.calls);

    wlmcl_output_teardown(&oif, 4321);
    BS_TEST_VERIFY_TRUE(test_ptr, bs_dllist_empty(&oif.outputs));
    BS_TEST_VERIFY_EQ(test_ptr, 3, tl.calls);
    BS_TEST_VERIFY_EQ(test_ptr, &oif.outputs, tl.last_data_ptr);
}

/* == End of output.c ====================================================== */
