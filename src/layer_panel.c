/* ========================================================================= */
/**
 * @file layer_panel.c
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

#include "layer_panel.h"

/* == Declarations ========================================================= */

/** State of a layer panel. */
struct _wlmaker_layer_panel_t {
    /** We're deriving this from a @ref wlmtk_panel_t as superclass. */
    wlmtk_panel_t             super_panel;

    /** Links to the wlroots layer surface for this panel. */
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr;
    /** Back-link to @ref wlmaker_server_t. */
    wlmaker_server_t          *server_ptr;

    /** The wrapped surface, will be the principal element of the panel. */
    wlmtk_surface_t           *wlmtk_surface_ptr;
    /** Listener for the `map` signal raised by `wlmtk_surface_t`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `unmap` signal raised by `wlmtk_surface_t`. */
    struct wl_listener        surface_unmap_listener;

    /** Listener for the `destroy` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for `new_popup` signal raised by `wlr_layer_surface_v1`. */
    struct wl_listener        new_popup_listener;
};

static wlmaker_layer_panel_t *_wlmaker_layer_panel_create_injected(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr,
    wlmtk_surface_create_t wlmtk_surface_create_fn);
static void _wlmaker_layer_panel_destroy(
    wlmaker_layer_panel_t *layer_panel_ptr);

