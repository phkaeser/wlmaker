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

#include "root.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the root element. */
struct _wlmtk_root_t {
    /** The root's container: Holds workspaces and the curtain. */
    wlmtk_container_t         container;
    /** Overwritten virtual method table before extending ig. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Back-link to the output layer provided to the ctor. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Whether the root is currently locked. */
    bool                      locked;
    /** Reference to the lock, see @ref wlmtk_root_lock. */
    wlmtk_lock_t               *lock_ptr;

    /** Curtain element: Permit dimming or hiding everything. */
    wlmtk_rectangle_t         *curtain_rectangle_ptr;

    /** Triggers whenever @ref wlmtk_root_unlock succeeds. */
    struct wl_signal          unlock_event;

    /** List of workspaces attached to root. @see wlmtk_workspace_t::dlnode. */
    bs_dllist_t               workspaces;
};

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

    struct wlr_box extents;
    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &extents);
    root_ptr->curtain_rectangle_ptr = wlmtk_rectangle_create(
        env_ptr, extents.width, extents.height, 0xff000020);
    if (NULL == root_ptr->curtain_rectangle_ptr) {
        wlmtk_root_destroy(root_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &root_ptr->container,
        wlmtk_rectangle_element(root_ptr->curtain_rectangle_ptr));

    wl_signal_init(&root_ptr->unlock_event);
    return root_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_destroy(wlmtk_root_t *root_ptr)
{
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

    // Guard clause: nothing to pass on if no element has the focus.
    event.button = event_ptr->button;
    event.time_msec = event_ptr->time_msec;
    if (WLR_BUTTON_PRESSED == event_ptr->state) {
        event.type = WLMTK_BUTTON_DOWN;
        return wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);

    } else if (WLR_BUTTON_RELEASED == event_ptr->state) {
        event.type = WLMTK_BUTTON_UP;
        wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);
        event.type = WLMTK_BUTTON_CLICK;
        return wlmtk_element_pointer_button(
            &root_ptr->container.super_element, &event);

    }

    bs_log(BS_WARNING,
           "Root %p: Unhandled state 0x%x for button 0x%x",
           root_ptr, event_ptr->state, event_ptr->button);
    return false;
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
    bs_dllist_push_back(
        &root_ptr->workspaces,
        wlmtk_dlnode_from_workspace(workspace_ptr));
    wlmtk_workspace_set_root(workspace_ptr, root_ptr);
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

    struct wlr_box extents;
    wlr_output_layout_get_box(root_ptr->wlr_output_layout_ptr, NULL, &extents);
    wlmtk_rectangle_set_size(
        root_ptr->curtain_rectangle_ptr,
        extents.width, extents.height);
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
    wl_signal_emit(&root_ptr->unlock_event, NULL);
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
void wlmtk_root_connect_unlock_signal(
    wlmtk_root_t *root_ptr,
    struct wl_listener *listener_ptr,
    wl_notify_func_t handler)
{
    wlmtk_util_connect_listener_signal(
        &root_ptr->unlock_event, listener_ptr, handler);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr)
{
    return &root_ptr->container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Destroys the workspace. */
void _wlmtk_root_destroy_workspace(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);
    wlmtk_root_remove_workspace(ud_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
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

/* == Unit tests =========================================================== */

static void test_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_root_test_cases[] = {
    { 0, "button", test_button },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests wlmtk_root_pointer_button. */
void test_button(bs_test_t *test_ptr)
{

    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&fake_element_ptr->element, true);
    BS_ASSERT(NULL != fake_element_ptr);

    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    struct wlr_output_layout wlr_output_layout = {};
    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlr_scene_ptr, &wlr_output_layout, NULL);

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
        .state = WLR_BUTTON_PRESSED,
        .time_msec = 4321,
    };
    // FIXME: return value?
    wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event);
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
    wlr_pointer_button_event.state = WLR_BUTTON_RELEASED;
    wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event);
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
}



/* == End of root.c ======================================================== */
