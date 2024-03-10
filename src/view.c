/* ========================================================================= */
/**
 * @file view.c
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

#include "view.h"

#include "button.h"
#include "config.h"
#include "decorations.h"
#include "menu.h"
#include "toolkit/toolkit.h"

#include <wlr/util/edges.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void handle_button_release(struct wl_listener *listener_ptr,
                                  void *data_ptr);

static void update_pointer_focus(wlmaker_view_t *view_ptr,
                                 struct wlr_scene_node *wlr_scene_node_ptr);

static void window_menu_callback_maximize(void *ud_ptr);
static void window_menu_callback_fullscreen(void *ud_ptr);
static void window_menu_callback_minimize(void *ud_ptr);
static void window_menu_callback_shade(void *ud_ptr);
static void window_menu_callback_move_to_workspace1(void *ud_ptr);
static void window_menu_callback_move_to_workspace2(void *ud_ptr);
static void window_menu_callback_close(void *ud_ptr);

/* == Data ================================================================= */

/** Descriptors for the menu entries of the view's "Window menu". */
static const wlmaker_menu_item_descriptor_t window_menu_descriptors[] = {
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Maximize", window_menu_callback_maximize),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Fullscreen", window_menu_callback_fullscreen),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Minimize", window_menu_callback_minimize),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Shade", window_menu_callback_shade),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Move to workspace 1", window_menu_callback_move_to_workspace1),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Move to workspace 2", window_menu_callback_move_to_workspace2),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(
        "Close", window_menu_callback_close),
    WLMAKER_MENU_ITEM_DESCRIPTOR_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmaker_view_init(
    wlmaker_view_t *view_ptr,
    const wlmaker_view_impl_t *view_impl_ptr,
    wlmaker_server_t *server_ptr,
    struct wlr_surface *wlr_surface_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_view_send_close_callback_t send_close_callback)
{
    memset(view_ptr, 0, sizeof(wlmaker_view_t));
    BS_ASSERT(NULL != view_impl_ptr);
    view_ptr->impl_ptr = view_impl_ptr;
    view_ptr->server_ptr = server_ptr;
    view_ptr->wlr_surface_ptr = wlr_surface_ptr;