static void _wlmaker_layer_panel_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_layer_panel_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_layer_panel_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_layer_panel_handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_layer_panel_t *wlmaker_layer_panel_create(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr)
{
    return _wlmaker_layer_panel_create_injected(
        wlr_layer_surface_v1_ptr,
        server_ptr,
        wlmtk_surface_create);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Constructor for the layer panel, with injectable methods.
 *
 * @param wlr_layer_surface_v1_ptr
 * @param server_ptr
 * @param wlmtk_surface_create_fn
 *
 * @return The handler for the layer surface or NULL on error.
 */
wlmaker_layer_panel_t *_wlmaker_layer_panel_create_injected(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr,
    wlmtk_surface_create_t wlmtk_surface_create_fn)
{
    wlmaker_layer_panel_t *layer_panel_ptr = logged_calloc(
        1, sizeof(wlmaker_layer_panel_t));
    if (NULL == layer_panel_ptr) return NULL;
    layer_panel_ptr->wlr_layer_surface_v1_ptr = wlr_layer_surface_v1_ptr;
    layer_panel_ptr->server_ptr = server_ptr;

    if (!wlmtk_panel_init(&layer_panel_ptr->super_panel,
                          server_ptr->env_ptr)) {
        _wlmaker_layer_panel_destroy(layer_panel_ptr);
        return NULL;
    }

    layer_panel_ptr->wlmtk_surface_ptr = wlmtk_surface_create_fn(
        wlr_layer_surface_v1_ptr->surface,
        server_ptr->env_ptr);
    if (NULL == layer_panel_ptr->wlmtk_surface_ptr) {
        _wlmaker_layer_panel_destroy(layer_panel_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &layer_panel_ptr->super_panel.super_container,
        wlmtk_surface_element(layer_panel_ptr->wlmtk_surface_ptr));
    wlmtk_element_set_visible(
        wlmtk_surface_element(layer_panel_ptr->wlmtk_surface_ptr), true);
    wlmtk_surface_connect_map_listener_signal(
        layer_panel_ptr->wlmtk_surface_ptr,
        &layer_panel_ptr->surface_map_listener,
        _wlmaker_layer_panel_handle_surface_map);
    wlmtk_surface_connect_unmap_listener_signal(
        layer_panel_ptr->wlmtk_surface_ptr,
        &layer_panel_ptr->surface_unmap_listener,
        _wlmaker_layer_panel_handle_surface_unmap);

    wlmtk_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.destroy,
        &layer_panel_ptr->destroy_listener,
        _wlmaker_layer_panel_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.new_popup,
        &layer_panel_ptr->new_popup_listener,
        _wlmaker_layer_panel_handle_new_popup);

    // FIXME: Should actually compute the available size based on the layer's
    // current population of panels. Which layer, though?
    //
    // It has 'desired_width', 'desired_height', layer, exclusive, anchor,
    // margin => in 'pending'.
    if (NULL != server_ptr->wlr_output_layout_ptr) {
        struct wlr_box extents;
        wlr_output_layout_get_box(
            server_ptr->wlr_output_layout_ptr, NULL, &extents);
        wlr_layer_surface_v1_configure(
            wlr_layer_surface_v1_ptr, extents.width, extents.height);
    }

    bs_log(BS_INFO, "Created layer panel %p with wlmtk surface %p",
           layer_panel_ptr, layer_panel_ptr->wlmtk_surface_ptr);
    return layer_panel_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the layer panel and frees up all associated resources.
 *
 * @param layer_panel_ptr
 */
void _wlmaker_layer_panel_destroy(wlmaker_layer_panel_t *layer_panel_ptr)
{
    bs_log(BS_INFO, "Destroying layer panel %p with wlmtk surface %p",
           layer_panel_ptr, layer_panel_ptr->wlmtk_surface_ptr);

    wlmtk_util_disconnect_listener(&layer_panel_ptr->destroy_listener);

    wlmtk_util_disconnect_listener(&layer_panel_ptr->surface_unmap_listener);
    wlmtk_util_disconnect_listener(&layer_panel_ptr->surface_map_listener);
    if (NULL != layer_panel_ptr->wlmtk_surface_ptr) {
        wlmtk_container_remove_element(
            &layer_panel_ptr->super_panel.super_container,
            wlmtk_surface_element(layer_panel_ptr->wlmtk_surface_ptr));

        wlmtk_surface_destroy(layer_panel_ptr->wlmtk_surface_ptr);
        layer_panel_ptr->wlmtk_surface_ptr = NULL;
    }

    wlmtk_panel_fini(&layer_panel_ptr->super_panel);
    free(layer_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal of `wlmtk_surface_t`: Maps the panel to layer.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_layer_panel_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_layer_panel_t, surface_map_listener);

    wlmtk_workspace_layer_t layer;
    switch (layer_panel_ptr->wlr_layer_surface_v1_ptr->current.layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
        layer = WLMTK_WORKSPACE_LAYER_BACKGROUND;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        layer = WLMTK_WORKSPACE_LAYER_BOTTOM;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        layer = WLMTK_WORKSPACE_LAYER_TOP;
        break;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        layer = WLMTK_WORKSPACE_LAYER_OVERLAY;
        break;
    default:
        wl_resource_post_error(
            layer_panel_ptr->wlr_layer_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Invalid layer value: %d",
            layer_panel_ptr->wlr_layer_surface_v1_ptr->current.layer);
        return;
    }

    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        layer_panel_ptr->server_ptr);
    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        workspace_ptr);

    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        wlmtk_workspace_ptr, layer);
    BS_ASSERT(NULL != layer_ptr);

    wlmtk_element_set_visible(
        wlmtk_panel_element(&layer_panel_ptr->super_panel), true);
    wlmtk_layer_add_panel(layer_ptr, &layer_panel_ptr->super_panel);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unmap` signal of `wlmtk_surface_t`: Unmaps the panel.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_layer_panel_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_layer_panel_t, surface_unmap_listener);

    wlmtk_layer_t *layer_ptr = wlmtk_panel_get_layer(
        &layer_panel_ptr->super_panel);
    if (NULL == layer_ptr) return;

    wlmtk_layer_remove_panel(layer_ptr, &layer_panel_ptr->super_panel);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of the `wlr_layer_surface_v1`: Destroys
 * the panel.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_layer_panel_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_layer_panel_t, destroy_listener);

    _wlmaker_layer_panel_destroy(layer_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_popup` signal of the `wlr_layer_surface_v1`: Creates
 * a new popup for this panel.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the new `struct wlr_xdg_popup`.
 */
void _wlmaker_layer_panel_handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_layer_panel_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    bs_log(BS_WARNING, "FIXME: Unimplemented new_popup %p for panel %p",
           wlr_xdg_popup_ptr, layer_panel_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_layer_panel_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises creation and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    struct wlr_layer_surface_v1 wlr_layer_surface_v1 = {};
    wlmaker_server_t server = {};

    // Inject: wlmtk_surface_create - fake_surface_create.

    wl_signal_init(&wlr_layer_surface_v1.events.destroy);
    wl_signal_init(&wlr_layer_surface_v1.events.new_popup);

    wlmaker_layer_panel_t *layer_panel_ptr =
        _wlmaker_layer_panel_create_injected(
            &wlr_layer_surface_v1,
            &server,
            wlmtk_fake_surface_create_inject);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, layer_panel_ptr);

    wl_signal_emit(&wlr_layer_surface_v1.events.destroy, NULL);
}

/* == End of layer_panel.c ================================================== */
