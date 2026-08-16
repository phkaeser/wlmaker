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
#include <stdio.h>
#include <stdlib.h>

#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "wlclient/wlclient.h"

struct ext_foreign_toplevel_handle_v1;
struct ext_foreign_toplevel_list_v1;

/* == Declarations ========================================================= */

/** State for listing toplevels. */
struct wlmtool_toplevel_list_state {
    /** Wayland display. */
    struct wl_display         *wl_display_ptr;
    /** Registry. */
    struct wl_registry        *wl_registry_ptr;
    /** The bound ext-foreign-toplevel-list-v1 interface. */
    struct ext_foreign_toplevel_list_v1 *list_ptr;
};

/** Information about one toplevel. */
struct wlmtool_toplevel_info {
    /** Application ID */
    char                      *app_id_ptr;
    /** Title */
    char                      *title_ptr;
    /** Identifier */
    char                      *identifier_ptr;
};

static void _wlmtool_toplevel_setup(
    void *bound_interface_ptr,
    void *userdata_ptr);

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

/** Implementation of the ext_foreign_toplevel_list callbacks. */
static const struct ext_foreign_toplevel_list_v1_listener _wlmtool_listener = {
    .toplevel = _wlmtool_handle_toplevel,
    .finished = _wlmtool_handle_finished
};

/** Implementation of the ext_foreign_toplevel_handle callbacks. */
static const struct ext_foreign_toplevel_handle_v1_listener _toplevel_listener = {
    .closed = _wlmtool_toplevel_handle_closed,
    .done = _wlmtool_toplevel_handle_done,
    .title = _wlmtool_toplevel_handle_title,
    .app_id = _wlmtool_toplevel_handle_app_id,
    .identifier = _wlmtool_toplevel_handle_identifier
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtool_ext_foreign_toplevel_list(
    __UNUSED__ int argc,
    __UNUSED__ const char **argv)
{
    struct wlmtool_toplevel_list_state tl_state = {};

    wlmcl_client_t *client_ptr = wlmcl_client_create("wlmaker.wlmtool");
    if (NULL == client_ptr) return false;

    wlmcl_client_register_interface(
        client_ptr,
        &ext_foreign_toplevel_list_v1_interface,
        /* desired_version */ 1,
        /* required */ true,
        _wlmtool_toplevel_setup,
        &tl_state);

    wlmcl_client_run(client_ptr);

    wlmcl_client_destroy(client_ptr);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Setup callback for binding the ext-foreign-toplevel-list-v1 interface. */
void _wlmtool_toplevel_setup(void *bound_interface_ptr, void *userdata_ptr)
{
    struct wlmtool_toplevel_list_state *tl_state_ptr = userdata_ptr;
    tl_state_ptr->list_ptr = bound_interface_ptr;
    ext_foreign_toplevel_list_v1_add_listener(
            tl_state_ptr->list_ptr,
            &_wlmtool_listener,
            tl_state_ptr);
}

/* ------------------------------------------------------------------------- */
/** Toplevel was added. */
void _wlmtool_handle_toplevel(
    __UNUSED__ void *data,
    __UNUSED__ struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1,
    struct ext_foreign_toplevel_handle_v1 *toplevel)
{
    struct wlmtool_toplevel_info *tl_info_ptr = logged_calloc(
        1, sizeof(*tl_info_ptr));
    if (NULL == tl_info_ptr) {
        ext_foreign_toplevel_handle_v1_destroy(toplevel);
        return;
    }

    ext_foreign_toplevel_handle_v1_add_listener(
        toplevel,
        &_toplevel_listener,
        tl_info_ptr);
}

/* ------------------------------------------------------------------------- */
/** Toplevel list is done, destroy. */
void _wlmtool_handle_finished(
    __UNUSED__ void *data,
    struct ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list_v1)
{
    ext_foreign_toplevel_list_v1_destroy(ext_foreign_toplevel_list_v1);
}

/* ------------------------------------------------------------------------- */
/** Toplevel closed. */
void _wlmtool_toplevel_handle_closed(
    __UNUSED__ void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    struct wlmtool_toplevel_info *tli = data;

    printf("Toplevel %p closed: app id %s, title %s, identifier %s\n",
           ext_foreign_toplevel_handle_v1,
           tli->app_id_ptr ? tli->app_id_ptr : "(none)",
           tli->title_ptr ? tli->title_ptr : "(none)",
           tli->identifier_ptr ? tli->identifier_ptr : "(none");
    fflush(stdout);
    ext_foreign_toplevel_handle_v1_destroy(ext_foreign_toplevel_handle_v1);

    if (tli->app_id_ptr) free(tli->app_id_ptr);
    if (tli->title_ptr) free(tli->title_ptr);
    if (tli->identifier_ptr) free(tli->identifier_ptr);
    free(tli);
}

/* ------------------------------------------------------------------------- */
/** Toplevel updates done. Print them. */
void _wlmtool_toplevel_handle_done(
    void *data,
    struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1)
{
    struct wlmtool_toplevel_info *tli = data;

    printf("Toplevel %p updated: app id %s, title %s, identifier %s\n",
           ext_foreign_toplevel_handle_v1,
           tli->app_id_ptr ? tli->app_id_ptr : "(none)",
           tli->title_ptr ? tli->title_ptr : "(none)",
           tli->identifier_ptr ? tli->identifier_ptr : "(none");
    fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/** Toplevel updates the title. */
void _wlmtool_toplevel_handle_title(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *title)
{
    struct wlmtool_toplevel_info *tl_info_ptr = data;

    if (NULL != tl_info_ptr->title_ptr) free(tl_info_ptr->title_ptr);
    tl_info_ptr->title_ptr = logged_strdup(title);
}

/* ------------------------------------------------------------------------- */
/** Toplevel updates the app id. */
void _wlmtool_toplevel_handle_app_id(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *app_id)
{
    struct wlmtool_toplevel_info *tl_info_ptr = data;

    if (NULL != tl_info_ptr->app_id_ptr) free(tl_info_ptr->app_id_ptr);
    tl_info_ptr->app_id_ptr = logged_strdup(app_id);
}

/* ------------------------------------------------------------------------- */
/** Toplevel updates the identifier. */
void _wlmtool_toplevel_handle_identifier(
    void *data,
    __UNUSED__ struct ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel_handle_v1,
    const char *identifier)
{
    struct wlmtool_toplevel_info *tl_info_ptr = data;

    if (NULL != tl_info_ptr->identifier_ptr) {
        bs_log(BS_WARNING, "Not permitting identifier change (%s -> %s)",
               tl_info_ptr->identifier_ptr,
               identifier);
        return;
    }
    tl_info_ptr->identifier_ptr = logged_strdup(identifier);
}

/* == End of toplevel_list.c =============================================== */
