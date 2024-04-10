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

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Forward declaration: Lock. */
typedef struct _wlmaker_lock_t wlmaker_lock_t;
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
    /** Listener for the `destroy` signal of `wlr_session_lock_surface_v1`. */
    struct wl_listener        destroy_listener;

    /** Serial returned by `wlr_session_lock_surface_v1_configure`. */
    uint32_t                  serial;
    /** Listener for `commit` signal of `wlr_session_lock_surface_v1::surface`. */
    struct wl_listener        surface_commit_listener;
};

static wlmaker_lock_t *_wlmaker_lock_create(
    struct wlr_session_lock_v1 *wlr_session_lock_v1_ptr,
    wlmaker_lock_mgr_t *lock_mgr_ptr);
static void _wlmaker_lock_destroy(
    wlmaker_lock_t *lock_ptr);

static wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
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
    wl_list_remove(&lock_ptr->destroy_listener.link);
    wl_list_remove(&lock_ptr->unlock_listener.link);
    wl_list_remove(&lock_ptr->new_surface_listener.link);

    free(lock_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a lock surface.
 *
 * @param wlr_session_lock_surface_v1_ptr
 * @param server_ptr
 *
 * @return The lock surface or NULL on error.
 */
wlmaker_lock_surface_t *_wlmaker_lock_surface_create(
    struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_lock_surface_t *lock_surface_ptr = logged_calloc(
        1, sizeof(wlmaker_lock_surface_t));
    if (NULL == lock_surface_ptr) return NULL;
    lock_surface_ptr->wlr_session_lock_surface_v1_ptr =
        wlr_session_lock_surface_v1_ptr;

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


    lock_surface_ptr->serial = wlr_session_lock_surface_v1_configure(
        lock_surface_ptr->wlr_session_lock_surface_v1_ptr,
        640, 480);

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

    bs_log(BS_ERROR, "FIXME: Destroy!");
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
        lock_ptr->lock_mgr_ptr->server_ptr);
    if (NULL == lock_surface_ptr) {
        wl_resource_post_error(
            wlr_session_lock_surface_v1_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed _wlmaker_lock_surface_create(%p, %p)",
            wlr_session_lock_surface_v1_ptr->surface,
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

    bs_log(BS_INFO, "FIXME: Unlock %p", lock_ptr);
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

    bs_log(BS_ERROR, "FIXME: commit %p %p - %"PRIu32, lock_surface_ptr,
           wlr_session_lock_surface_v1_ptr,
           wlr_session_lock_surface_v1_ptr->current.configure_serial);

    // FIXME: Report this output as locked, and "send_locked" if all outputs.
}

/* == End of lock_mgr.c ==================================================== */
