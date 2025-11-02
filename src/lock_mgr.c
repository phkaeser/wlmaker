/* ========================================================================= */
/**
 * @file lock_mgr.c
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

#include "lock_mgr.h"

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/util/box.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

struct _wlmaker_lock_surface_t;
struct _wlmaker_lock_t;

/* == Declarations ========================================================= */

/** Type of wlr_session_lock_surface_v1_configure(). */
typedef uint32_t (*_wlmaker_lock_surface_configure_t)(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height);
/** Type of wlr_session_lock_v1_send_locked(). */
typedef void (*_wlmaker_lock_send_locked_t)(
    struct wlr_session_lock_v1 *lock);

/** State of the session lock manager. */
struct _wlmaker_lock_mgr_t {
    /** The wlroots session lock manager. */
    struct wlr_session_lock_manager_v1 *wlr_session_lock_manager_v1_ptr;

    /** Reference to the wlmaker server. */
    wlmaker_server_t          *server_ptr;

    /** Listener for the `new_lock` signal of `wlr_session_lock_manager_v1`. */
    struct wl_listener        new_lock_listener;
    /** Listener for the `destroy` signal of `wlr_session_lock_manager_v1`. */
    struct wl_listener        destroy_listener;
};

/** Forward declaration: Lock. */
typedef struct _wlmaker_lock_t wlmaker_lock_t;
/** Forward declaration: Lock surface. */
typedef struct _wlmaker_lock_surface_t wlmaker_lock_surface_t;

/** State of the session lock. */
struct _wlmaker_lock_t {
    /** The wlroots session lock. */
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr;
    /** Seat for the session. */
    struct wlr_seat           *wlr_seat_ptr;
    /** The root this lock is applied for. */
    wlmtk_root_t              *root_ptr;

    /** The output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Injected method: Configure the lock surface. */
    _wlmaker_lock_surface_configure_t injected_surface_configure;
    /** Injected method: Confirm session lock. */
    _wlmaker_lock_send_locked_t injected_send_locked;

    /** Holds all @ref wlmaker_lock_surface_t, keyed by output. */
    bs_avltree_t              *lock_surface_tree_ptr;
    /** Container holding the lock surfaces. */
    wlmtk_container_t         container;

    /** Listener for the `new_surface` signal of `wlr_session_lock_v1`. */
    struct wl_listener        new_surface_listener;
    /** Listener for the `unlock` signal of `wlr_session_lock_v1`. */
    struct wl_listener        unlock_listener;
    /** Listener for the `destroy` signal of `wlr_session_lock_v1`. */
    struct wl_listener        destroy_listener;

    /** Event: Output layout changed. Parameter: struct wlr_box*. */
    struct wl_listener        output_layout_change_listener;
};

/** State of a lock surface. */
struct _wlmaker_lock_surface_t {
    /** The wlroots session lock surface. */
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr;
    /** Toolkit surface for the associated wl_surface. */
    wlmtk_surface_t           *wlmtk_surface_ptr;
    /** Back-link to the lock. */
    wlmaker_lock_t              *lock_ptr;

    /** Tree node, element of @ref wlmaker_lock_t::lock_surface_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** Serial returned by `wlr_session_lock_surface_v1_configure`. */
    uint32_t                  configure_serial;

    /** Listener for the `destroy` signal of `wlr_session_lock_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for `commit` signal of `wlr_session_lock_surface_v1::surface`. */
    struct wl_listener        surface_commit_listener;
};

/** Arguent to @ref _wlmaker_lock_update_output. */
typedef struct {
    /** The lock we're working on. */
    wlmaker_lock_t              *lock_ptr;
    /** The former lock surface tree. */
    bs_avltree_t              *former_lock_surface_tree_ptr;
    /** Whether there was an activated (keyboard-focussed) surface. */
    bool                      has_activated_surface;
} wlmaker_lock_update_output_arg_t;

static void _wlmaker_lock_mgr_handle_new_lock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr,
    _wlmaker_lock_surface_configure_t injected_surface_configure,
    _wlmaker_lock_send_locked_t injected_send_locked);
