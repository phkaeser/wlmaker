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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_session_lock_v1.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Forward declaration: Lock surface. */
typedef struct _wlmaker_lock_surface_t wlmaker_lock_surface_t;

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

/** State of the session lock. */
struct _wlmaker_lock_t {
    /** The wlroots session lock. */
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr;

    /** Back-link to the lock manager. */
    wlmaker_lock_mgr_t        *lock_mgr_ptr;

    /** List of surfaces, via @ref wlmaker_lock_surface_t::dlnode. */
    bs_dllist_t               lock_surfaces;
    /** Container holding the lock surfaces. */
    wlmtk_container_t         container;

    /** Listener for the `new_surface` signal of `wlr_session_lock_v1`. */
    struct wl_listener        new_surface_listener;
    /** Listener for the `unlock` signal of `wlr_session_lock_v1`. */
    struct wl_listener        unlock_listener;
    /** Listener for the `destroy` signal of `wlr_session_lock_v1`. */
    struct wl_listener        destroy_listener;
};

/** State of a lock surface. */
struct _wlmaker_lock_surface_t {
    /** The wlroots session lock surface. */
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr;
    /** Toolkit surface for the associated wl_surface. */
    wlmtk_surface_t           *wlmtk_surface_ptr;
    /** Back-link to the lock. */
    wlmaker_lock_t            *lock_ptr;

    /** Link node, element of @ref wlmaker_lock_t::lock_surfaces. */
    bs_dllist_node_t          dlnode;
    /** Serial returned by `wlr_session_lock_surface_v1_configure`. */
    uint32_t                  configure_serial;

    /** Listener for the `destroy` signal of `wlr_session_lock_surface_v1`. */
    struct wl_listener        destroy_listener;
    /** Listener for `commit` signal of `wlr_session_lock_surface_v1::surface`. */
    struct wl_listener        surface_commit_listener;
};

static wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    wlmaker_lock_mgr_t *lock_mgr_ptr);
static void _wlmaker_lock_destroy(
    wlmaker_lock_t *lock_ptr);
void _wlmaker_lock_report_surface_locked(
    wlmaker_lock_t *lock_ptr,
    wlmaker_lock_surface_t *lock_surface_ptr);
static bool _wlmaker_lock_surface_has_wlr_output(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr,
    wlmaker_server_t *server_ptr);
static void _wlmaker_lock_surface_destroy(
    wlmaker_lock_surface_t *lock_surface_ptr);