    view_ptr->elements_wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr->node.parent);
    if (NULL == view_ptr->elements_wlr_scene_tree_ptr) {
        wlmaker_view_fini(view_ptr);
        return;
    }
    view_ptr->elements_wlr_scene_tree_ptr->node.data = view_ptr;
    wlr_scene_node_reparent(
        &wlr_scene_tree_ptr->node, view_ptr->elements_wlr_scene_tree_ptr);
    wlr_scene_tree_ptr->node.data = view_ptr;
    view_ptr->view_wlr_scene_tree_ptr = wlr_scene_tree_ptr;

    view_ptr->send_close_callback = send_close_callback;

    view_ptr->interactive_tree_ptr = bs_avltree_create(
        wlmaker_interactive_node_cmp,
        wlmaker_interactive_node_destroy);
    BS_ASSERT(view_ptr->interactive_tree_ptr);

    wlmtk_util_connect_listener_signal(
        &view_ptr->server_ptr->cursor_ptr->button_release_event,
        &view_ptr->button_release_listener,
        handle_button_release);

    if (NULL != wlr_surface_ptr) {
        wl_client_get_credentials(
            wlr_surface_ptr->resource->client,
            &view_ptr->client.pid,
            &view_ptr->client.uid,
            &view_ptr->client.gid);
    }

    wl_signal_emit(&server_ptr->view_created_event, view_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_fini(wlmaker_view_t *view_ptr)
{
    // In case the view is still mapped: Unmap first.
    if (NULL != view_ptr->workspace_ptr) {
        wlmaker_view_unmap(view_ptr);
    }

    wl_signal_emit(&view_ptr->server_ptr->view_destroyed_event, view_ptr);

    wl_list_remove(&view_ptr->button_release_listener.link);

    if (NULL != view_ptr->title_ptr) {
        free(view_ptr->title_ptr);
        view_ptr->title_ptr = NULL;
    }

    if (NULL != view_ptr->app_id_ptr) {
        free(view_ptr->app_id_ptr);
        view_ptr->app_id_ptr = NULL;
    }

    if (NULL != view_ptr->interactive_tree_ptr) {
        // Will also destroy all interactives in the three.
        bs_avltree_destroy(view_ptr->interactive_tree_ptr);
        view_ptr->interactive_tree_ptr = NULL;
    }

    if (NULL != view_ptr->elements_wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(&view_ptr->elements_wlr_scene_tree_ptr->node);
        view_ptr->elements_wlr_scene_tree_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_active(wlmaker_view_t *view_ptr, bool active)
{
    // Ignore the call for views that cannot be (de)activated.
    if (NULL == view_ptr->impl_ptr->set_activated) return;
    view_ptr->impl_ptr->set_activated(view_ptr, active);

    bs_avltree_node_t *avl_node_ptr;
    for (avl_node_ptr = bs_avltree_min(view_ptr->interactive_tree_ptr);
         avl_node_ptr != NULL;
         avl_node_ptr = bs_avltree_node_next(view_ptr->interactive_tree_ptr,
                                             avl_node_ptr)) {
        wlmaker_interactive_t *interactive_ptr = wlmaker_interactive_from_avlnode(
            avl_node_ptr);
        wlmaker_interactive_focus(interactive_ptr, active);
    }

    if (active) {
        struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
            view_ptr->server_ptr->wlr_seat_ptr);
        if (NULL != wlr_keyboard_ptr) {
            wlr_seat_keyboard_notify_enter(
                view_ptr->server_ptr->wlr_seat_ptr,
                wlmaker_view_get_wlr_surface(view_ptr),
                wlr_keyboard_ptr->keycodes,
                wlr_keyboard_ptr->num_keycodes,
                &wlr_keyboard_ptr->modifiers);
        }
    } else {
        struct wlr_surface *focussed_surface_ptr = NULL;
        struct wlr_seat *seat_ptr = view_ptr->server_ptr->wlr_seat_ptr;
        if (NULL != seat_ptr) {
            focussed_surface_ptr = seat_ptr->keyboard_state.focused_surface;
        }

        if (view_ptr->active) {
            BS_ASSERT(focussed_surface_ptr ==
                      wlmaker_view_get_wlr_surface(view_ptr));
            wlr_seat_keyboard_notify_clear_focus(
                view_ptr->server_ptr->wlr_seat_ptr);
        } else {
            BS_ASSERT(focussed_surface_ptr !=
                      wlmaker_view_get_wlr_surface(view_ptr));
        }
    }
    view_ptr->active = active;
}

/* ------------------------------------------------------------------------- */
wlmaker_view_t *wlmaker_view_from_dlnode(bs_dllist_node_t *node_ptr)
{
    return BS_CONTAINER_OF(node_ptr, wlmaker_view_t, views_node);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_view(wlmaker_view_t *view_ptr)
{
    return &view_ptr->views_node;
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_view(
    wlmaker_view_t *view_ptr)
{
    return &view_ptr->elements_wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
struct wlr_surface *wlmaker_view_get_wlr_surface(wlmaker_view_t *view_ptr)
{
    return view_ptr->wlr_surface_ptr;
}

/* ------------------------------------------------------------------------- */
wlmaker_view_t *wlmaker_view_at(
    wlmaker_server_t *server_ptr,
    double x,
    double y,
    struct wlr_surface **wlr_surface_ptr_ptr,
    double *rel_x_ptr,
    double *rel_y_ptr)
{
    struct wlr_scene_node *wlr_scene_node_ptr = wlr_scene_node_at(
        &server_ptr->wlr_scene_ptr->tree.node, x, y, rel_x_ptr, rel_y_ptr);
    if (NULL == wlr_scene_node_ptr ||
        WLR_SCENE_NODE_BUFFER != wlr_scene_node_ptr->type) {
        return NULL;
    }

    struct wlr_scene_buffer *wlr_scene_buffer_ptr =
        wlr_scene_buffer_from_node(wlr_scene_node_ptr);
    struct wlr_scene_surface *wlr_scene_surface_ptr =
        wlr_scene_surface_try_from_buffer(wlr_scene_buffer_ptr);
    if (NULL == wlr_scene_surface_ptr) {
        if (NULL != wlr_scene_node_ptr->data) {
            // For server-side decoration control surfaces (buffers), we also
            // set |data| to the view. So that events can propagate there.
            return (wlmaker_view_t*)wlr_scene_node_ptr->data;
        }
        return NULL;
    }
    *wlr_surface_ptr_ptr = wlr_scene_surface_ptr->surface;

    // Step up the tree to find the anchoring view. The node.data field is set
    // only for the node we initialized in wlmaker_view_init().
    struct wlr_scene_tree *wlr_scene_tree_ptr = wlr_scene_node_ptr->parent;
    while (NULL != wlr_scene_tree_ptr &&
           NULL == wlr_scene_tree_ptr->node.data) {
        wlr_scene_tree_ptr = wlr_scene_tree_ptr->node.parent;
    }
    if (NULL == wlr_scene_tree_ptr) return NULL;

    return (wlmaker_view_t*)wlr_scene_tree_ptr->node.data;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_handle_motion(wlmaker_view_t *view_ptr,
                                double x,
                                double y)
{
    double rel_x, rel_y;
    struct wlr_scene_node *wlr_scene_node_ptr = wlr_scene_node_at(
        &view_ptr->elements_wlr_scene_tree_ptr->node, x, y, &rel_x, &rel_y);

    update_pointer_focus(view_ptr, wlr_scene_node_ptr);

    bs_avltree_node_t *avl_node_ptr = bs_avltree_lookup(
        view_ptr->interactive_tree_ptr, wlr_scene_node_ptr);
    if (NULL != avl_node_ptr) {
        wlmaker_interactive_motion(
            wlmaker_interactive_from_avlnode(avl_node_ptr), rel_x, rel_y);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_handle_button(wlmaker_view_t *view_ptr,
                                double x,
                                double y,
                                struct wlr_pointer_button_event *event_ptr)
{
    double rel_x, rel_y;
    struct wlr_scene_node *wlr_scene_node_ptr = wlr_scene_node_at(
        &view_ptr->elements_wlr_scene_tree_ptr->node, x, y, &rel_x, &rel_y);

    update_pointer_focus(view_ptr, wlr_scene_node_ptr);

    bs_avltree_node_t *avl_node_ptr = bs_avltree_lookup(
        view_ptr->interactive_tree_ptr, wlr_scene_node_ptr);
    if (NULL != avl_node_ptr) {
        wlmaker_interactive_button(
            wlmaker_interactive_from_avlnode(avl_node_ptr),
            rel_x, rel_y, event_ptr);
    }

    if (WLR_BUTTON_PRESSED == event_ptr->state &&
        NULL != view_ptr->impl_ptr->set_activated) {
        // TODO(kaeser@gubbe.ch): Not every click needs to trigger a raise.
        wlmaker_workspace_raise_view(view_ptr->workspace_ptr, view_ptr);
        wlmaker_workspace_activate_view(view_ptr->workspace_ptr, view_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_handle_axis(
    wlmaker_view_t *view_ptr,
    __UNUSED__ double x,
    __UNUSED__ double y,
    struct wlr_pointer_axis_event *event_ptr)
{
    if (NULL != view_ptr->impl_ptr->handle_axis) {
        view_ptr->impl_ptr->handle_axis(view_ptr, event_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_window_menu_show(wlmaker_view_t *view_ptr)
{
    if (NULL != view_ptr->window_menu_wlr_scene_buffer_ptr) return;

    view_ptr->window_menu_wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        view_ptr->elements_wlr_scene_tree_ptr, NULL);
    if (NULL == view_ptr->window_menu_wlr_scene_buffer_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_buffer_create(%p, NULL)",
               view_ptr->elements_wlr_scene_tree_ptr);
        return;
    }

    wlmaker_interactive_t *interactive_ptr = wlmaker_menu_create(
        view_ptr->window_menu_wlr_scene_buffer_ptr,
        view_ptr->server_ptr->cursor_ptr,
        view_ptr,
        window_menu_descriptors,
        view_ptr);  // callback_ud_ptr.
    if (NULL == interactive_ptr) {
        wlr_scene_node_destroy(
            &view_ptr->window_menu_wlr_scene_buffer_ptr->node);
        view_ptr->window_menu_wlr_scene_buffer_ptr = NULL;
        return;
    }

    // We just created the node. A collision in the interactive_tree_ptr would
    // indicate we have a serious corruption issue.
    bool inserted = bs_avltree_insert(
        view_ptr->interactive_tree_ptr,
        &interactive_ptr->wlr_scene_buffer_ptr->node,
        &interactive_ptr->avlnode,
        false);
    BS_ASSERT(inserted);

    interactive_ptr->wlr_scene_buffer_ptr->node.data = view_ptr;
    wlr_scene_node_set_enabled(
        &interactive_ptr->wlr_scene_buffer_ptr->node,
        true);
    wlr_scene_node_raise_to_top(
        &interactive_ptr->wlr_scene_buffer_ptr->node);

    // Menu placement: Just below the title bar, centered on the pointer
    // position. Attempt to bound it by the window dimensions - but may stretch
    // beyond the right window border, if the window is too narrow.
    // TODO(kaeser@gubbe.ch): An ugly piece. Clean this up.
    int x, y;
    wlmaker_view_get_position(view_ptr, &x, &y);
    uint32_t view_width;
    wlmaker_view_get_size(view_ptr, &view_width, NULL);

    double cursor_x, cursor_y;
    wlmaker_cursor_get_position(
        view_ptr->server_ptr->cursor_ptr, &cursor_x, &cursor_y);

    uint32_t menu_width;
    wlmaker_menu_get_size(interactive_ptr, &menu_width, NULL);

    int desired_x = cursor_x - menu_width / 2.0;
    if (desired_x + menu_width > x + view_width) {
        desired_x = x + view_width - menu_width;
    }
    desired_x = BS_MAX(x, desired_x);
    wlr_scene_node_set_position(
        &interactive_ptr->wlr_scene_buffer_ptr->node,
        desired_x - x, 0);

    // The window menu can be added anytime, so we need to inform the
    // interactive about the current state of "pointer-focussedness".
    wlmaker_interactive_focus(interactive_ptr, view_ptr->active);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_window_menu_hide(wlmaker_view_t *view_ptr)
{
    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        view_ptr->interactive_tree_ptr,
        &view_ptr->window_menu_wlr_scene_buffer_ptr->node);
    if (NULL == avlnode_ptr) return;
    wlmaker_interactive_node_destroy(avlnode_ptr);

    wlr_scene_node_destroy(
        &view_ptr->window_menu_wlr_scene_buffer_ptr->node);
    view_ptr->window_menu_wlr_scene_buffer_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_cursor_leave(wlmaker_view_t *view_ptr)
{
    // leaves the window. currently active view needs to be updated.
    update_pointer_focus(view_ptr, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_shade(__UNUSED__ wlmaker_view_t *view_ptr)
{
    bs_log(BS_INFO, "Shade only available when server-side-decorated.");
    return;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_get_size(wlmaker_view_t *view_ptr,
                           uint32_t *width_ptr,
                           uint32_t *height_ptr)
{
    view_ptr->impl_ptr->get_size(view_ptr, width_ptr, height_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_size(wlmaker_view_t *view_ptr, int width, int height)
{
    width = BS_MAX(1, width);
    height = BS_MAX(1, height);

    view_ptr->impl_ptr->set_size(view_ptr, width, height);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_get_position(wlmaker_view_t *view_ptr,
                               int *x_ptr, int *y_ptr)
{
    *x_ptr = view_ptr->elements_wlr_scene_tree_ptr->node.x;
    *y_ptr = view_ptr->elements_wlr_scene_tree_ptr->node.y;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_position(wlmaker_view_t *view_ptr,
                               int x, int y)
{
    if (x != view_ptr->elements_wlr_scene_tree_ptr->node.x ||
        y != view_ptr->elements_wlr_scene_tree_ptr->node.y) {
        wlr_scene_node_set_position(
            &view_ptr->elements_wlr_scene_tree_ptr->node, x, y);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_maximized(wlmaker_view_t *view_ptr, bool maximize)
{
    if (view_ptr->maximized == maximize) return;

    struct wlr_box new_box;
    if (!view_ptr->maximized) {
        // Not maximized yet. Store the organic position...
        wlmaker_view_get_position(view_ptr,
                                  &view_ptr->organic_box.x,
                                  &view_ptr->organic_box.y);
        uint32_t width, height;
        wlmaker_view_get_size(view_ptr, &width, &height);
        view_ptr->organic_box.width = width;
        view_ptr->organic_box.height = height;

        // And determine the size of the output, for setting pos + size.
        wlmaker_workspace_get_maximize_area(
            view_ptr->workspace_ptr,
            wlmaker_view_get_wlr_output(view_ptr),
            &new_box);
    } else {
        // It was maximized. Restore to previous (organic) position and size.
        new_box = view_ptr->organic_box;
    }

    wlmaker_view_set_position(view_ptr, new_box.x, new_box.y);
    wlmaker_view_set_size(view_ptr, new_box.width, new_box.height);

    if (NULL != view_ptr->impl_ptr->set_maximized) {
        view_ptr->impl_ptr->set_maximized(view_ptr, maximize);
    }
    view_ptr->maximized = maximize;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_fullscreen(wlmaker_view_t *view_ptr, bool fullscreen)
{
    if (fullscreen == view_ptr->fullscreen) return;  // Nothing to do.

    struct wlr_box new_box;
    if (!view_ptr->fullscreen) {
        // Not maximized yet. Store the organic position...
        wlmaker_view_get_position(view_ptr,
                                  &view_ptr->organic_box.x,
                                  &view_ptr->organic_box.y);
        uint32_t width, height;
        wlmaker_view_get_size(view_ptr, &width, &height);
        view_ptr->organic_box.width = width;
        view_ptr->organic_box.height = height;

        wlmaker_workspace_get_fullscreen_area(
            view_ptr->workspace_ptr,
            wlmaker_view_get_wlr_output(view_ptr),
            &new_box);

    } else {
        // It had been in fullscreen mode. Restore to organic dimensions.
        new_box = view_ptr->organic_box;
    }
    view_ptr->fullscreen = fullscreen;

    if (fullscreen) {
        wlmaker_workspace_promote_view_to_fullscreen(
            view_ptr->workspace_ptr, view_ptr);
    } else {
        wlmaker_workspace_demote_view_from_fullscreen(
            view_ptr->workspace_ptr, view_ptr);
    }

    wlmaker_view_set_position(view_ptr, new_box.x, new_box.y);
    wlmaker_view_set_size(view_ptr, new_box.width, new_box.height);

    if (NULL != view_ptr->impl_ptr->set_fullscreen) {
        view_ptr->impl_ptr->set_fullscreen(view_ptr, fullscreen);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_iconified(wlmaker_view_t *view_ptr, bool iconified)
{
    // state: fullscreen, iconified, organic, (some form of maximized)
    if (iconified) {
        wlmaker_workspace_view_set_as_iconified(
            view_ptr->workspace_ptr, view_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_title(wlmaker_view_t *view_ptr, const char *title_ptr)
{
    if (NULL != view_ptr->title_ptr) {
        if (NULL != title_ptr && 0 == strcmp(view_ptr->title_ptr, title_ptr)) {
            // Title didn't change. Nothing to do.
            return;
        }
        free(view_ptr->title_ptr);
        view_ptr->title_ptr = NULL;
    }
    if (NULL != title_ptr) {
        view_ptr->title_ptr = logged_strdup(title_ptr);
    }
}

/* ------------------------------------------------------------------------- */
const char *wlmaker_view_get_title(wlmaker_view_t *view_ptr)
{
    return view_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_set_app_id(wlmaker_view_t *view_ptr, const char *app_id_ptr)
{
    if (NULL != view_ptr->app_id_ptr) {
        free(view_ptr->app_id_ptr);
        view_ptr->app_id_ptr = NULL;
    }

    if (NULL != app_id_ptr) {
        view_ptr->app_id_ptr = logged_strdup(app_id_ptr);
    }
}

/* ------------------------------------------------------------------------- */
const char *wlmaker_view_get_app_id(wlmaker_view_t *view_ptr)
{
    return view_ptr->app_id_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_map(wlmaker_view_t *view_ptr,
                      wlmaker_workspace_t *workspace_ptr,
                      wlmaker_workspace_layer_t layer)
{
    BS_ASSERT(NULL == view_ptr->workspace_ptr);  // Shouldn't be mapped yet.
    view_ptr->workspace_ptr = workspace_ptr;
    BS_ASSERT(NULL != view_ptr->workspace_ptr);
    view_ptr->default_layer = layer;

    wlmaker_workspace_add_view(
        view_ptr->workspace_ptr,
        view_ptr,
        layer);

    wl_signal_emit(&view_ptr->server_ptr->view_mapped_event, view_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_view_unmap(wlmaker_view_t *view_ptr)
{
    BS_ASSERT(NULL != view_ptr->workspace_ptr);  // Should be mapped.
    wlmaker_workspace_remove_view(view_ptr->workspace_ptr, view_ptr);
    view_ptr->workspace_ptr = NULL;

    wl_signal_emit(&view_ptr->server_ptr->view_unmapped_event, view_ptr);
}

/* ------------------------------------------------------------------------- */
uint32_t wlmaker_view_get_anchor(wlmaker_view_t *view_ptr)
{
    return view_ptr->anchor;
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmaker_view_get_wlr_output(wlmaker_view_t *view_ptr)
{
    int pos_x, pos_y;
    uint32_t width, height;
    wlmaker_view_get_position(view_ptr, &pos_x, &pos_y);
    wlmaker_view_get_size(view_ptr, &width, &height);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_at(
        view_ptr->server_ptr->wlr_output_layout_ptr,
        pos_x + width / 2,
        pos_y + height / 2);
    return wlr_output_ptr;
}

/* ------------------------------------------------------------------------- */
const wlmaker_client_t *wlmaker_view_get_client(wlmaker_view_t *view_ptr)
{
    return &view_ptr->client;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `button_release` signal.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_button_release(struct wl_listener *listener_ptr,
                           void *data_ptr)
{
    wlmaker_view_t *view_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_view_t, button_release_listener);
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr = data_ptr;

    // Note: |wlmaker_view_handle_button| already handled button events and
    // passed them on to any interactive below the cursor. We still want to
    // forward 'release button' events to all other interactives, for proper
    // closure of state.
    bs_avltree_node_t *avl_node_ptr = bs_avltree_min(
        view_ptr->interactive_tree_ptr);
    while (NULL != avl_node_ptr) {
        bs_avltree_node_t *next_avl_node_ptr = bs_avltree_node_next(
            view_ptr->interactive_tree_ptr, avl_node_ptr);

        // Cautious -> this might delete the node.
        wlmaker_interactive_t *interactive_ptr =
            wlmaker_interactive_from_avlnode(avl_node_ptr);
        if (view_ptr->pointer_focussed_wlr_scene_node_ptr !=
            &interactive_ptr->wlr_scene_buffer_ptr->node) {
            wlmaker_interactive_button(
                interactive_ptr, -1, -1, wlr_pointer_button_event_ptr);
        }

        avl_node_ptr = next_avl_node_ptr;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the node currently having "pointer focus".
 *
 * @param view_ptr
 * @param wlr_scene_node_ptr  Node that is now below the cursor, ie. is going
 *                            to have "pointer focus".
 */
void update_pointer_focus(wlmaker_view_t *view_ptr,
                          struct wlr_scene_node *wlr_scene_node_ptr)
{
    if (view_ptr->pointer_focussed_wlr_scene_node_ptr == wlr_scene_node_ptr) {
        // Nothing to update.
        return;
    }

    if (NULL != view_ptr->pointer_focussed_wlr_scene_node_ptr) {
        bs_avltree_node_t *avl_node_ptr = bs_avltree_lookup(
            view_ptr->interactive_tree_ptr,
            view_ptr->pointer_focussed_wlr_scene_node_ptr);
        if (NULL != avl_node_ptr) {
            wlmaker_interactive_leave(
                wlmaker_interactive_from_avlnode(avl_node_ptr));
        }
    }

    view_ptr->pointer_focussed_wlr_scene_node_ptr = wlr_scene_node_ptr;

    if (NULL != view_ptr->pointer_focussed_wlr_scene_node_ptr) {
        bs_avltree_node_t *avl_node_ptr = bs_avltree_lookup(
            view_ptr->interactive_tree_ptr,
            view_ptr->pointer_focussed_wlr_scene_node_ptr);
        if (NULL != avl_node_ptr) {
            wlmaker_interactive_enter(
                wlmaker_interactive_from_avlnode(avl_node_ptr));
        }
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Toggle maximized view.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_maximize(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    wlmaker_view_set_maximized(view_ptr, !view_ptr->maximized);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Toggle fullscreen mode.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_fullscreen(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    wlmaker_view_set_fullscreen(view_ptr, !view_ptr->fullscreen);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: .
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_minimize(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    wlmaker_view_set_iconified(view_ptr, true);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Shade (roll up) the window.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_shade(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    wlmaker_view_shade(view_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Move the view to workspace 1.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_move_to_workspace1(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    bs_log(BS_WARNING, "Unimplemented: Move view %p to workspace 1.", view_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Move the view to workspace 2.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_move_to_workspace2(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    bs_log(BS_WARNING, "Unimplemented: Move view %p to workspace 1.", view_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Window menu callback: Close the view.
 *
 * @param ud_ptr              Non-typed pointer to the @ref wlmaker_view_t.
 */
void window_menu_callback_close(void *ud_ptr)
{
    wlmaker_view_t *view_ptr = (wlmaker_view_t*)ud_ptr;
    view_ptr->send_close_callback(view_ptr);
}

/* == End of view.c ======================================================== */