static void _wlmaker_lock_destroy(wlmaker_lock_t *lock_ptr);
static wlmtk_element_t *_wlmaker_lock_element(wlmaker_lock_t *lock_ptr);

static void _wlmaker_lock_register_lock_surface(
    wlmaker_lock_t *lock_ptr,
    wlmaker_lock_surface_t *lock_surface_ptr);

static void _wlmaker_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static bool _wlmaker_lock_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr);

static wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr);
static void _wlmaker_lock_surface_destroy(
    wlmaker_lock_surface_t *lock_surface_ptr);

static void _wlmaker_lock_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_surface_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static int _wlmaker_lock_surface_avlnode_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);
static void _wlmaker_lock_surface_avlnode_destroy(
    bs_avltree_node_t *node_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_lock_mgr_t *wlmaker_lock_mgr_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = logged_calloc(
        1, sizeof(wlmaker_lock_mgr_t));
    if (NULL == lock_mgr_ptr) return NULL;
    lock_mgr_ptr->server_ptr = server_ptr;

    lock_mgr_ptr->wlr_session_lock_manager_v1_ptr =
        wlr_session_lock_manager_v1_create(server_ptr->wl_display_ptr);
    if (NULL == lock_mgr_ptr->wlr_session_lock_manager_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_session_lock_manager_v1_create(%p)",
               server_ptr->wl_display_ptr);
        wlmaker_lock_mgr_destroy(lock_mgr_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_mgr_ptr->wlr_session_lock_manager_v1_ptr->events.new_lock,
        &lock_mgr_ptr->new_lock_listener,
        _wlmaker_lock_mgr_handle_new_lock);
    wlmtk_util_connect_listener_signal(
        &lock_mgr_ptr->wlr_session_lock_manager_v1_ptr->events.destroy,
        &lock_mgr_ptr->destroy_listener,
        _wlmaker_lock_mgr_handle_destroy);

    return lock_mgr_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_lock_mgr_destroy(wlmaker_lock_mgr_t *lock_mgr_ptr)
{
    wlmtk_util_disconnect_listener(&lock_mgr_ptr->destroy_listener);
    wlmtk_util_disconnect_listener(&lock_mgr_ptr->new_lock_listener);

    // Note: No destroy method for wlr_session_lock_manager_v1_ptr.

    free(lock_mgr_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_lock` signal of `wlr_session_lock_manager_v1`: creates
 * the corresponding lock.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_mgr_handle_new_lock(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_mgr_t, new_lock_listener);
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr = data_ptr;

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        wlr_session_lock_v1_ptr,
        lock_mgr_ptr->server_ptr->wlr_output_layout_ptr,
        lock_mgr_ptr->server_ptr->wlr_seat_ptr,
        lock_mgr_ptr->server_ptr->root_ptr,
        wlr_session_lock_surface_v1_configure,
        wlr_session_lock_v1_send_locked);
    if (NULL == lock_ptr) {
        wl_resource_post_error(
            wlr_session_lock_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmt_lock_create(%p, %p)",
            wlr_session_lock_v1_ptr,
            lock_mgr_ptr->server_ptr->root_ptr);
        bs_log(BS_WARNING, "Failed wlmt_lock_create(%p, %p)",
               wlr_session_lock_v1_ptr,
               lock_mgr_ptr->server_ptr->root_ptr);
        return;
    }

    bs_log(BS_INFO, "Lock manager %p: New lock %p", lock_mgr_ptr, lock_ptr);
}
/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_manager_v1`: Cleans
 * up associated resources.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_mgr_t *lock_mgr_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_mgr_t, destroy_listener);

    wlmaker_lock_mgr_destroy(lock_mgr_ptr);
}

/* == lock methods ========================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Creates a session lock handle.
 *
 * @param wlr_session_lock_v1_ptr
 * @param wlr_output_layout_ptr
 * @param wlr_seat_ptr
 * @param root_ptr
 * @param injected_surface_configure
 * @param injected_send_locked
 *
 * @return The lock handle or NULL on error.
 */
wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    struct wlr_seat *wlr_seat_ptr,
    wlmtk_root_t *root_ptr,
    _wlmaker_lock_surface_configure_t injected_surface_configure,
    _wlmaker_lock_send_locked_t injected_send_locked)
{
    wlmaker_lock_t *lock_ptr = logged_calloc(1, sizeof(wlmaker_lock_t));
    if (NULL == lock_ptr) return NULL;
    lock_ptr->wlr_session_lock_v1_ptr = wlr_session_lock_v1_ptr;
    lock_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    lock_ptr->wlr_seat_ptr = wlr_seat_ptr;
    lock_ptr->root_ptr = root_ptr;
    lock_ptr->injected_surface_configure = injected_surface_configure;
    lock_ptr->injected_send_locked = injected_send_locked;

    lock_ptr->lock_surface_tree_ptr = bs_avltree_create(
        _wlmaker_lock_surface_avlnode_cmp,
        _wlmaker_lock_surface_avlnode_destroy);
    if (NULL == lock_ptr->lock_surface_tree_ptr) {
        bs_log(BS_ERROR, "Failed bs_avltree_create(%p, %p)",
               _wlmaker_lock_surface_avlnode_cmp,
               _wlmaker_lock_surface_avlnode_destroy);
        _wlmaker_lock_destroy(lock_ptr);
        return NULL;
    }

    if (!wlmtk_container_init(&lock_ptr->container)) {
        _wlmaker_lock_destroy(lock_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.new_surface,
        &lock_ptr->new_surface_listener,
        _wlmaker_lock_handle_new_surface);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.unlock,
        &lock_ptr->unlock_listener,
        _wlmaker_lock_handle_unlock);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.destroy,
        &lock_ptr->destroy_listener,
        _wlmaker_lock_handle_destroy);

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &lock_ptr->output_layout_change_listener,
        _wlmaker_lock_handle_output_layout_change);
    _wlmaker_lock_handle_output_layout_change(
        &lock_ptr->output_layout_change_listener,
        NULL);

    return lock_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the session lock handle.
 *
 * @param lock_ptr
 */
