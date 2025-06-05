/* ========================================================================= */
/**
 * @file lock.c
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

#include "lock.h"

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
#include <wlr/types/wlr_session_lock_v1.h>
#undef WLR_USE_UNSTABLE
#include "libbase/libbase.h"

#include "container.h"
#include "content.h"
#include "surface.h"
#include "util.h"

struct _wlmtk_lock_surface_t;

/* == Declarations ========================================================= */

/** Forward declaration: Lock surface. */
typedef struct _wlmtk_lock_surface_t wlmtk_lock_surface_t;

/** State of the session lock. */
struct _wlmtk_lock_t {
    /** The wlroots session lock. */
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr;
    /** The root this lock is applied for. */
    wlmtk_root_t              *root_ptr;

    /** The output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Holds all @ref wlmtk_lock_surface_t, keyed by output. */
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
struct _wlmtk_lock_surface_t {
    /** The wlroots session lock surface. */
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr;
    /** Toolkit surface for the associated wl_surface. */
    wlmtk_surface_t           *wlmtk_surface_ptr;
    /** Back-link to the lock. */
    wlmtk_lock_t              *lock_ptr;

    /** Tree node, element of @ref wlmtk_lock_t::lock_surface_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** Serial returned by `wlr_session_lock_surface_v1_configure`. */
    uint32_t                  configure_serial;

    /** Listener for the `destroy` signal of `wlr_session_lock_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for `commit` signal of `wlr_session_lock_surface_v1::surface`. */
    struct wl_listener        surface_commit_listener;
};

/** Arguent to @ref _wlmtk_lock_update_output. */
typedef struct {
    /** The lock we're working on. */
    wlmtk_lock_t              *lock_ptr;
    /** The former lock surface tree. */
    bs_avltree_t              *former_lock_surface_tree_ptr;
} wlmtk_lock_update_output_arg_t;

static void _wlmtk_lock_register_lock_surface(
    wlmtk_lock_t *lock_ptr,
    wlmtk_lock_surface_t *lock_surface_ptr);

static void _wlmtk_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_lock_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static bool _wlmtk_lock_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr);

static wlmtk_lock_surface_t *_wlmtk_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmtk_lock_t *lock_ptr);
static void _wlmtk_lock_surface_destroy(
    wlmtk_lock_surface_t *lock_surface_ptr);

