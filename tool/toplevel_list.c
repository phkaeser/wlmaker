/* ========================================================================= */
/**
 * @file toplevel_list.c
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

#include "toplevel_list.h"

#include <libbase/libbase.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"

/* == Declarations ========================================================= */

struct wlmtool_toplevel_list_state {
    struct wl_display         *wl_display_ptr;

    struct wl_registry        *wl_registry_ptr;

    struct ext_foreign_toplevel_list_v1 *list_ptr;
};

static void _wlmtool_handle_global_announce(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name,
    const char *interface_name_ptr,
    uint32_t version);
static void _wlmtool_handle_global_remove(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name);

static void _wlmtool_handle_toplevel(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
    struct ext_foreign_toplevel_handle_v1 *toplevel);
static void _wlmtool_handle_finished(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1);

static void _wlmtool_toplevel_handle_closed(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
static void _wlmtool_toplevel_handle_done(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1);
static void _wlmtool_toplevel_handle_title(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title);
static void _wlmtool_toplevel_handle_app_id(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *app_id);
static void _wlmtool_toplevel_handle_identifier(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier);

/* == Data ================================================================= */

/** Listener for the registry, taking note of registry updates. */
static const struct wl_registry_listener _wlmtool_registry_listener = {
    .global = _wlmtool_handle_global_announce,
    .global_remove = _wlmtool_handle_global_remove,
};

static const struct ext_foreign_toplevel_list_v1_listener _wlmtool_listener = {
    .toplevel = _wlmtool_handle_toplevel,
    .finished = _wlmtool_handle_finished
};

static const struct ext_foreign_toplevel_handle_v1_listener _toplevel_listener = {
    .closed = _wlmtool_toplevel_handle_closed,
    .done = _wlmtool_toplevel_handle_done,
    .title = _wlmtool_toplevel_handle_title,
    .app_id = _wlmtool_toplevel_handle_app_id,
    .identifier = _wlmtool_toplevel_handle_identifier
};

/* == Exported methods ===================================================== */

bool wlmtool_toplevel_list(__UNUSED__ int argc, __UNUSED__ const char **argv)
{
    struct wlmtool_toplevel_list_state tl_state = {};

    tl_state.wl_display_ptr = wl_display_connect(NULL);
    if (NULL == tl_state.wl_display_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_connect(NULL)");
        return false;
    }

    tl_state.wl_registry_ptr = wl_display_get_registry(
        tl_state.wl_display_ptr);
    if (NULL == tl_state.wl_registry_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_get_registry(%p)",
               tl_state.wl_display_ptr);
        return false;
    }

    if (0 != wl_registry_add_listener(
            tl_state.wl_registry_ptr,
            &_wlmtool_registry_listener,
            &tl_state)) {
        bs_log(BS_ERROR, "Failed wl_registry_add_listener(%p, %p, %p)",
               tl_state.wl_registry_ptr,
               &_wlmtool_registry_listener,
               &tl_state);
        return false;
    }

    while (wl_display_dispatch(tl_state.wl_display_ptr) != -1) {}
    wl_display_disconnect(tl_state.wl_display_ptr);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the announcement of a global object.
 *
 * Called by `struct wl_registry_listener` `global` callback, invoked to notify
 * clients of global objects.
 *
 * @param data_ptr            Points to a @ref wlmcl_client_t.
 * @param wl_registry_ptr     The `struct wl_registry` this is invoked for.
 * @param name                Numeric name of the global object.
 * @param interface_name_ptr  Name of the interface implemented by the object.
 * @param version             Interface version.
 */
void _wlmtool_handle_global_announce(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name,
    const char *interface_name_ptr,
    uint32_t version)
{
    struct wlmtool_toplevel_list_state *tl_state_ptr = data_ptr;

    bs_log(BS_ERROR, "name %"PRIu32", %s, %"PRIu32,
           name, interface_name_ptr, version);

    if (0 == strcmp("ext_foreign_toplevel_list_v1", interface_name_ptr)) {
        tl_state_ptr->list_ptr = wl_registry_bind(
            wl_registry_ptr,
            name,
            &ext_foreign_toplevel_list_v1_interface,
            1);

        bs_log(BS_ERROR, "FIXME: add");
        ext_foreign_toplevel_list_v1_add_listener(
            tl_state_ptr->list_ptr,
            &_wlmtool_listener,
            tl_state_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles removal of a global object. */
void _wlmtool_handle_global_remove(
    void *data_ptr,
    struct wl_registry *wl_registry_ptr,
    uint32_t name)
{
    // TODO(kaeser@gubbe.ch): Add implementation.
    bs_log(BS_INFO, "handle_global_remove(%p, %p, %"PRIu32").",
           data_ptr, wl_registry_ptr, name);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_handle_toplevel(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
    struct ext_foreign_toplevel_handle_v1 *toplevel)
{
    bs_log(BS_ERROR, "toplevel: data %p, list %p, toplevel %p",
           data, ext_foreign_toplevel_list_v1, toplevel);

    ext_foreign_toplevel_handle_v1_add_listener(
        toplevel,
        &_toplevel_listener,
        data);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_handle_finished(
    void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
    bs_log(BS_ERROR, "finiished: data %p, list %p",
           data, ext_foreign_toplevel_list_v1);
}


/* ------------------------------------------------------------------------- */
void _wlmtool_toplevel_handle_closed(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    bs_log(BS_ERROR, "FIXME: toplevel closed %p, %p", data,
           ext_foreign_toplevel_handle_v1);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_toplevel_handle_done(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    bs_log(BS_ERROR, "FIXME: toplevel done %p, %p", data,
           ext_foreign_toplevel_handle_v1);
}

/* ------------------------------------------------------------------------- */
void _wlmtool_toplevel_handle_title(
    __UNUSED__ void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    __UNUSED__ const char *title)
{
}

/* ------------------------------------------------------------------------- */
void _wlmtool_toplevel_handle_app_id(
    __UNUSED__ void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    __UNUSED__ const char *app_id)
{
}

/* ------------------------------------------------------------------------- */
void _wlmtool_toplevel_handle_identifier(
    __UNUSED__ void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    __UNUSED__ const char *identifier)
{
}

/* == Unit Tests =========================================================== */

/* == End of toplevel_list.c =============================================== */