void _wlmaker_lock_destroy(wlmaker_lock_t *lock_ptr)
{
    wlmtk_util_disconnect_listener(
        &lock_ptr->output_layout_change_listener);

    if (NULL != lock_ptr->lock_surface_tree_ptr) {
        bs_avltree_destroy(lock_ptr->lock_surface_tree_ptr);
        lock_ptr->lock_surface_tree_ptr = NULL;
    }

    wl_list_remove(&lock_ptr->destroy_listener.link);
    wl_list_remove(&lock_ptr->unlock_listener.link);
    wl_list_remove(&lock_ptr->new_surface_listener.link);

    wlmtk_root_lock_unreference(lock_ptr->root_ptr,
                                _wlmaker_lock_element(lock_ptr));
    wlmtk_container_fini(&lock_ptr->container);

    free(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * @returns Pointer to @ref wlmtk_element_t of @ref wlmaker_lock_t::container.
 * */
wlmtk_element_t *_wlmaker_lock_element(wlmaker_lock_t *lock_ptr)
{
    return &lock_ptr->container.super_element;
}

/* ------------------------------------------------------------------------- */
/**
 * Registers the provided surface as a lock surface for it's output, unless
 * it had already been registered. Locks the session, once lock surfaces for
 * all surfaces have been registered.
 *
 * @param lock_ptr
 * @param lock_surface_ptr
 */
void _wlmaker_lock_register_lock_surface(
    wlmaker_lock_t *lock_ptr,
    wlmaker_lock_surface_t *lock_surface_ptr)
{
    // Guard clause: Refuse surfaces for outputs not in layout.
    if (NULL == wlr_output_layout_get(
            lock_ptr->wlr_output_layout_ptr,
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output)) {
        bs_log(BS_WARNING,
               "Invalid wlr_output %p for lock surface %p. Ignoring.",
               lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output,
               lock_surface_ptr->wlr_session_lock_surface_v1_ptr);
        return;
    }

    // Guard clause: Don't register more than once.
    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        lock_ptr->lock_surface_tree_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);
    if (NULL != avlnode_ptr) {
        wlmaker_lock_surface_t *existing_lock_surface_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmaker_lock_surface_t, avlnode);
        if (existing_lock_surface_ptr != lock_surface_ptr) {
            // Registering more than one surfaces per output is a protocol
            // violation. wlroots is supposed to catch these, so this is just
            // an extra safety check.
            wl_resource_post_error(
                lock_ptr->wlr_session_lock_v1_ptr->resource,
                WL_DISPLAY_ERROR_INVALID_METHOD,
                "Extra lock surface %p found for wlr_output %p (surface %p)",
                lock_surface_ptr->wlr_session_lock_surface_v1_ptr,
                lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output,
                existing_lock_surface_ptr->wlr_session_lock_surface_v1_ptr);
        }
        return;
    }

    // Inserting *must* succeed -- we verified above that there's no duplicate.
    BS_ASSERT(bs_avltree_insert(
                  lock_ptr->lock_surface_tree_ptr,
                  lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output,
                  &lock_surface_ptr->avlnode,
                  false));
    wlmtk_container_add_element(
        &lock_ptr->container,
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr));
    wlmtk_element_set_visible(
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr), true);

    // If not all outputs are covered: No lock yet.
    if (bs_avltree_size(lock_ptr->lock_surface_tree_ptr) <
        (size_t)wl_list_length(&lock_ptr->wlr_output_layout_ptr->outputs)) {
        return;
    }

    if (!wlmtk_root_lock(lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr))) {
        wl_resource_post_error(
            lock_ptr->wlr_session_lock_v1_ptr->resource,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Failed wlmtk_root_lock(%p, %p): Already locked?",
            lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr));
        return;
    }
    wlmtk_element_set_visible(&lock_ptr->container.super_element, true);
    // Grant keyboard focus to the lock surface we just added.
    wlmtk_surface_set_activated(lock_surface_ptr->wlmtk_surface_ptr, true);

    // Root is locked. Send confirmation to the client.
    lock_ptr->injected_send_locked(lock_ptr->wlr_session_lock_v1_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_surface` signal of `wlr_session_lock_v1`: Creates the
 * associated surface and enables it on the screenlock container.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, new_surface_listener);
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        data_ptr;

    wlmaker_lock_surface_t *lock_surface_ptr = _wlmaker_lock_surface_create(
        wlr_session_lock_surface_v1_ptr,
        lock_ptr);
    if (NULL == lock_surface_ptr) {
        wl_resource_post_error(
            wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmaker_lock_surface_create(%p, %p)",
            wlr_session_lock_surface_v1_ptr->surface,
            lock_ptr);
        return;
    }

    bs_log(BS_INFO, "Lock %p: New lock surface %p", lock_ptr, lock_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `unlock` signal of `wlr_session_lock_v1`: Marks the session
 * as unlocked.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    __UNUSED__ wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, unlock_listener);

    wlmtk_element_set_visible(&lock_ptr->container.super_element, false);
    wlmtk_root_unlock(lock_ptr->root_ptr, _wlmaker_lock_element(lock_ptr));
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_v1`: Destroy the lock.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, destroy_listener);
    _wlmaker_lock_destroy(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the `change` event of wlr_output_layout::events.
 *
 * Walks through outputs of @ref wlmaker_lock_t::wlr_output_layout_ptr, and
 * updates @ref wlmaker_lock_t::lock_surface_tree_ptr accordingly.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, output_layout_change_listener);

    wlmaker_lock_update_output_arg_t arg = {
        .lock_ptr = lock_ptr,
        .former_lock_surface_tree_ptr = lock_ptr->lock_surface_tree_ptr,
        .has_activated_surface = false
    };

    lock_ptr->lock_surface_tree_ptr = bs_avltree_create(
        _wlmaker_lock_surface_avlnode_cmp,
        _wlmaker_lock_surface_avlnode_destroy);
    BS_ASSERT(NULL != lock_ptr->lock_surface_tree_ptr);

    BS_ASSERT(wlmtk_util_wl_list_for_each(
                  &lock_ptr->wlr_output_layout_ptr->outputs,
                  _wlmaker_lock_update_output,
                  &arg));

    if (!arg.has_activated_surface &&
        0 < bs_avltree_size(lock_ptr->lock_surface_tree_ptr)) {
        bs_avltree_node_t *avlnode_ptr = BS_ASSERT_NOTNULL(
            bs_avltree_min(lock_ptr->lock_surface_tree_ptr));
        wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmaker_lock_surface_t, avlnode);
        wlmtk_surface_set_activated(lock_surface_ptr->wlmtk_surface_ptr, true);
    }

    bs_avltree_destroy(arg.former_lock_surface_tree_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the lock surfaces for an output: If there is a surface for the
 * output, moves it into the new surface tree, and sends a configure() event
 * with new dimensions. Otherwise, a no-op.
 *
 * @param link_ptr
 * @param ud_ptr              A @ref wlmaker_lock_update_output_arg_t.
 *
 * @return true if the operation succeeded. It may fail if there's multiple
 * surface per output; although that would be indicative of data corruption.
 */
bool _wlmaker_lock_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_ptr->output;
    wlmaker_lock_update_output_arg_t *arg_ptr = ud_ptr;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        arg_ptr->former_lock_surface_tree_ptr, wlr_output_ptr);
    if (NULL == avlnode_ptr) {
        // We have a new output, but no lock surface. That can happen. The
        // curtain will have to cover it.
        return true;
    }

    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_lock_surface_t, avlnode);
    BS_ASSERT(
        wlr_output_ptr =
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);

    // The output dimensions may have changed. Send a configure().
    arg_ptr->lock_ptr->injected_surface_configure(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output->width,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output->height);

    struct wlr_box box;
    wlr_output_layout_get_box(
        arg_ptr->lock_ptr->wlr_output_layout_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output,
        &box);
    wlmtk_element_set_position(
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr),
        box.x, box.y);

    if (wlmtk_surface_is_activated(lock_surface_ptr->wlmtk_surface_ptr)) {
        arg_ptr->has_activated_surface = true;
    }

    return bs_avltree_insert(
        arg_ptr->lock_ptr->lock_surface_tree_ptr,
        wlr_output_ptr,
        &lock_surface_ptr->avlnode,
        false);
}

