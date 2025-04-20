/* ========================================================================= */
/**
 * @file root.c
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

#include <toolkit/root.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the root element. */
struct _wlmtk_root_t {
    /** The root's container: Holds workspaces and the curtain. */
    wlmtk_container_t         container;
    /** Overwritten virtual method table before extending ig. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** Extents to be used by root. */
    struct wlr_box            extents;

    /** Events availabe of the root. */
    wlmtk_root_events_t       events;

    /** Whether the root is currently locked. */
    bool                      locked;
    /** Reference to the lock, see @ref wlmtk_root_lock. */
    wlmtk_lock_t               *lock_ptr;

    /** Curtain element: Permit dimming or hiding everything. */
    wlmtk_rectangle_t         *curtain_rectangle_ptr;

    /** List of workspaces attached to root. @see wlmtk_workspace_t::dlnode. */
    bs_dllist_t               workspaces;
    /** Currently-active workspace. */
    wlmtk_workspace_t         *current_workspace_ptr;

    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;

    // Elements below not owned by wlmtk_root_t.
    /** wlroots output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
};

static void _wlmtk_root_switch_to_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr);
static void _wlmtk_root_workspace_update_output_layout(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_root_enumerate_workspaces(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_root_destroy_workspace(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static bool _wlmtk_root_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool _wlmtk_root_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_root_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static bool _wlmtk_root_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    const xkb_keysym_t *key_syms,
    size_t key_syms_count,
    uint32_t modifiers);

static void _wlmtk_root_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_vmt_t _wlmtk_root_element_vmt = {
    .pointer_motion = _wlmtk_root_element_pointer_motion,
    .pointer_button = _wlmtk_root_element_pointer_button,
    .pointer_axis = _wlmtk_root_element_pointer_axis,
    .keyboard_event = _wlmtk_root_element_keyboard_event,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_root_t *wlmtk_root_create(
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_root_t *root_ptr = logged_calloc(1, sizeof(wlmtk_root_t));
    if (NULL == root_ptr) return NULL;
    root_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    if (!wlmtk_container_init_attached(
            &root_ptr->container,
            env_ptr,
            &wlr_scene_ptr->tree)) {
        wlmtk_root_destroy(root_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(&root_ptr->container.super_element, true);
    root_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &root_ptr->container.super_element,
        &_wlmtk_root_element_vmt);

    root_ptr->curtain_rectangle_ptr = wlmtk_rectangle_create(
        env_ptr, root_ptr->extents.width, root_ptr->extents.height, 0xff000020);
    if (NULL == root_ptr->curtain_rectangle_ptr) {
        wlmtk_root_destroy(root_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &root_ptr->container,
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr));

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &root_ptr->output_layout_change_listener,
        _wlmtk_root_handle_output_layout_change);
    _wlmtk_root_handle_output_layout_change(
        &root_ptr->output_layout_change_listener,
        wlr_output_layout_ptr);

    wl_signal_init(&root_ptr->events.workspace_changed);
    wl_signal_init(&root_ptr->events.unlock_event);
    wl_signal_init(&root_ptr->events.window_mapped);
    wl_signal_init(&root_ptr->events.window_unmapped);
    wl_signal_init(&root_ptr->events.unclaimed_button_event);
    return root_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_destroy(wlmtk_root_t *root_ptr)
{
    wlmtk_util_disconnect_listener(
        &root_ptr->output_layout_change_listener);

    bs_dllist_for_each(
        &root_ptr->workspaces,
        _wlmtk_root_destroy_workspace,
        root_ptr);

    if (NULL != root_ptr->curtain_rectangle_ptr) {
        wlmtk_container_remove_element(
            &root_ptr->container,
            wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr));

        wlmtk_rectangle_destroy(root_ptr->curtain_rectangle_ptr);
        root_ptr->curtain_rectangle_ptr = NULL;
    }

    wlmtk_container_fini(&root_ptr->container);

    free(root_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_root_events_t *wlmtk_root_events(wlmtk_root_t *root_ptr)
{
    return &root_ptr->events;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_pointer_motion(
    wlmtk_root_t *root_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    return wlmtk_element_pointer_motion(
        &root_ptr->container.super_element, x, y, time_msec);

}

/* ------------------------------------------------------------------------- */
// TODO(kaeser@gubbe.ch): Improve this, has multiple bugs: It won't keep
// different buttons apart, and there's currently no test associated.
bool wlmtk_root_pointer_button(
    wlmtk_root_t *root_ptr,
    const struct wlr_pointer_button_event *event_ptr)
{
    wlmtk_button_event_t event;
    bool rv;

    // Guard clause: nothing to pass on if no element has the focus.
    event.button = event_ptr->button;
    event.time_msec = event_ptr->time_msec;
    switch (event_ptr->state) {
#if WLR_VERSION_NUM >= (18 << 8)
    case WL_POINTER_BUTTON_STATE_PRESSED:
#else // WLR_VERSION_NUM >= (18 << 8)
    case WLR_BUTTON_PRESSED:
#endif // WLR_VERSION_NUM >= (18 << 8)
        event.type = WLMTK_BUTTON_DOWN;
        rv = wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);
        break;

#if WLR_VERSION_NUM >= (18 << 8)
    case WL_POINTER_BUTTON_STATE_RELEASED:
#else // WLR_VERSION_NUM >= (18 << 8)
    case WLR_BUTTON_RELEASED:
#endif // WLR_VERSION_NUM >= (18 << 8)
        event.type = WLMTK_BUTTON_UP;
        wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);
        event.type = WLMTK_BUTTON_CLICK;
        rv = wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);
        break;

    default:
        bs_log(BS_WARNING,
               "Root %p: Unhandled state 0x%x for button 0x%x",
               root_ptr, event_ptr->state, event_ptr->button);
        return false;
    }

    if (!rv) {
        wl_signal_emit(&root_ptr->events.unclaimed_button_event, &event);
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_pointer_axis(
    wlmtk_root_t *root_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    return wlmtk_element_pointer_axis(
        &root_ptr->container.super_element,
        wlr_pointer_axis_event_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_add_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    BS_ASSERT(NULL == wlmtk_workspace_get_root(workspace_ptr));

    wlmtk_container_add_element(
        &root_ptr->container,
        wlmtk_workspace_element(workspace_ptr));

    // Keep the curtain on top.
    wlmtk_container_remove_element(
        &root_ptr->container,
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr));
    wlmtk_container_add_element(
        &root_ptr->container,
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr));

    bs_dllist_push_back(
        &root_ptr->workspaces,
        wlmtk_dlnode_from_workspace(workspace_ptr));
    wlmtk_workspace_set_details(
        workspace_ptr, bs_dllist_size(&root_ptr->workspaces));
    wlmtk_workspace_set_root(workspace_ptr, root_ptr);

    if (NULL == root_ptr->current_workspace_ptr) {
        _wlmtk_root_switch_to_workspace(root_ptr, workspace_ptr);
    }

    wlmtk_workspace_update_output_layout(
        workspace_ptr, root_ptr->wlr_output_layout_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_remove_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    BS_ASSERT(root_ptr == wlmtk_workspace_get_root(workspace_ptr));
    wlmtk_workspace_set_root(workspace_ptr, NULL);
    bs_dllist_remove(
        &root_ptr->workspaces,
        wlmtk_dlnode_from_workspace(workspace_ptr));
    wlmtk_container_remove_element(
        &root_ptr->container,
        wlmtk_workspace_element(workspace_ptr));
    wlmtk_element_set_visible(
        wlmtk_workspace_element(workspace_ptr), false);

    if (root_ptr->current_workspace_ptr == workspace_ptr) {
        _wlmtk_root_switch_to_workspace(
            root_ptr,
            wlmtk_workspace_from_dlnode(root_ptr->workspaces.head_ptr));
    }

    int index = 0;
    bs_dllist_for_each(
        &root_ptr->workspaces,
        _wlmtk_root_enumerate_workspaces,
        &index);
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_root_get_current_workspace(wlmtk_root_t *root_ptr)
{
    return root_ptr->current_workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_switch_to_next_workspace(wlmtk_root_t *root_ptr)
{
    if (NULL == root_ptr->current_workspace_ptr) return;

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_workspace(
        root_ptr->current_workspace_ptr);
    if (NULL == dlnode_ptr->next_ptr) {
        dlnode_ptr = root_ptr->workspaces.head_ptr;
    } else {
        dlnode_ptr = dlnode_ptr->next_ptr;
    }
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);

    _wlmtk_root_switch_to_workspace(root_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_switch_to_previous_workspace(wlmtk_root_t *root_ptr)
{
    if (NULL == root_ptr->current_workspace_ptr) return;
    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_workspace(
        root_ptr->current_workspace_ptr);
    if (NULL == dlnode_ptr->prev_ptr) {
        dlnode_ptr = root_ptr->workspaces.tail_ptr;
    } else {
        dlnode_ptr = dlnode_ptr->prev_ptr;
    }
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);

    _wlmtk_root_switch_to_workspace(root_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_for_each_workspace(
    wlmtk_root_t *root_ptr,
    void (*func)(bs_dllist_node_t *dlnode_ptr, void *ud_ptr),
    void *ud_ptr)
{
    bs_dllist_for_each(&root_ptr->workspaces, func, ud_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_lock(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr)
{
    if (root_ptr->locked) {
        bs_log(BS_WARNING, "Root already locked by %p", root_ptr->lock_ptr);
        return false;
    }

    wlmtk_workspace_enable(root_ptr->current_workspace_ptr, false);

    wlmtk_rectangle_set_size(
        root_ptr->curtain_rectangle_ptr,
        root_ptr->extents.width, root_ptr->extents.height);
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr),
        true);

    wlmtk_container_add_element(
        &root_ptr->container,
        wlmtk_lock_element(lock_ptr));
    root_ptr->lock_ptr = lock_ptr;

    root_ptr->locked = true;
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_unlock(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr)
{
    // Guard clause: Not locked => nothing to do.
    if (!root_ptr->locked) return false;
    if (lock_ptr != root_ptr->lock_ptr) {
        bs_log(BS_ERROR, "Lock held by %p, but attempted to unlock by %p",
               root_ptr->lock_ptr, lock_ptr);
        return false;
    }

    wlmtk_root_lock_unreference(root_ptr, lock_ptr);
    root_ptr->locked = false;

    wlmtk_element_set_visible(
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr),
        false);

    wl_signal_emit(&root_ptr->events.unlock_event, NULL);

    wlmtk_workspace_enable(root_ptr->current_workspace_ptr, true);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_lock_unreference(
    wlmtk_root_t *root_ptr,
    wlmtk_lock_t *lock_ptr)
{
    if (lock_ptr != root_ptr->lock_ptr) return;

    wlmtk_container_remove_element(
        &root_ptr->container,
        wlmtk_lock_element(root_ptr->lock_ptr));
    root_ptr->lock_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_set_lock_surface(
    __UNUSED__ wlmtk_root_t *root_ptr,
    wlmtk_surface_t *surface_ptr)
{
    wlmtk_surface_set_activated(surface_ptr, true);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr)
{
    return &root_ptr->container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Switches to `workspace_ptr` as the current workspace.
 *
 * @param root_ptr
 * @param workspace_ptr
 */
void _wlmtk_root_switch_to_workspace(
    wlmtk_root_t *root_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    if (root_ptr->current_workspace_ptr == workspace_ptr) return;

    if (NULL == workspace_ptr) {
        root_ptr->current_workspace_ptr = NULL;
    } else {
        BS_ASSERT(root_ptr = wlmtk_workspace_get_root(workspace_ptr));

        if (NULL != root_ptr->current_workspace_ptr) {
            wlmtk_element_set_visible(
                wlmtk_workspace_element(root_ptr->current_workspace_ptr),
                false);
            wlmtk_workspace_enable(root_ptr->current_workspace_ptr, false);
        }
        root_ptr->current_workspace_ptr = workspace_ptr;
        wlmtk_element_set_visible(
            wlmtk_workspace_element(root_ptr->current_workspace_ptr), true);
        wlmtk_workspace_enable(root_ptr->current_workspace_ptr, true);
    }

    wl_signal_emit(
        &root_ptr->events.workspace_changed,
        root_ptr->current_workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for `bs_dllist_for_each` to update the output layout of the
 * workspace.
 *
 * @param dlnode_ptr
 * @param ud_ptr              A pointer to struct wlr_output_layout.
 */
void _wlmtk_root_workspace_update_output_layout(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout *wlr_output_layout_ptr = ud_ptr;
    wlmtk_workspace_update_output_layout(
        wlmtk_workspace_from_dlnode(dlnode_ptr),
        wlr_output_layout_ptr);
}

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Destroys the workspace. */
void _wlmtk_root_destroy_workspace(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);
    wlmtk_root_remove_workspace(ud_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Enumerates the workspace. */
void _wlmtk_root_enumerate_workspaces(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    int *index_ptr = ud_ptr;
    wlmtk_workspace_set_details(
        wlmtk_workspace_from_dlnode(dlnode_ptr),
        *index_ptr);
    *index_ptr += 1;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_motion. Handle pointer moves.
 *
 * When locked, the root container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether the move was accepted.
 */
bool _wlmtk_root_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec)
{
    wlmtk_root_t *root_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_root_t, container.super_element);

    if (!root_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return root_ptr->orig_super_element_vmt.pointer_motion(
            element_ptr, x, y, time_msec);
    } else if (NULL != root_ptr->lock_ptr) {
        return wlmtk_element_pointer_motion(
            wlmtk_lock_element(root_ptr->lock_ptr),
            x, y, time_msec);
    }

    // Fall-through.
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_button. Handle button events.
 *
 * When locked, the root container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true if the button was handled.
 */
bool _wlmtk_root_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_root_t *root_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_root_t, container.super_element);

    if (!root_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return root_ptr->orig_super_element_vmt.pointer_button(
            element_ptr, button_event_ptr);
    } else if (NULL != root_ptr->lock_ptr) {
        return wlmtk_element_pointer_button(
            wlmtk_lock_element(root_ptr->lock_ptr),
            button_event_ptr);
    }

    // Fall-through.
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_axis. Handle axis events.
 *
 * When locked, the root container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return true if the axis event was handled.
 */
bool _wlmtk_root_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmtk_root_t *root_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_root_t, container.super_element);

    if (!root_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return root_ptr->orig_super_element_vmt.pointer_axis(
            element_ptr, wlr_pointer_axis_event_ptr);
    } else if (NULL != root_ptr->lock_ptr) {
        return wlmtk_element_pointer_axis(
            wlmtk_lock_element(root_ptr->lock_ptr),
            wlr_pointer_axis_event_ptr);
    }

    // Fall-through.
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::keyboard_event. Handle keyboard events.
 *
 * When locked, the root container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param wlr_keyboard_key_event_ptr
 * @param key_syms
 * @param key_syms_count
 * @param modifiers
 *
 * @return true if the axis event was handled.
 */
bool _wlmtk_root_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr,
    const xkb_keysym_t *key_syms,
    size_t key_syms_count,
    uint32_t modifiers)
{
    wlmtk_root_t *root_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_root_t, container.super_element);

    if (!root_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return root_ptr->orig_super_element_vmt.keyboard_event(
            element_ptr,
            wlr_keyboard_key_event_ptr,
            key_syms, key_syms_count, modifiers);
    } else if (NULL != root_ptr->lock_ptr) {
        return wlmtk_element_keyboard_event(
            wlmtk_lock_element(root_ptr->lock_ptr),
            wlr_keyboard_key_event_ptr,
            key_syms, key_syms_count, modifiers);
    }

    // Fall-through: Too bad -- the screen is locked, but the lock element
    // disappeared (crashed?). No more handling of keys here...
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles wlr_output_layout::events::change. Triggers when the output layout
 * changes, and we use this for updating the curtain and the layout in each
 * workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_root_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_root_t *root_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_root_t, output_layout_change_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &root_ptr->extents);
    wlmtk_rectangle_set_size(
        root_ptr->curtain_rectangle_ptr,
        root_ptr->extents.width,
        root_ptr->extents.height);

    bs_dllist_for_each(
        &root_ptr->workspaces,
        _wlmtk_root_workspace_update_output_layout,
        wlr_output_layout_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_workspaces(bs_test_t *test_ptr);
static void test_pointer_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_root_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "workspaces", test_workspaces },
    { 1, "pointer_button", test_pointer_button },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
void test_create_destroy(bs_test_t *test_ptr)
{
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);

    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlr_scene_ptr, wlr_output_layout_ptr, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr, &root_ptr->events, wlmtk_root_events(root_ptr));

    wlmtk_root_destroy(root_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* ------------------------------------------------------------------------- */
/** Exercises workspace adding and removal. */
void test_workspaces(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t l;
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(wl_display_ptr);
    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlr_scene_ptr, wlr_output_layout_ptr, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, wlmtk_root_get_current_workspace(root_ptr));

    wlmtk_util_connect_test_listener(
        &wlmtk_root_events(root_ptr)->workspace_changed, &l);

    static const wlmtk_tile_style_t tstyle = {};
    wlmtk_workspace_t *ws1_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "1", &tstyle, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws1_ptr);
    wlmtk_root_add_workspace(root_ptr, ws1_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws1_ptr, wlmtk_root_get_current_workspace(root_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_element(ws1_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, ws1_ptr, l.last_data_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    wlmtk_workspace_t *ws2_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "2", &tstyle, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws2_ptr);
    wlmtk_root_add_workspace(root_ptr, ws2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws1_ptr, wlmtk_root_get_current_workspace(root_ptr));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_element(ws2_ptr)->visible);

    wlmtk_root_remove_workspace(root_ptr, ws1_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_element(ws1_ptr)->visible);
    wlmtk_workspace_destroy(ws1_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws2_ptr, wlmtk_root_get_current_workspace(root_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_element(ws2_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, ws2_ptr, l.last_data_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 2, l.calls);

    wlmtk_root_remove_workspace(root_ptr, ws2_ptr);
    wlmtk_workspace_destroy(ws2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, wlmtk_root_get_current_workspace(root_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, l.last_data_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 3, l.calls);

    wlmtk_util_disconnect_test_listener(&l);
    wlmtk_root_destroy(root_ptr);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* ------------------------------------------------------------------------- */
/** Tests wlmtk_root_pointer_button. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);
    wlmtk_element_set_visible(&fake_element_ptr->element, true);

    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);
    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlr_scene_ptr, wlr_output_layout_ptr, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);
    wlmtk_container_add_element(
        &root_ptr->container, &fake_element_ptr->element);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_motion(root_ptr, 0, 0, 1234));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        fake_element_ptr->pointer_motion_called);

    // Verify that a button down event is passed.
    struct wlr_pointer_button_event wlr_pointer_button_event = {
        .button = 42,
#if WLR_VERSION_NUM >= (18 << 8)
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
#else // WLR_VERSION_NUM >= (18 << 8)
        .state = WLR_BUTTON_PRESSED,
#endif // WLR_VERSION_NUM >= (18 << 8)
        .time_msec = 4321,
    };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event));
    wlmtk_button_event_t expected_event = {
        .button = 42,
        .type = WLMTK_BUTTON_DOWN,
        .time_msec = 4321,
    };
    BS_TEST_VERIFY_MEMEQ(
        test_ptr,
        &expected_event,
        &fake_element_ptr->pointer_button_event,
        sizeof(wlmtk_button_event_t));

    // The button up event should trigger a click.
#if WLR_VERSION_NUM >= (18 << 8)
    wlr_pointer_button_event.state = WL_POINTER_BUTTON_STATE_RELEASED;
#else  // WLR_VERSION_NUM >= (18 << 8)
    wlr_pointer_button_event.state = WLR_BUTTON_RELEASED;
#endif // WLR_VERSION_NUM >= (18 << 8)
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event));
    expected_event.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_MEMEQ(
        test_ptr,
        &expected_event,
        &fake_element_ptr->pointer_button_event,
        sizeof(wlmtk_button_event_t));

    wlmtk_container_remove_element(
        &root_ptr->container, &fake_element_ptr->element);
    wlmtk_element_destroy(&fake_element_ptr->element);

    wlmtk_root_destroy(root_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* == End of root.c ======================================================== */
