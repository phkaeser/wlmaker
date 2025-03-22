/* ========================================================================= */
/**
 * @file background.c
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

#include "background.h"

#include <libbase/libbase.h>
#include <wayland-server-core.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Background state. */
struct _wlmaker_background_t {
    /** Links to layer. */
    wlmtk_layer_t              *layer_ptr;

    /** color of the background. */
    uint32_t                   color;

    /** Environment. */
    wlmtk_env_t                *env_ptr;
    /** The output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Holds outputs and respective panels. */
    bs_avltree_t              *output_tree_ptr;

    /** Event: Output layout changed. Parameter: struct wlr_box*. */
    struct wl_listener        output_layout_change_listener;
};

/** Background panel: The workspace's backgrund for the output. */
typedef struct  {
    /** A layer background for one output is a panel. */
    wlmtk_panel_t             super_panel;

    /** Initial implementation: The background is a uni-color rectangle. */
    wlmtk_rectangle_t         *rectangle_ptr;

    /** Tree node. Element of @ref wlmaker_background_t::output_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** The output covered by this background panel. */
    struct wlr_output         *wlr_output_ptr;
    /** Back-link to the background state. */
    wlmaker_background_t      *background_ptr;
} wlmaker_background_panel_t;

/** Arguent to @ref _wlmaker_background_update_output. */
typedef struct {
    /** The background we're working on. */
    wlmaker_background_t      *background_ptr;
    /** The former output tree. */
    bs_avltree_t              *former_output_tree_ptr;
} wlmaker_background_panel_update_arg_t;

static void _wlmaker_background_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmaker_background_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr);

static wlmaker_background_panel_t *_wlmaker_background_panel_create(
    wlmtk_layer_t *layer_ptr,
    struct wlr_output *wlr_output_ptr,
    uint32_t color,
    wlmtk_env_t *env_ptr);
static void _wlmaker_background_panel_destroy(
    wlmaker_background_panel_t *background_panel_ptr);
static int _wlmaker_background_panel_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr);
static void _wlmaker_background_panel_node_destroy(
    bs_avltree_node_t *avlnode_ptr);
static void _wlmaker_background_panel_element_destroy(
    wlmtk_element_t *element_ptr);
static uint32_t _wlmaker_background_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height);

/* == Data ================================================================= */

/** The background panel's element superclass virtual method. */
static const wlmtk_element_vmt_t _wlmaker_background_panel_element_vmt = {
    .destroy = _wlmaker_background_panel_element_destroy,
};

/** The background panels' virtual method table. */
static const wlmtk_panel_vmt_t _wlmaker_background_panel_vmt = {
    .request_size = _wlmaker_background_panel_request_size
};