/* == Lock surface methods ================================================= */

/* ------------------------------------------------------------------------- */
/**
 * Creates a lock surface.
 *
 * @param wlr_session_lock_surface_v1_ptr
 * @param lock_ptr
 *
 * @return The lock surface or NULL on error.
 */
wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr)
{
    // Guard clause: We expect the output to be set.
    if (NULL == wlr_session_lock_surface_v1_ptr->output) {
        bs_log(BS_ERROR, "Session lock surface %p does not have an output!",
               wlr_session_lock_surface_v1_ptr);
        return NULL;
    }

    wlmaker_lock_surface_t *lock_surface_ptr = logged_calloc(
        1, sizeof(wlmaker_lock_surface_t));
    if (NULL == lock_surface_ptr) return NULL;
    lock_surface_ptr->wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_ptr;
    lock_surface_ptr->lock_ptr = lock_ptr;

    lock_surface_ptr->wlmtk_surface_ptr = wlmtk_surface_create(
        wlr_session_lock_surface_v1_ptr->surface,
        lock_ptr->wlr_seat_ptr);
    if (NULL == lock_surface_ptr->wlmtk_surface_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_surface_create(%p)",
               wlr_session_lock_surface_v1_ptr->surface);
        _wlmaker_lock_surface_destroy(lock_surface_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_session_lock_surface_v1_ptr->events.destroy,
        &lock_surface_ptr->destroy_listener,
        _wlmaker_lock_surface_handle_destroy);

    wlmtk_util_connect_listener_signal(
        &wlr_session_lock_surface_v1_ptr->surface->events.commit,
        &lock_surface_ptr->surface_commit_listener,
        _wlmaker_lock_surface_handle_surface_commit);

    // We need computed & scaled output resolution for setting the lock
    // surface's dimensions.
    int w, h;
    wlr_output_effective_resolution(
        wlr_session_lock_surface_v1_ptr->output, &w, &h);
    lock_surface_ptr->configure_serial = lock_ptr->injected_surface_configure(
        wlr_session_lock_surface_v1_ptr, w, h);

    struct wlr_box box;
    wlr_output_layout_get_box(
        lock_ptr->wlr_output_layout_ptr,
        wlr_session_lock_surface_v1_ptr->output,
        &box);
    wlmtk_element_set_position(
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr),
        box.x, box.y);
    return lock_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the lock surface.
 *
 * @param lock_surface_ptr
 */
