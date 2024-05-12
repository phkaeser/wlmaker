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

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "xdg_popup.h"

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

    /** Listener for the `commit` signal raised by `wlr_surface`. */
    struct wl_listener        surface_commit_listener;

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

static bool _wlmaker_layer_panel_apply_keyboard(
    wlmaker_layer_panel_t *layer_panel_ptr,
    enum zwlr_layer_surface_v1_keyboard_interactivity interactivity);
static bool _wlmaker_layer_panel_apply_layer(
    wlmaker_layer_panel_t *layer_panel_ptr,
    enum zwlr_layer_shell_v1_layer zwlr_layer);
static void _wlmaker_layer_panel_set_positioning(
    wlmtk_panel_positioning_t *positioning_ptr,
    const struct wlr_layer_surface_v1_state *state_ptr);

static uint32_t _wlmaker_layer_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

static void _wlmaker_layer_panel_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
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

/* == Data ================================================================= */

/** Virtual method table for the layer panel. */
static const wlmtk_panel_vmt_t _wlmtk_layer_panel_vmt = {
    .request_size = _wlmaker_layer_panel_request_size,
};

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

    wlmtk_panel_positioning_t pos;
    _wlmaker_layer_panel_set_positioning(
        &pos, &layer_panel_ptr->wlr_layer_surface_v1_ptr->pending);
    if (!wlmtk_panel_init(
            &layer_panel_ptr->super_panel, &pos, server_ptr->env_ptr)) {
        _wlmaker_layer_panel_destroy(layer_panel_ptr);
        return NULL;
    }
    wlmtk_panel_extend(
        &layer_panel_ptr->super_panel,
        &_wlmtk_layer_panel_vmt);

    layer_panel_ptr->wlmtk_surface_ptr = wlmtk_surface_create_fn(
        wlr_layer_surface_v1_ptr->surface,
        server_ptr->env_ptr);
    if (NULL == layer_panel_ptr->wlmtk_surface_ptr) {
        _wlmaker_layer_panel_destroy(layer_panel_ptr);
        return NULL;
    }
    wlmtk_container_add_element_atop(
        &layer_panel_ptr->super_panel.super_container,
        NULL,
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
        &wlr_layer_surface_v1_ptr->surface->events.commit,
        &layer_panel_ptr->surface_commit_listener,
        _wlmaker_layer_panel_handle_surface_commit);

    wlmtk_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.destroy,
        &layer_panel_ptr->destroy_listener,
        _wlmaker_layer_panel_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_layer_surface_v1_ptr->events.new_popup,
        &layer_panel_ptr->new_popup_listener,
        _wlmaker_layer_panel_handle_new_popup);

    if (!_wlmaker_layer_panel_apply_keyboard(
            layer_panel_ptr,
            wlr_layer_surface_v1_ptr->pending.keyboard_interactive) ||
        !_wlmaker_layer_panel_apply_layer(
            layer_panel_ptr,
            layer_panel_ptr->wlr_layer_surface_v1_ptr->pending.layer)) {
        _wlmaker_layer_panel_destroy(layer_panel_ptr);
        return NULL;
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

    wlmtk_layer_t *layer_ptr = wlmtk_panel_get_layer(
        &layer_panel_ptr->super_panel);
    if (NULL != layer_ptr) {
        wlmtk_layer_remove_panel(layer_ptr, &layer_panel_ptr->super_panel);
    }

    wlmtk_util_disconnect_listener(&layer_panel_ptr->new_popup_listener);
    wlmtk_util_disconnect_listener(&layer_panel_ptr->destroy_listener);

    wlmtk_util_disconnect_listener(&layer_panel_ptr->surface_commit_listener);

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
/** @return Layer number, translated from protocol value. -1 on error. */
wlmtk_workspace_layer_t _wlmaker_layer_from_zwlr_layer(
    enum zwlr_layer_shell_v1_layer zwlr_layer)
{
    wlmtk_workspace_layer_t layer;
    switch (zwlr_layer) {
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
        layer = -1;
        break;
    }
    return layer;
}

/* ------------------------------------------------------------------------- */
/** Applies the requested keyboard setting. Currently warns on non-zero. */
bool _wlmaker_layer_panel_apply_keyboard(
    wlmaker_layer_panel_t *layer_panel_ptr,
    enum zwlr_layer_surface_v1_keyboard_interactivity interactivity)
{
    if (ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE != interactivity) {
        wl_resource_post_error(
            layer_panel_ptr->wlr_layer_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_IMPLEMENTATION,
            "Unsupported setting for keyboard interactivity: %d",
            interactivity);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates the layer this panel is part of. Posts an error if invalid. */
bool _wlmaker_layer_panel_apply_layer(
    wlmaker_layer_panel_t *layer_panel_ptr,
    enum zwlr_layer_shell_v1_layer zwlr_layer)
{
    wlmtk_workspace_layer_t layer;
    switch (zwlr_layer) {
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
            "Invalid value for for zwlr_layer value: %d",
            layer_panel_ptr->wlr_layer_surface_v1_ptr->pending.layer);
        return false;
    }

    wlmaker_workspace_t *workspace_ptr = wlmaker_server_get_current_workspace(
        layer_panel_ptr->server_ptr);
    wlmtk_workspace_t *wlmtk_workspace_ptr = wlmaker_workspace_wlmtk(
        workspace_ptr);
    wlmtk_layer_t *layer_ptr = wlmtk_workspace_get_layer(
        wlmtk_workspace_ptr, layer);

    wlmtk_layer_t *current_layer_ptr = wlmtk_panel_get_layer(
        &layer_panel_ptr->super_panel);
    if (layer_ptr == current_layer_ptr) return true;

    if (NULL != current_layer_ptr) {
        wlmtk_layer_remove_panel(
            current_layer_ptr, &layer_panel_ptr->super_panel);
    }

    if (NULL != layer_ptr) {
        wlmtk_layer_add_panel(
            layer_ptr, &layer_panel_ptr->super_panel);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Updates `positioning_ptr` from the given surface state. */
void _wlmaker_layer_panel_set_positioning(
    wlmtk_panel_positioning_t *positioning_ptr,
    const struct wlr_layer_surface_v1_state *state_ptr)
{
    positioning_ptr->anchor = state_ptr->anchor;

    positioning_ptr->desired_width = state_ptr->desired_width;
    positioning_ptr->desired_height = state_ptr->desired_height;

    positioning_ptr->margin_left = state_ptr->margin.left;
    positioning_ptr->margin_top = state_ptr->margin.top;
    positioning_ptr->margin_right = state_ptr->margin.right;
    positioning_ptr->margin_bottom = state_ptr->margin.bottom;

    positioning_ptr->exclusive_zone = state_ptr->exclusive_zone;
}

/* ------------------------------------------------------------------------- */
/** Implements wlmtk_panel_vmt_t::request_size. */
uint32_t _wlmaker_layer_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        panel_ptr, wlmaker_layer_panel_t, super_panel);

    return wlr_layer_surface_v1_configure(
        layer_panel_ptr->wlr_layer_surface_v1_ptr, width, height);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal of `wlr_surface`.
 *
 * Updates positioning and layer of the panel, as required.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_layer_panel_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_layer_panel_t *layer_panel_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_layer_panel_t, surface_commit_listener);

    struct wlr_layer_surface_v1_state *state_ptr =
        &layer_panel_ptr->wlr_layer_surface_v1_ptr->pending;

    wlmtk_panel_positioning_t pos;
    _wlmaker_layer_panel_set_positioning(&pos, state_ptr);
    wlmtk_panel_commit(
        &layer_panel_ptr->super_panel,
        state_ptr->configure_serial,
        &pos);

    // Updates keyboard and layer values. Ignore failures here.
    _wlmaker_layer_panel_apply_keyboard(
        layer_panel_ptr,
        state_ptr->keyboard_interactive);
    _wlmaker_layer_panel_apply_layer(
        layer_panel_ptr,
        state_ptr->layer);
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

    wlmtk_element_set_visible(
        wlmtk_panel_element(&layer_panel_ptr->super_panel), true);
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

    wlmtk_element_set_visible(
        wlmtk_panel_element(&layer_panel_ptr->super_panel), false);
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

    wlmaker_xdg_popup_t *popup_ptr = wlmaker_xdg_popup2_create(
        wlr_xdg_popup_ptr,
        layer_panel_ptr->server_ptr->env_ptr);
    if (NULL == popup_ptr) {
        wl_resource_post_error(
            wlr_xdg_popup_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmtk_xdg_popup2_create.");
        return;
    }

    wlmtk_element_set_visible(
        wlmtk_popup_element(&popup_ptr->super_popup), true);
    wlmtk_pubase_add_popup(
        &layer_panel_ptr->super_panel.pubase,
        &popup_ptr->super_popup);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_layer_panel_test_cases[] = {
    { 0, "create_destroy", test_create_destroy },
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