/** Panel's position: Anchored to all 4 edges, and auto-sized. */
static const wlmtk_panel_positioning_t _wlmaker_background_panel_position = {
    .desired_width = 0,
    .desired_height = 0,
    .anchor = WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_background_t *wlmaker_background_create(
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    uint32_t color,
    wlmtk_env_t *env_ptr)
{
    wlmaker_background_t *background_ptr = logged_calloc(
        1, sizeof(wlmaker_background_t));
    if (NULL == background_ptr) return NULL;
    background_ptr->layer_ptr = wlmtk_workspace_get_layer(
        workspace_ptr, WLMTK_WORKSPACE_LAYER_BACKGROUND),

    background_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    background_ptr->color = color;
    background_ptr->env_ptr = env_ptr;

    background_ptr->output_tree_ptr = bs_avltree_create(
        _wlmaker_background_panel_node_cmp,
        _wlmaker_background_panel_node_destroy);
    if (NULL == background_ptr->output_tree_ptr) {
        wlmaker_background_destroy(background_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &background_ptr->output_layout_change_listener,
        _wlmaker_background_handle_output_layout_change);
    _wlmaker_background_handle_output_layout_change(
        &background_ptr->output_layout_change_listener,
        NULL);
    return background_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_background_destroy(wlmaker_background_t *background_ptr)
{
    wlmtk_util_disconnect_listener(
        &background_ptr->output_layout_change_listener);

    if (NULL != background_ptr->output_tree_ptr) {
        bs_avltree_destroy(background_ptr->output_tree_ptr);
        background_ptr->output_tree_ptr = NULL;
    }
    free(background_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the `change` event of wlr_output_layout::events.
 *
 * Walks through outputs of @ref wlmaker_background_t::wlr_output_layout_ptr,
 * and creates, removes or updates removes panels as needed.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_background_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_background_t *background_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_background_t, output_layout_change_listener);

    wlmaker_background_panel_update_arg_t arg = {
        .background_ptr = background_ptr,
        .former_output_tree_ptr = background_ptr->output_tree_ptr
    };

    background_ptr->output_tree_ptr = bs_avltree_create(
        _wlmaker_background_panel_node_cmp,
        _wlmaker_background_panel_node_destroy);
    BS_ASSERT(NULL != background_ptr->output_tree_ptr);

    BS_ASSERT(wlmtk_util_wl_list_for_each(
                  &background_ptr->wlr_output_layout_ptr->outputs,
                  _wlmaker_background_update_output,
                  &arg));

    bs_avltree_destroy(arg.former_output_tree_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the output.
 *
 * @param link_ptr            struct wlr_output_layout_output::link.
 * @param ud_ptr              @ref wlmaker_background_panel_update_arg_t.
 *
 * @return true on success, or false on error.
 */
bool _wlmaker_background_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_ptr->output;
    wlmaker_background_panel_update_arg_t *arg_ptr = ud_ptr;

    wlmaker_background_panel_t *background_panel_ptr = NULL;
    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        arg_ptr->former_output_tree_ptr, wlr_output_ptr);
    if (NULL != avlnode_ptr) {
        background_panel_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmaker_background_panel_t, avlnode);
    } else {
        background_panel_ptr = _wlmaker_background_panel_create(
            arg_ptr->background_ptr->layer_ptr,
            wlr_output_ptr,
            arg_ptr->background_ptr->color,
            arg_ptr->background_ptr->env_ptr);
        if (NULL == background_panel_ptr) return false;
    }

    return bs_avltree_insert(
        arg_ptr->background_ptr->output_tree_ptr,
        wlr_output_ptr,
        &background_panel_ptr->avlnode,
        false);
}

/* ------------------------------------------------------------------------- */
/** Ctor. */
wlmaker_background_panel_t *_wlmaker_background_panel_create(
    wlmtk_layer_t *layer_ptr,
    struct wlr_output *wlr_output_ptr,
    uint32_t color,
    wlmtk_env_t *env_ptr)
{
    wlmaker_background_panel_t *background_panel_ptr = logged_calloc(
        1, sizeof(wlmaker_background_panel_t));
    if (NULL == background_panel_ptr) return NULL;
    background_panel_ptr->wlr_output_ptr = wlr_output_ptr;

    if (!wlmtk_panel_init(&background_panel_ptr->super_panel,
                          &_wlmaker_background_panel_position,
                          env_ptr)) {
        _wlmaker_background_panel_node_destroy(
            &background_panel_ptr->avlnode);
        return NULL;
    }
    wlmtk_element_extend(
        &background_panel_ptr->super_panel.super_container.super_element,
        &_wlmaker_background_panel_element_vmt);
    wlmtk_panel_extend(&background_panel_ptr->super_panel,
                       &_wlmaker_background_panel_vmt);

    background_panel_ptr->rectangle_ptr = wlmtk_rectangle_create(
        env_ptr, 0, 0, color);
    if (NULL == background_panel_ptr->rectangle_ptr) {
        _wlmaker_background_panel_node_destroy(
            &background_panel_ptr->avlnode);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr),
        true);
    wlmtk_container_add_element(
        &background_panel_ptr->super_panel.super_container,
        wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr));

    wlmtk_element_set_visible(
        wlmtk_panel_element(&background_panel_ptr->super_panel),
        true);

    wlmtk_layer_add_panel_output(
        layer_ptr,
        &background_panel_ptr->super_panel,
        wlr_output_ptr);

    return background_panel_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor. */
void _wlmaker_background_panel_destroy(
    wlmaker_background_panel_t *background_panel_ptr)
{
    if (NULL != wlmtk_panel_get_layer(
            &background_panel_ptr->super_panel)) {
        wlmtk_layer_remove_panel(
            wlmtk_panel_get_layer(&background_panel_ptr->super_panel),
            &background_panel_ptr->super_panel);
    }

    if (NULL != background_panel_ptr->rectangle_ptr) {
        wlmtk_container_remove_element(
            &background_panel_ptr->super_panel.super_container,
            wlmtk_rectangle_element(background_panel_ptr->rectangle_ptr));

        wlmtk_rectangle_destroy(background_panel_ptr->rectangle_ptr);
        background_panel_ptr->rectangle_ptr = NULL;
    }

    wlmtk_panel_fini(&background_panel_ptr->super_panel);

    free(background_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for @ref wlmaker_background_t::output_tree_ptr. */
int _wlmaker_background_panel_node_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr)
{
    wlmaker_background_panel_t *background_panel_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_background_panel_t, avlnode);
    return bs_avltree_cmp_ptr(background_panel_ptr->wlr_output_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_element_vmt_t::destroy. Dtor for the panel. */
void _wlmaker_background_panel_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmaker_background_panel_t *background_panel_ptr = BS_CONTAINER_OF(
        element_ptr,
        wlmaker_background_panel_t,
        super_panel.super_container.super_element);

    if (NULL != background_panel_ptr->background_ptr) {
        bs_avltree_delete(
            background_panel_ptr->background_ptr->output_tree_ptr,
            background_panel_ptr->wlr_output_ptr);
    }
    _wlmaker_background_panel_destroy(background_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destructor for @ref wlmaker_background_panel_t. */
void _wlmaker_background_panel_node_destroy(
    bs_avltree_node_t *avlnode_ptr)
{
    wlmaker_background_panel_t *background_panel_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_background_panel_t, avlnode);
    _wlmaker_background_panel_destroy(background_panel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_panel_vmt_t::request_size. Updates the panel size. */
uint32_t _wlmaker_background_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height)
{
    wlmaker_background_panel_t *background_ptr = BS_CONTAINER_OF(
        panel_ptr, wlmaker_background_panel_t, super_panel);

    wlmtk_rectangle_set_size(background_ptr->rectangle_ptr, width, height);

    wlmtk_panel_commit(
        &background_ptr->super_panel, 0,
        &_wlmaker_background_panel_position);
    return 0;
}

/* == End of background.c ================================================== */