void _wlmaker_lock_surface_destroy(wlmaker_lock_surface_t *lock_surface_ptr)
{
    bs_log(BS_INFO, "Lock %p: Destroying lock surface %p",
           lock_surface_ptr->lock_ptr, lock_surface_ptr);

    bs_avltree_delete(
        lock_surface_ptr->lock_ptr->lock_surface_tree_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);

    if (NULL != wlmtk_surface_element(
            lock_surface_ptr->wlmtk_surface_ptr)->parent_container_ptr) {
        wlmtk_container_remove_element(
            &lock_surface_ptr->lock_ptr->container,
            wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr));
    }

    wl_list_remove(&lock_surface_ptr->surface_commit_listener.link);
    wl_list_remove(&lock_surface_ptr->destroy_listener.link);

    if (NULL != lock_surface_ptr->wlmtk_surface_ptr) {
        wlmtk_surface_destroy(lock_surface_ptr->wlmtk_surface_ptr);
        lock_surface_ptr->wlmtk_surface_ptr =  NULL;
    }

    free(lock_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_surface_v1`: Destroy
 * the surface.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_surface_t, destroy_listener);
    _wlmaker_lock_surface_destroy(lock_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal of `wlr_session_lock_surface_v1::surface`.
 *
 * Checks whether the serial is at-or-above the 'configure' serial, and
 * reports the surface and output as locked. Once all surfaces are locked,
 * a 'send_locked' event will be sent.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmaker_lock_surface_handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_surface_t, surface_commit_listener);
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr;

    // Do not accept locking for commits before the requested configuration.
    if (wlr_session_lock_surface_v1_ptr->current.configure_serial >=
        lock_surface_ptr->configure_serial) {
        _wlmaker_lock_register_lock_surface(
            lock_surface_ptr->lock_ptr, lock_surface_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Comparator for @ref wlmaker_lock_t::lock_surface_tree_ptr by output. */
int _wlmaker_lock_surface_avlnode_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_lock_surface_t, avlnode);
    return bs_avltree_cmp_ptr(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor for  @ref wlmaker_lock_t::lock_surface_tree_ptr nodes. */
void _wlmaker_lock_surface_avlnode_destroy(
    bs_avltree_node_t *avlnode_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_lock_surface_t, avlnode);
    _wlmaker_lock_surface_destroy(lock_surface_ptr);
}

/* == Unit tests =========================================================== */

static void test_lock_unlock(bs_test_t *test_ptr);
static void test_lock_crash(bs_test_t *test_ptr);
static void test_lock_multi_output(bs_test_t *test_ptr);

static uint32_t _mock_wlr_session_lock_surface_v1_configure(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height);
static void _mock_wlr_session_lock_v1_send_locked(
    struct wlr_session_lock_v1 *lock);
static void _init_test_session_lock(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr);
static void _init_test_surface(struct wlr_surface *wlr_surface_ptr);

const bs_test_case_t wlmaker_lock_mgr_test_cases[] = {
    { 1, "lock_unlock", test_lock_unlock },
    { 1, "lock_crash", test_lock_crash },
    { 1, "lock_multi_output", test_lock_multi_output },
    { 0, NULL, NULL }
};

/** Return value for @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_serial;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_width;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static uint32_t _mock_configure_height;
/** Arg of last call to @ref _mock_wlr_session_lock_surface_v1_configure. */
static struct wlr_session_lock_surface_v1 *_mock_configure_lock_surface;
/** Arg of last call to @ref _mock_wlr_session_lock_v1_send_locked. */
static struct wlr_session_lock_v1 *_mock_send_locked_lock;

/** Mock for configure(). */
uint32_t _mock_wlr_session_lock_surface_v1_configure(
    struct wlr_session_lock_surface_v1 *lock_surface,
    uint32_t width, uint32_t height)
{
    _mock_configure_lock_surface = lock_surface;
    _mock_configure_width = width;
    _mock_configure_height = height;
    return _mock_configure_serial;
}

/** Mock for send_locked(). */
void _mock_wlr_session_lock_v1_send_locked(
    struct wlr_session_lock_v1 *lock)
{
    _mock_send_locked_lock = lock;
}

/** Initializes the minimum required attributes of the session lock. */
void _init_test_session_lock(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr)
{
    wl_signal_init(&wlr_session_lock_v1_ptr->events.new_surface);
    wl_signal_init(&wlr_session_lock_v1_ptr->events.unlock);
    wl_signal_init(&wlr_session_lock_v1_ptr->events.destroy);
}

/** Initializes the minimum required attributes of the wlr_surface. */
void _init_test_surface(struct wlr_surface *wlr_surface_ptr)
{
    wl_list_init(&wlr_surface_ptr->current.subsurfaces_below);
    wl_list_init(&wlr_surface_ptr->current.subsurfaces_above);
    wl_signal_init(&wlr_surface_ptr->events.commit);
    wl_signal_init(&wlr_surface_ptr->events.destroy);
    wl_signal_init(&wlr_surface_ptr->events.map);
    wl_signal_init(&wlr_surface_ptr->events.unmap);
}

/* ------------------------------------------------------------------------- */
/** Tests locking & unlocking, proper sequence, single output. */
void test_lock_unlock(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &output);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface = {};
    _init_test_surface(&wlr_surface);
    struct wlr_session_lock_surface_v1 lock_surface = {
        .surface = &wlr_surface,
        .output = &output,
    };
    wl_signal_init(&lock_surface.events.destroy);

    // A new surface request will be greeted by a configure() event.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    // A commit, but with too-low serial. Will be ignored.
    lock_surface.current.configure_serial = 41;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // Another commit, with matching serial. Will mark as locked.
    wlr_surface.current.width = 1024;
    wlr_surface.current.height = 768;
    lock_surface.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // Client unlocks.
    wl_signal_emit(&wlr_session_lock_v1.events.unlock, NULL);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    _wlmaker_lock_destroy(lock_ptr);
    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests locking, and then the session lock going away: Must remain locked. */
void test_lock_crash(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &output);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface = {};
    _init_test_surface(&wlr_surface);
    struct wlr_session_lock_surface_v1 lock_surface = {
        .surface = &wlr_surface,
        .output = &output,
    };
    wl_signal_init(&lock_surface.events.destroy);

    // A new surface request will be greeted by a configure() event.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    // Commit with matching serial. Will mark as locked.
    wlr_surface.current.width = 1024;
    wlr_surface.current.height = 768;
    lock_surface.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    // No unlock. If the session lock is destroyed without: Lock remains.
    _wlmaker_lock_destroy(lock_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests locking with multiple outputs. Lock only when all outputs covered. */
void test_lock_multi_output(bs_test_t *test_ptr)
{
    wlmaker_server_t server = { .wl_display_ptr = wl_display_create() };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    struct wlr_output o2 = { .width = 1024, .height = 768, .scale = 1 };
    struct wlr_output o3 = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlmtk_test_wlr_output_init(&o2);
    wlmtk_test_wlr_output_init(&o3);
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &o1);
    // But not: o2.
    wlr_output_layout_add_auto(server.wlr_output_layout_ptr, &o3);
    server.root_ptr = wlmtk_root_create(NULL, server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);

    wlmtk_tile_style_t tile_style = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        server.wlr_output_layout_ptr, "name", &tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(server.root_ptr, workspace_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    struct wlr_session_lock_v1 wlr_session_lock_v1 = {};
    _init_test_session_lock(&wlr_session_lock_v1);

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(
        &wlr_session_lock_v1,
        server.wlr_output_layout_ptr,
        NULL,
        server.root_ptr,
        _mock_wlr_session_lock_surface_v1_configure,
        _mock_wlr_session_lock_v1_send_locked);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, lock_ptr);

    struct wlr_surface wlr_surface1 = {};
    _init_test_surface(&wlr_surface1);
    struct wlr_session_lock_surface_v1 lock_surface1 = {
        .surface = &wlr_surface1,
        .output = &o1,
    };
    wl_signal_init(&lock_surface1.events.destroy);
    struct wlr_surface wlr_surface2 = {};
    _init_test_surface(&wlr_surface2);
    struct wlr_session_lock_surface_v1 lock_surface2 = {
        .surface = &wlr_surface2,
        .output = &o2,
    };
    wl_signal_init(&lock_surface2.events.destroy);
    struct wlr_surface wlr_surface3 = {};
    _init_test_surface(&wlr_surface3);
    struct wlr_session_lock_surface_v1 lock_surface3 = {
        .surface = &wlr_surface3,
        .output = &o3,
    };
    wl_signal_init(&lock_surface3.events.destroy);

    // Surface 1. Create, configure, commit. No lock yet.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface1);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface1, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    wlr_surface1.current.width = 1024;
    wlr_surface1.current.height = 768;
    lock_surface1.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface1.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wlmtk_surface_t *surface_ptr = wlr_surface1.data;
    int x, y;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 0, x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y);

    // Surface 2. Create, configure, commit. Non-layout output -> ignored.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface2);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface2, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    wlr_surface2.current.width = 1024;
    wlr_surface2.current.height = 768;
    lock_surface2.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface2.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    wl_signal_emit(&lock_surface2.events.destroy, NULL);

    // Surface 3.
    _mock_configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_session_lock_v1.events.new_surface, &lock_surface3);
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface3, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, _mock_configure_height);

    wlr_surface3.current.width = 1024;
    wlr_surface3.current.height = 768;
    lock_surface3.current.configure_serial = 42;
    wl_signal_emit(&wlr_surface3.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, &wlr_session_lock_v1, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    surface_ptr = wlr_surface3.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, y);

    // o3 changes size & position. Test configure(). Remains locked.
    o3.width = 1920;
    o3.height = 1080;
    wlr_output_layout_add(server.wlr_output_layout_ptr, &o3, 1200, 200);
    _mock_configure_serial = 43;
    _mock_configure_lock_surface = NULL;
    wl_signal_emit(&server.wlr_output_layout_ptr->events.change,
                   server.wlr_output_layout_ptr);
    // Note: Issues two configure() events, the second one is for o3.
    BS_TEST_VERIFY_EQ(test_ptr, &lock_surface3, _mock_configure_lock_surface);
    BS_TEST_VERIFY_EQ(test_ptr, 1920, _mock_configure_width);
    BS_TEST_VERIFY_EQ(test_ptr, 1080, _mock_configure_height);

    wlr_surface3.current.width = 1024;
    wlr_surface3.current.height = 768;
    lock_surface3.current.configure_serial = 42;
    _mock_send_locked_lock = NULL;
    wl_signal_emit(&wlr_surface3.events.commit, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, _mock_send_locked_lock);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_root_locked(server.root_ptr));

    surface_ptr = wlr_surface1.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 3120, x);
    BS_TEST_VERIFY_EQ(test_ptr, 200, y);

    surface_ptr = wlr_surface3.data;
    wlmtk_element_get_position(wlmtk_surface_element(surface_ptr), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 1200, x);
    BS_TEST_VERIFY_EQ(test_ptr, 200, y);

    // Confirm: The most recently added surface is active.
    surface_ptr = wlr_surface3.data;
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_surface_is_activated(surface_ptr));
    wlr_output_layout_remove(server.wlr_output_layout_ptr, &o3);
    _mock_configure_serial = 44;
    _mock_configure_lock_surface = NULL;
    wl_signal_emit(&server.wlr_output_layout_ptr->events.change,
                   server.wlr_output_layout_ptr);

    // Now want surface1 active.
    surface_ptr = wlr_surface1.data;
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, surface_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_surface_is_activated(surface_ptr));

    // Unlock correctly.
    wl_signal_emit(&wlr_session_lock_v1.events.unlock, NULL);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_workspace_enabled(workspace_ptr));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_root_locked(server.root_ptr));

    _wlmaker_lock_destroy(lock_ptr);
    wlmtk_root_remove_workspace(server.root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
}

/* == End of lock_mgr.c ==================================================== */