static void _wlmtk_lock_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_lock_surface_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static int _wlmtk_lock_surface_avlnode_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);
static void _wlmtk_lock_surface_avlnode_destroy(
    bs_avltree_node_t *node_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_lock_t *wlmtk_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_root_t *root_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_lock_t *lock_ptr = logged_calloc(1, sizeof(wlmtk_lock_t));
    if (NULL == lock_ptr) return NULL;
    lock_ptr->wlr_session_lock_v1_ptr = wlr_session_lock_v1_ptr;
    lock_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    lock_ptr->root_ptr = root_ptr;

    lock_ptr->lock_surface_tree_ptr = bs_avltree_create(
        _wlmtk_lock_surface_avlnode_cmp,
        _wlmtk_lock_surface_avlnode_destroy);
    if (NULL == lock_ptr->lock_surface_tree_ptr) {
        bs_log(BS_ERROR, "Failed bs_avltree_create(%p, %p)",
               _wlmtk_lock_surface_avlnode_cmp,
               _wlmtk_lock_surface_avlnode_destroy);
        wlmtk_lock_destroy(lock_ptr);
        return NULL;
    }

    if (!wlmtk_container_init(&lock_ptr->container, env_ptr)) {
        wlmtk_lock_destroy(lock_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.new_surface,
        &lock_ptr->new_surface_listener,
        _wlmtk_lock_handle_new_surface);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.unlock,
        &lock_ptr->unlock_listener,
        _wlmtk_lock_handle_unlock);
    wlmtk_util_connect_listener_signal(
        &lock_ptr->wlr_session_lock_v1_ptr->events.destroy,
        &lock_ptr->destroy_listener,
        _wlmtk_lock_handle_destroy);

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &lock_ptr->output_layout_change_listener,
        _wlmtk_lock_handle_output_layout_change);
    _wlmtk_lock_handle_output_layout_change(
        &lock_ptr->output_layout_change_listener,
        NULL);

    return lock_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_lock_destroy(wlmtk_lock_t *lock_ptr)
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
                                wlmtk_lock_element(lock_ptr));
    wlmtk_container_fini(&lock_ptr->container);

    free(lock_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_lock_element(wlmtk_lock_t *lock_ptr)
{
    return &lock_ptr->container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Registers the provided surface as a lock surface for it's output, unless
 * it had already been registered. Locks the session, once lock surfaces for
 * all surfaces have been registered.
 *
 * @param lock_ptr
 * @param lock_surface_ptr
 */
void _wlmtk_lock_register_lock_surface(
    wlmtk_lock_t *lock_ptr,
    wlmtk_lock_surface_t *lock_surface_ptr)
{
    // Guard clause: Don't register more than once.
    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        lock_ptr->lock_surface_tree_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);
    if (NULL != avlnode_ptr) {
        wlmtk_lock_surface_t *existing_lock_surface_ptr = BS_CONTAINER_OF(
            avlnode_ptr, wlmtk_lock_surface_t, avlnode);
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

    if (!wlmtk_root_lock(lock_ptr->root_ptr, wlmtk_lock_element(lock_ptr))) {
        wl_resource_post_error(
            lock_ptr->wlr_session_lock_v1_ptr->resource,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Failed wlmtk_root_lock(%p, %p): Already locked?",
            lock_ptr->root_ptr, wlmtk_lock_element(lock_ptr));
        return;
    }
    wlmtk_element_set_visible(&lock_ptr->container.super_element, true);
    // Grant keyboard focus to the lock surface we just added.
    wlmtk_surface_set_activated(lock_surface_ptr->wlmtk_surface_ptr, true);

    // Root is locked. Send confirmation to the client.
    wlr_session_lock_v1_send_locked(lock_ptr->wlr_session_lock_v1_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `new_surface` signal of `wlr_session_lock_v1`: Creates the
 * associated surface and enables it on the screenlock container.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_t, new_surface_listener);
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        data_ptr;

    wlmtk_lock_surface_t *lock_surface_ptr = _wlmtk_lock_surface_create(
        wlr_session_lock_surface_v1_ptr,
        lock_ptr);
    if (NULL == lock_surface_ptr) {
        wl_resource_post_error(
            wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmtk_lock_surface_create(%p, %p)",
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
void _wlmtk_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    __UNUSED__ wlmtk_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_t, unlock_listener);

    wlmtk_element_set_visible(&lock_ptr->container.super_element, false);
    wlmtk_root_unlock(lock_ptr->root_ptr, wlmtk_lock_element(lock_ptr));
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `destroy` signal of `wlr_session_lock_v1`: Destroy the lock.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_t, destroy_listener);
    wlmtk_lock_destroy(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the `change` event of wlr_output_layout::events.
 *
 * Walks through outputs of @ref wlmtk_lock_t::wlr_output_layout_ptr, and
 * updates @ref wlmtk_lock_t::lock_surface_tree_ptr accordingly.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_lock_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_t, output_layout_change_listener);

    wlmtk_lock_update_output_arg_t arg = {
        .lock_ptr = lock_ptr,
        .former_lock_surface_tree_ptr = lock_ptr->lock_surface_tree_ptr
    };

    lock_ptr->lock_surface_tree_ptr = bs_avltree_create(
        _wlmtk_lock_surface_avlnode_cmp,
        _wlmtk_lock_surface_avlnode_destroy);
    BS_ASSERT(NULL != lock_ptr->lock_surface_tree_ptr);

    BS_ASSERT(wlmtk_util_wl_list_for_each(
                  &lock_ptr->wlr_output_layout_ptr->outputs,
                  _wlmtk_lock_update_output,
                  &arg));

    bs_avltree_destroy(arg.former_lock_surface_tree_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates the lock surfaces for an output: If there is a surface for the
 * output, moves it into the new surface tree, and sends a configure() event
 * with new dimensions. Otherwise, a no-op.
 *
 * @param link_ptr
 * @param ud_ptr              A @ref wlmtk_lock_update_output_arg_t.
 *
 * @return true if the operation succeeded. It may fail if there's multiple
 * surface per output; although that would be indicative of data corruption.
 */
bool _wlmtk_lock_update_output(
    struct wl_list *link_ptr,
    void *ud_ptr)
{
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
    struct wlr_output *wlr_output_ptr = wlr_output_layout_output_ptr->output;
    wlmtk_lock_update_output_arg_t *arg_ptr = ud_ptr;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        arg_ptr->former_lock_surface_tree_ptr, wlr_output_ptr);
    if (NULL == avlnode_ptr) {
        // We have a new output, but no lock surface. That can happen. The
        // curtain will have to cover it.
        return true;
    }

    wlmtk_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_lock_surface_t, avlnode);
    BS_ASSERT(
        wlr_output_ptr =
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);

    // The output dimensions may have changed. Send a configure().
    wlr_session_lock_surface_v1_configure(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output->width,
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output->height);

    return bs_avltree_insert(
        arg_ptr->lock_ptr->lock_surface_tree_ptr,
        wlr_output_ptr,
        &lock_surface_ptr->avlnode,
        false);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a lock surface.
 *
 * @param wlr_session_lock_surface_v1_ptr
 * @param lock_ptr
 *
 * @return The lock surface or NULL on error.
 */
wlmtk_lock_surface_t *_wlmtk_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmtk_lock_t *lock_ptr)
{
    // Guard clause: We expect the output to be set.
    if (NULL == wlr_session_lock_surface_v1_ptr->output) {
        bs_log(BS_ERROR, "Session lock surface %p does not have an output!",
               wlr_session_lock_surface_v1_ptr);
        return NULL;
    }

    wlmtk_lock_surface_t *lock_surface_ptr = logged_calloc(
        1, sizeof(wlmtk_lock_surface_t));
    if (NULL == lock_surface_ptr) return NULL;
    lock_surface_ptr->wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_ptr;
    lock_surface_ptr->lock_ptr = lock_ptr;

    lock_surface_ptr->wlmtk_surface_ptr = wlmtk_surface_create(
        wlr_session_lock_surface_v1_ptr->surface,
        wlmtk_lock_element(lock_ptr)->env_ptr);
    if (NULL == lock_surface_ptr->wlmtk_surface_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_surface_create(%p, %p)",
               wlr_session_lock_surface_v1_ptr->surface,
               wlmtk_lock_element(lock_ptr)->env_ptr);
        _wlmtk_lock_surface_destroy(lock_surface_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_surface_ptr->wlr_session_lock_surface_v1_ptr->events.destroy,
        &lock_surface_ptr->destroy_listener,
        _wlmtk_lock_surface_handle_destroy);

    wlmtk_util_connect_listener_signal(
        &lock_surface_ptr->wlr_session_lock_surface_v1_ptr->surface->events.commit,
        &lock_surface_ptr->surface_commit_listener,
        _wlmtk_lock_surface_handle_surface_commit);

    // We need computed & scaled output resolution for setting the lock
    // surface's dimensions.
    int w, h;
    wlr_output_effective_resolution(
        wlr_session_lock_surface_v1_ptr->output, &w, &h);
    lock_surface_ptr->configure_serial = wlr_session_lock_surface_v1_configure(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr, w, h);

    return lock_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the lock surface.
 *
 * @param lock_surface_ptr
 */
void _wlmtk_lock_surface_destroy(wlmtk_lock_surface_t *lock_surface_ptr)
{
    bs_log(BS_INFO, "Lock %p: Destroying lock surface %p",
           lock_surface_ptr->lock_ptr, lock_surface_ptr);

    if (NULL != bs_avltree_delete(
            lock_surface_ptr->lock_ptr->lock_surface_tree_ptr,
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output)) {
        // Means: This lock surface had been registered.
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
void _wlmtk_lock_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_surface_t, destroy_listener);
    _wlmtk_lock_surface_destroy(lock_surface_ptr);
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
void _wlmtk_lock_surface_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_lock_surface_t, surface_commit_listener);

    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_try_from_wlr_surface(data_ptr);

    // Do not accept locking for commits before the requested configuration.
    if (wlr_session_lock_surface_v1_ptr->current.configure_serial >=
        lock_surface_ptr->configure_serial) {
        _wlmtk_lock_register_lock_surface(
            lock_surface_ptr->lock_ptr, lock_surface_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Comparator for @ref wlmtk_lock_t::lock_surface_tree_ptr by output. */
int _wlmtk_lock_surface_avlnode_cmp(
    const bs_avltree_node_t *avlnode_ptr,
    const void *key_ptr)
{
    wlmtk_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_lock_surface_t, avlnode);
    return bs_avltree_cmp_ptr(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor for  @ref wlmtk_lock_t::lock_surface_tree_ptr nodes. */
void _wlmtk_lock_surface_avlnode_destroy(
    bs_avltree_node_t *avlnode_ptr)
{
    wlmtk_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmtk_lock_surface_t, avlnode);
    _wlmtk_lock_surface_destroy(lock_surface_ptr);
}

/* == End of lock.c ======================================================== */
