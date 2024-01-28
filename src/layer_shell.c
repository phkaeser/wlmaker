/* ========================================================================= */
/**
 * @file layer_shell.c
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

#include "layer_shell.h"

#include "layer_surface.h"
#include "toolkit/toolkit.h"

#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_layer_shell_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of Layer Shell handler. */
struct _wlmaker_layer_shell_t {
    /** wlroots Layer Shell v1 handler. */
    struct wlr_layer_shell_v1 *wlr_layer_shell_v1_ptr;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Listener for the `new_surface` signal raised by `wlr_layer_shell_v1`. */
    struct wl_listener        new_surface_listener;
    /** Listener for the `destroy` signal raised by `wlr_layer_shell_v1`. */
    struct wl_listener        destroy_listener;
};

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_layer_shell_t *wlmaker_layer_shell_create(wlmaker_server_t *server_ptr)
{
    wlmaker_layer_shell_t *layer_shell_ptr = logged_calloc(
        1, sizeof(wlmaker_layer_shell_t));
    if (NULL == layer_shell_ptr) return NULL;
    layer_shell_ptr->server_ptr = server_ptr;

    layer_shell_ptr->wlr_layer_shell_v1_ptr = wlr_layer_shell_v1_create(
        server_ptr->wl_display_ptr, 4  /* version */);
    if (NULL == layer_shell_ptr->wlr_layer_shell_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_layer_shell_v1_create()");
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &layer_shell_ptr->wlr_layer_shell_v1_ptr->events.new_surface,
        &layer_shell_ptr->new_surface_listener,
        handle_new_surface);
    wlmtk_util_connect_listener_signal(
        &layer_shell_ptr->wlr_layer_shell_v1_ptr->events.destroy,
        &layer_shell_ptr->destroy_listener,
        handle_destroy);

    return layer_shell_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_layer_shell_destroy(wlmaker_layer_shell_t *layer_shell_ptr)
{
    wl_list_remove(&layer_shell_ptr->destroy_listener.link);
    wl_list_remove(&layer_shell_ptr->new_surface_listener.link);
    free(layer_shell_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_layer_shell_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_shell_t *layer_shell_ptr = wl_container_of(
        listener_ptr, layer_shell_ptr, destroy_listener);

    wlmaker_layer_shell_destroy(layer_shell_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_surface` signal raised by `wlr_layer_shell_v1`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_layer_shell_t *layer_shell_ptr = wl_container_of(
        listener_ptr, layer_shell_ptr, new_surface_listener);
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr = data_ptr;

    if (NULL == wlr_layer_surface_v1_ptr->output) {
        wlr_layer_surface_v1_ptr->output = wlmaker_server_get_output_at_cursor(
            layer_shell_ptr->server_ptr);
    }

    __UNUSED__ wlmaker_layer_surface_t *layer_surface_ptr =
        wlmaker_layer_surface_create(
            wlr_layer_surface_v1_ptr,
            layer_shell_ptr->server_ptr);
}

/* == End of layer_shell.c ================================================= */