static void _wlmaker_lock_mgr_handle_new_lock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_mgr_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_lock_handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_unlock(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_lock_surface_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_lock_surface_handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);

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
    wl_list_remove(&lock_mgr_ptr->destroy_listener.link);
    wl_list_remove(&lock_mgr_ptr->new_lock_listener.link);

    // Note: No destroy method for wlr_session_lock_manager_v1_ptr.

    free(lock_mgr_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmaker_lock_element(wlmaker_lock_t *lock_ptr)
{
    return &lock_ptr->container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates a session lock handle.
 *
 * @param wlr_session_lock_v1_ptr
 * @param lock_mgr_ptr
 *
 * @return The lock handle or NULL on error.
 */
wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    wlmaker_lock_mgr_t *lock_mgr_ptr)
{
    wlmaker_lock_t *lock_ptr = logged_calloc(1, sizeof(wlmaker_lock_t));
    if (NULL == lock_ptr) return NULL;
    lock_ptr->wlr_session_lock_v1_ptr = wlr_session_lock_v1_ptr;
    lock_ptr->lock_mgr_ptr = lock_mgr_ptr;

    if (!wlmtk_container_init(
            &lock_ptr->container,
            lock_mgr_ptr->server_ptr->env_ptr)) {
        wlmaker_lock_mgr_destroy(lock_mgr_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(&lock_ptr->container.super_element, true);

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
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (
               dlnode_ptr = bs_dllist_pop_front(&lock_ptr->lock_surfaces))) {
        wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmaker_lock_surface_t, dlnode);
        _wlmaker_lock_surface_destroy(lock_surface_ptr);
    }

    wl_list_remove(&lock_ptr->destroy_listener.link);
    wl_list_remove(&lock_ptr->unlock_listener.link);
    wl_list_remove(&lock_ptr->new_surface_listener.link);

    wlmaker_root_lock_unreference(
        lock_ptr->lock_mgr_ptr->server_ptr->root_ptr,
        lock_ptr);
    wlmtk_container_fini(&lock_ptr->container);

    free(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Registers the provided surface as 'locked'. Locks the session, if all
 * outputs have been locked.
 *
 * @param lock_ptr
 * @param lock_surface_ptr
 */
void _wlmaker_lock_report_surface_locked(
    wlmaker_lock_t *lock_ptr,
    wlmaker_lock_surface_t *lock_surface_ptr)
{
    // Guard clause: Don't add the surface if already reported.
    if (bs_dllist_contains(
            &lock_ptr->lock_surfaces, &lock_surface_ptr->dlnode)) return;

    // Another guard: Don't accept the same output twice.
    if (bs_dllist_find(
            &lock_ptr->lock_surfaces,
            _wlmaker_lock_surface_has_wlr_output,
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output)) {
        bs_log(BS_WARNING, "Extra lock surface detected for wlr_output %p",
               lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);
        wl_resource_post_error(
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Extra lock surface detected for wlr_output %p",
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output);
        return;
    }

    bs_dllist_push_back(&lock_ptr->lock_surfaces, &lock_surface_ptr->dlnode);
    wlmtk_container_add_element(
        &lock_ptr->container,
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr));
    wlmtk_element_set_visible(
        wlmtk_surface_element(lock_surface_ptr->wlmtk_surface_ptr), true);

    // If not all outputs are covered: No lock yet.
    if (bs_dllist_size(&lock_ptr->lock_surfaces) <
        bs_dllist_size(&lock_ptr->lock_mgr_ptr->server_ptr->outputs)) return;

    if (!wlmaker_root_lock(
            lock_ptr->lock_mgr_ptr->server_ptr->root_ptr,
            lock_ptr)) {
        wl_resource_post_error(
            lock_surface_ptr->wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_INVALID_METHOD,
            "Failed wlmaker_root_lock(%p, %p): Already locked?",
            lock_ptr->lock_mgr_ptr->server_ptr,
            lock_ptr);
        return;
    }

    wlmaker_lock_surface_t *first_surface_ptr = BS_CONTAINER_OF(
        lock_ptr->lock_surfaces.head_ptr, wlmaker_lock_surface_t, dlnode);
    wlmaker_root_set_lock_surface(
        lock_ptr->lock_mgr_ptr->server_ptr->root_ptr,
        first_surface_ptr->wlmtk_surface_ptr);

    // Root is locked. Send confirmation to the client.
    wlr_session_lock_v1_send_locked(lock_ptr->wlr_session_lock_v1_ptr);
}

/* ------------------------------------------------------------------------- */
/** Returns whether the surface at dlnode_ptr has wlr_output == ud_ptr. */
bool _wlmaker_lock_surface_has_wlr_output(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmaker_lock_surface_t, dlnode);
    return lock_surface_ptr->wlr_session_lock_surface_v1_ptr->output == ud_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a lock surface.
 *
 * @param wlr_session_lock_surface_v1_ptr
 * @param lock_ptr
 * @param server_ptr
 *
 * @return The lock surface or NULL on error.
 */
wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_lock_t *lock_ptr,
    wlmaker_server_t *server_ptr)
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
        server_ptr->env_ptr);
    if (NULL == lock_surface_ptr->wlmtk_surface_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_surface_create(%p, %p",
               wlr_session_lock_surface_v1_ptr->surface,
               server_ptr->env_ptr);
        _wlmaker_lock_surface_destroy(lock_surface_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &lock_surface_ptr->wlr_session_lock_surface_v1_ptr->events.destroy,
        &lock_surface_ptr->destroy_listener,
        _wlmaker_lock_surface_handle_destroy);

    wlmtk_util_connect_listener_signal(
        &lock_surface_ptr->wlr_session_lock_surface_v1_ptr->surface->events.commit,
        &lock_surface_ptr->surface_commit_listener,
        _wlmaker_lock_surface_handle_surface_commit);

    lock_surface_ptr->configure_serial = wlr_session_lock_surface_v1_configure(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr,
        wlr_session_lock_surface_v1_ptr->output->width,
        wlr_session_lock_surface_v1_ptr->output->height);

    return lock_surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the lock surface.
 *
 * @param lock_surface_ptr
 */
void _wlmaker_lock_surface_destroy(
    wlmaker_lock_surface_t *lock_surface_ptr)
{
    if (bs_dllist_contains(&lock_surface_ptr->lock_ptr->lock_surfaces,
                           &lock_surface_ptr->dlnode)) {
        bs_dllist_remove(&lock_surface_ptr->lock_ptr->lock_surfaces,
                         &lock_surface_ptr->dlnode);
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

    wlmaker_lock_t *lock_ptr = _wlmaker_lock_create(data_ptr, lock_mgr_ptr);

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
        lock_ptr,
        lock_ptr->lock_mgr_ptr->server_ptr);
    if (NULL == lock_surface_ptr) {
        wl_resource_post_error(
            wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmaker_lock_surface_create(%p, %p, %p)",
            wlr_session_lock_surface_v1_ptr->surface,
            lock_ptr,
            lock_ptr->lock_mgr_ptr->server_ptr);
        return;
    }

    bs_log(BS_INFO, "Lock mgr %p, lock %p: New lock surface %p",
           lock_ptr->lock_mgr_ptr, lock_ptr, lock_surface_ptr);
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
    wlmaker_lock_t *lock_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_t, unlock_listener);

    wlmaker_root_unlock(
        lock_ptr->lock_mgr_ptr->server_ptr->root_ptr,
        lock_ptr);
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
    void *data_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_lock_surface_t, surface_commit_listener);

    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_try_from_wlr_surface(data_ptr);

    // Do not accept locking for commits before the requested configuration.
    if (wlr_session_lock_surface_v1_ptr->current.configure_serial >=
        lock_surface_ptr->configure_serial) {
        _wlmaker_lock_report_surface_locked(
            lock_surface_ptr->lock_ptr, lock_surface_ptr);
    }
}

/* == End of lock_mgr.c ==================================================== */
