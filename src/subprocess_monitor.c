/* ========================================================================= */
/**
 * @file subprocess_monitor.c
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

#include "subprocess_monitor.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include "toolkit/toolkit.h"

struct wl_event_loop;
struct wl_event_source;

/* == Declarations ========================================================= */

/** State of the subprocess monitor. */
struct _wlmaker_subprocess_monitor_t {
    /** Reference to the event loop. */
    struct wl_event_loop      *wl_event_loop_ptr;
    /** Event source used for monitoring SIGCHLD. */
    struct wl_event_source    *sigchld_event_source_ptr;

    /** Listener: Receives a signal whenever a window is created. */
    struct wl_listener        window_created_listener;
    /** Listener: Receives a signal whenever a window is mapped. */
    struct wl_listener        window_mapped_listener;
    /** Listener: Receives a signal whenever a window is unmapped. */
    struct wl_listener        window_unmapped_listener;
    /** Listener: Receives a signal whenever a window is destroyed. */
    struct wl_listener        window_destroyed_listener;

    /** Monitored subprocesses. */
    bs_dllist_t               subprocesses;
    /** Windows for monitored subprocesses. */
    bs_avltree_t              *window_tree_ptr;
};

/** A subprocess. */
struct _wlmaker_subprocess_handle_t {
    /** Element of @ref wlmaker_subprocess_monitor_t `subprocesses`. */
    bs_dllist_node_t          dlnode;
    /** Points to the subprocess. */
    bs_subprocess_t           *subprocess_ptr;

    /** File descriptor of the subprocess' stdout. */
    int                       stdout_read_fd;
    /** Event source corresponding to events related to reading stdout. */
    struct wl_event_source    *stdout_wl_event_source_ptr;
    /** File descriptor of the subprocess' strderr. */
    int                       stderr_read_fd;
    /** Event source corresponding to events related to reading stderr. */
    struct wl_event_source    *stderr_wl_event_source_ptr;

    /** Callback:  The subprocess was terminated. */
    wlmaker_subprocess_terminated_callback_t terminated_callback;
    /** Argument to all the callbacks. */
    void                      *userdata_ptr;
    /** Subprocess's windows. @ref wlmaker_subprocess_window_t::dlnode. */
    bs_dllist_t               windows;

    /** Dynamic buffer holding the process' stdout, or NULL if not set. */
    bs_dynbuf_t               *stdout_dynbuf_ptr;

    /** Callback: A window was created from this subprocess. */
    wlmaker_subprocess_window_callback_t window_created_callback;
    /** Callback: Window was mapped from this subprocess. */
    wlmaker_subprocess_window_callback_t window_mapped_callback;
    /** Callback: Window was unmapped from this subprocess. */
    wlmaker_subprocess_window_callback_t window_unmapped_callback;
    /** Callback: Window was destroyed from this subprocess. */
    wlmaker_subprocess_window_callback_t window_destroyed_callback;
};

/** Registry entry for @ref wlmtk_window_t and subprocesses. */
typedef struct {
    /** See @ref wlmaker_subprocess_monitor_t::window_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** The window registered here. Also the tree lookup key. */
    wlmtk_window_t            *window_ptr;

    /** See @ref wlmaker_subprocess_handle_t::windows. */
    bs_dllist_node_t          dlnode;
    /** The subprocess that the window is mapped to, or NULL. */
    wlmaker_subprocess_handle_t *subprocess_handle_ptr;

    /** Whether the window was reported as mapped. */
    bool                      mapped;
} wlmaker_subprocess_window_t;

static wlmaker_subprocess_handle_t *wlmaker_subprocess_handle_create(
    bs_subprocess_t *subprocess_ptr,
    struct wl_event_loop *wl_event_loop_ptr);
static void wlmaker_subprocess_handle_destroy(
    wlmaker_subprocess_handle_t *sp_handle_ptr);
static int _wlmaker_subprocess_monitor_handle_read_stdout(
    int fd, uint32_t mask, void *data_ptr);
static int _wlmaker_subprocess_monitor_handle_read_stderr(
    int fd, uint32_t mask, void *data_ptr);

static int _wlmaker_subprocess_monitor_process_fd(
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    struct wl_event_source **wl_event_source_ptr_ptr,
    int fd,
    uint32_t mask,
    const char *fd_name_ptr,
    bs_dynbuf_t *dynbuf_ptr);

static int _wlmaker_subprocess_monitor_handle_sigchld(int signum, void *data_ptr);

static void _wlmaker_subprocess_monitor_handle_window_created(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_subprocess_monitor_handle_window_mapped(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_subprocess_monitor_handle_window_unmapped(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_subprocess_monitor_handle_window_destroyed(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static wlmaker_subprocess_handle_t *subprocess_handle_from_window(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    wlmtk_window_t *window_ptr);

static wlmaker_subprocess_window_t *wlmaker_subprocess_window_create(
    wlmtk_window_t *window_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr);
static void wlmaker_subprocess_window_destroy(
    wlmaker_subprocess_window_t *ws_window_ptr);

static int wlmaker_subprocess_window_node_cmp(
    const bs_avltree_node_t *node_ptr,
    const void *key_ptr);
static void wlmaker_subprocess_window_node_destroy(
    bs_avltree_node_t *node_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_subprocess_monitor_t* wlmaker_subprocess_monitor_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = logged_calloc(
        1, sizeof(wlmaker_subprocess_monitor_t));
    if (NULL == monitor_ptr) return NULL;

    monitor_ptr->window_tree_ptr = bs_avltree_create(
        wlmaker_subprocess_window_node_cmp,
        wlmaker_subprocess_window_node_destroy);
    if (NULL == monitor_ptr->window_tree_ptr) {
        bs_log(BS_ERROR, "Failed bs_avltree_create(%p, %p)",
               wlmaker_subprocess_window_node_cmp,
               wlmaker_subprocess_window_node_destroy);
        wlmaker_subprocess_monitor_destroy(monitor_ptr);
        return NULL;
    }

    monitor_ptr->wl_event_loop_ptr = wl_display_get_event_loop(
        server_ptr->wl_display_ptr);
    if (NULL == monitor_ptr->wl_event_loop_ptr) {
        bs_log(BS_ERROR, "Failed wl_display_get_event_loop().");
        wlmaker_subprocess_monitor_destroy(monitor_ptr);
        return NULL;
    }

    monitor_ptr->sigchld_event_source_ptr = wl_event_loop_add_signal(
        monitor_ptr->wl_event_loop_ptr,
        SIGCHLD,
        _wlmaker_subprocess_monitor_handle_sigchld,
        monitor_ptr);

    wlmtk_util_connect_listener_signal(
        &server_ptr->window_created_event,
        &monitor_ptr->window_created_listener,
        _wlmaker_subprocess_monitor_handle_window_created);
    wlmtk_util_connect_listener_signal(
        &server_ptr->window_destroyed_event,
        &monitor_ptr->window_destroyed_listener,
        _wlmaker_subprocess_monitor_handle_window_destroyed);

    if (NULL != server_ptr->root_ptr) {
        wlmtk_util_connect_listener_signal(
            &wlmtk_root_events(server_ptr->root_ptr)->window_mapped,
            &monitor_ptr->window_mapped_listener,
            _wlmaker_subprocess_monitor_handle_window_mapped);
        wlmtk_util_connect_listener_signal(
            &wlmtk_root_events(server_ptr->root_ptr)->window_unmapped,
            &monitor_ptr->window_unmapped_listener,
            _wlmaker_subprocess_monitor_handle_window_unmapped);
    }
    return monitor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_subprocess_monitor_destroy(
    wlmaker_subprocess_monitor_t *monitor_ptr)
{
    wl_list_remove(&monitor_ptr->window_destroyed_listener.link);
    wl_list_remove(&monitor_ptr->window_created_listener.link);
    wl_list_remove(&monitor_ptr->window_unmapped_listener.link);
    wl_list_remove(&monitor_ptr->window_mapped_listener.link);

    if (NULL != monitor_ptr->sigchld_event_source_ptr) {
        wl_event_source_remove(monitor_ptr->sigchld_event_source_ptr);
        monitor_ptr->sigchld_event_source_ptr = NULL;
    }

    if (NULL != monitor_ptr->window_tree_ptr) {
        bs_avltree_destroy(monitor_ptr->window_tree_ptr);
        monitor_ptr->window_tree_ptr = NULL;
    }

    monitor_ptr->wl_event_loop_ptr = NULL;
    free(monitor_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_subprocess_monitor_run(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr)
{
    if (NULL == subprocess_ptr) return false;
    if (!bs_subprocess_start(subprocess_ptr)) goto error;

    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        wlmaker_subprocess_monitor_entrust(
            monitor_ptr,
            subprocess_ptr,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);
    if (NULL == subprocess_handle_ptr) goto error;

    wlmaker_subprocess_monitor_cede(monitor_ptr, subprocess_handle_ptr);
    return true;

error:
    if (NULL != subprocess_ptr) bs_subprocess_destroy(subprocess_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
wlmaker_subprocess_handle_t *wlmaker_subprocess_monitor_entrust(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr,
    wlmaker_subprocess_terminated_callback_t terminated_callback,
    void *userdata_ptr,
    wlmaker_subprocess_window_callback_t window_created_callback,
    wlmaker_subprocess_window_callback_t window_mapped_callback,
    wlmaker_subprocess_window_callback_t window_unmapped_callback,
    wlmaker_subprocess_window_callback_t window_destroyed_callback,
    bs_dynbuf_t *stdout_dynbuf_ptr)
{
    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        wlmaker_subprocess_handle_create(
            subprocess_ptr, monitor_ptr->wl_event_loop_ptr);
    if (NULL == subprocess_handle_ptr) return NULL;
    bs_dllist_push_back(&monitor_ptr->subprocesses,
                        &subprocess_handle_ptr->dlnode);

    subprocess_handle_ptr->terminated_callback = terminated_callback;
    subprocess_handle_ptr->userdata_ptr = userdata_ptr;
    subprocess_handle_ptr->window_created_callback = window_created_callback;
    subprocess_handle_ptr->window_mapped_callback = window_mapped_callback;
    subprocess_handle_ptr->window_unmapped_callback = window_unmapped_callback;
    subprocess_handle_ptr->window_destroyed_callback =
        window_destroyed_callback;
    subprocess_handle_ptr->stdout_dynbuf_ptr = stdout_dynbuf_ptr;

    return subprocess_handle_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_subprocess_monitor_cede(
    __UNUSED__ wlmaker_subprocess_monitor_t *monitor_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = bs_dllist_pop_front(
                        &subprocess_handle_ptr->windows))) {
        wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmaker_subprocess_window_t, dlnode);
        BS_ASSERT(ws_window_ptr->subprocess_handle_ptr ==
                  subprocess_handle_ptr) ;

        if (NULL != subprocess_handle_ptr->window_unmapped_callback) {
            subprocess_handle_ptr->window_unmapped_callback(
                subprocess_handle_ptr->userdata_ptr,
                subprocess_handle_ptr,
                ws_window_ptr->window_ptr);
            ws_window_ptr->mapped = false;
        }
        if (NULL != subprocess_handle_ptr->window_destroyed_callback) {
            subprocess_handle_ptr->window_destroyed_callback(
                subprocess_handle_ptr->userdata_ptr,
                subprocess_handle_ptr,
                ws_window_ptr->window_ptr);
        }

        ws_window_ptr->subprocess_handle_ptr = NULL;
    }

    subprocess_handle_ptr->terminated_callback = NULL;
}

/* ------------------------------------------------------------------------- */
bs_subprocess_t *wlmaker_subprocess_from_subprocess_handle(
    wlmaker_subprocess_handle_t *subprocess_handle_ptr)
{
    return subprocess_handle_ptr->subprocess_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates a @ref wlmaker_subprocess_handle_t and connects to subprocess_ptr.
 *
 * @param subprocess_ptr
 * @param wl_event_loop_ptr
 *
 * @return The subprocess handle or NULL on error.
 */
wlmaker_subprocess_handle_t *wlmaker_subprocess_handle_create(
    bs_subprocess_t *subprocess_ptr,
    struct wl_event_loop *wl_event_loop_ptr)
{
    wlmaker_subprocess_handle_t *subprocess_handle_ptr = logged_calloc(
        1, sizeof(wlmaker_subprocess_handle_t));
    if (NULL == subprocess_handle_ptr) return NULL;

    subprocess_handle_ptr->subprocess_ptr = subprocess_ptr;

    bs_subprocess_get_fds(
        subprocess_ptr,
        NULL,  // no interest in stdin.
        &subprocess_handle_ptr->stdout_read_fd,
        &subprocess_handle_ptr->stderr_read_fd);

    subprocess_handle_ptr->stdout_wl_event_source_ptr = wl_event_loop_add_fd(
        wl_event_loop_ptr,
        subprocess_handle_ptr->stdout_read_fd,
        WL_EVENT_READABLE,
        _wlmaker_subprocess_monitor_handle_read_stdout,
        subprocess_handle_ptr);
    subprocess_handle_ptr->stderr_wl_event_source_ptr = wl_event_loop_add_fd(
        wl_event_loop_ptr,
        subprocess_handle_ptr->stderr_read_fd,
        WL_EVENT_READABLE,
        _wlmaker_subprocess_monitor_handle_read_stderr,
        subprocess_handle_ptr);

    return subprocess_handle_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the subprocess handle and frees up associated resources.
 *
 * @param sp_handle_ptr
 */
void wlmaker_subprocess_handle_destroy(
    wlmaker_subprocess_handle_t *sp_handle_ptr)
{
    BS_ASSERT(NULL == sp_handle_ptr->dlnode.prev_ptr);
    int exit_status, signal_number;
    if (!bs_subprocess_terminated(
            sp_handle_ptr->subprocess_ptr, &exit_status, &signal_number)) {
        bs_log(BS_FATAL, "Destroying subprocess handle, but still running: "
               "subprocess %p (pid: %"PRIdMAX")",
               sp_handle_ptr->subprocess_ptr,
               (intmax_t)bs_subprocess_pid(sp_handle_ptr->subprocess_ptr));
    }
    bs_log(BS_DEBUG, "Terminated subprocess %p. Status %d, signal %d.",
           sp_handle_ptr->subprocess_ptr, exit_status, signal_number);

    if (NULL != sp_handle_ptr->terminated_callback) {
        // Attempt to drain stdout & stderr before closing the pipes.
        _wlmaker_subprocess_monitor_handle_read_stdout(
            sp_handle_ptr->stdout_read_fd,
            WL_EVENT_READABLE,
            sp_handle_ptr);
        _wlmaker_subprocess_monitor_handle_read_stderr(
            sp_handle_ptr->stderr_read_fd,
            WL_EVENT_READABLE,
            sp_handle_ptr);

        sp_handle_ptr->terminated_callback(
            sp_handle_ptr->userdata_ptr,
            sp_handle_ptr,
            exit_status,
            signal_number);
        sp_handle_ptr->terminated_callback = NULL;
    }

    if (NULL != sp_handle_ptr->subprocess_ptr) {
        bs_subprocess_destroy(sp_handle_ptr->subprocess_ptr);
        sp_handle_ptr->subprocess_ptr = NULL;
    }

    if (NULL != sp_handle_ptr->stdout_wl_event_source_ptr) {
        wl_event_source_remove(sp_handle_ptr->stdout_wl_event_source_ptr);
        sp_handle_ptr->stdout_wl_event_source_ptr = NULL;
    }
    if (NULL != sp_handle_ptr->stderr_wl_event_source_ptr) {
        wl_event_source_remove(sp_handle_ptr->stderr_wl_event_source_ptr);
        sp_handle_ptr->stderr_wl_event_source_ptr = NULL;
    }

    free(sp_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for activity on stdout file descriptor, as prescribed by
 * wl_event_loop_fd_func_t.
 *
 * @param fd
 * @param mask                A bitmask of WL_EVENT_READABLE, WL_EVENT_HANGUP
 *                            or WL_EVENT_ERROR.
 * @param data_ptr            Points ot a @ref wlmaker_subprocess_handle_t.
 *
 * @return 0.
 */
int _wlmaker_subprocess_monitor_handle_read_stdout(
    int fd, uint32_t mask, void *data_ptr)
{
    char buf[1024];
    bs_dynbuf_t dynbuf, *dynbuf_ptr;

    wlmaker_subprocess_handle_t *subprocess_handle_ptr = data_ptr;
    BS_ASSERT(fd == subprocess_handle_ptr->stdout_read_fd);

    dynbuf_ptr = subprocess_handle_ptr->stdout_dynbuf_ptr;
    if (NULL == dynbuf_ptr) {
        bs_dynbuf_init_unmanaged(&dynbuf, buf, sizeof(buf) - 1);
        dynbuf_ptr = &dynbuf;
    }

    int rv = _wlmaker_subprocess_monitor_process_fd(
        subprocess_handle_ptr,
        &subprocess_handle_ptr->stdout_wl_event_source_ptr,
        subprocess_handle_ptr->stdout_read_fd,
        mask,
        "stdout",
        dynbuf_ptr);
    // Log subprocess stdout, but only if not using an explicit stdout dynbuf.
    if (dynbuf_ptr != subprocess_handle_ptr->stdout_dynbuf_ptr &&
        0 < dynbuf_ptr->length) {
        buf[BS_MIN(sizeof(buf) - 1, dynbuf.length)] = '\0';
        bs_log(BS_INFO, "subprocess %"PRIdMAX" stdout: %s",
               (intmax_t)bs_subprocess_pid(subprocess_handle_ptr->subprocess_ptr),
               buf);
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for activity on stderr file descriptor, as prescribed by
 * wl_event_loop_fd_func_t.
 *
 * @param fd
 * @param mask                A bitmask of WL_EVENT_READABLE, WL_EVENT_HANGUP
 *                            or WL_EVENT_ERROR.
 * @param data_ptr            Points to a @ref wlmaker_subprocess_handle_t.
 *
 * @return 0.
 */
int _wlmaker_subprocess_monitor_handle_read_stderr(
    int fd, uint32_t mask, void *data_ptr)
{
    char buf[1024];
    bs_dynbuf_t dynbuf;

    wlmaker_subprocess_handle_t *subprocess_handle_ptr = data_ptr;
    BS_ASSERT(fd == subprocess_handle_ptr->stderr_read_fd);

    bs_dynbuf_init_unmanaged(&dynbuf, buf, sizeof(buf));
    int rv = _wlmaker_subprocess_monitor_process_fd(
        subprocess_handle_ptr,
        &subprocess_handle_ptr->stdout_wl_event_source_ptr,
        subprocess_handle_ptr->stdout_read_fd,
        mask,
        "stdout",
        &dynbuf);
    if (0 < dynbuf.length) {
        buf[BS_MIN(sizeof(buf) - 1, dynbuf.length)] = '\0';
        bs_log(BS_WARNING, "subprocess %"PRIdMAX" stderr: %s",
               (intmax_t)bs_subprocess_pid(subprocess_handle_ptr->subprocess_ptr),
               buf);
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
/**
 * Processes activity on a file descriptor, matches wl_event_loop_fd_func_t.
 *
 * @param subprocess_handle_ptr
 * @param wl_event_source_ptr_ptr
 * @param fd
 * @param mask
 * @param fd_name_ptr
 * @param dynbuf_ptr
 *
 * @return 0.
 */
int _wlmaker_subprocess_monitor_process_fd(
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    struct wl_event_source **wl_event_source_ptr_ptr,
    int fd,
    uint32_t mask,
    const char *fd_name_ptr,
    bs_dynbuf_t *dynbuf_ptr)
{
    // Convenience copy.
    intmax_t pid = bs_subprocess_pid(subprocess_handle_ptr->subprocess_ptr);

    if (mask & WL_EVENT_READABLE) {
        bs_dynbuf_read(dynbuf_ptr, fd);
        return 0;
    }

    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR) &&
        NULL != *wl_event_source_ptr_ptr) {
        bs_log(BS_DEBUG, "subprocess %"PRIdMAX" %s: Mask 0x%x, removing.",
               pid, fd_name_ptr, mask);
        wl_event_source_remove(*wl_event_source_ptr_ptr);
        *wl_event_source_ptr_ptr = NULL;
        return 0;
    }

    bs_log(BS_WARNING, "subprocess %"PRIdMAX" %s: Unexpected event, mask 0x%x",
           pid, fd_name_ptr, mask);
    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles SIGCHLD. Callback for Wayland event loop.
 *
 * @param signum
 *
 * @param data_ptr            Points to @ref wlmaker_subprocess_monitor_t.
 */
int _wlmaker_subprocess_monitor_handle_sigchld(
    __UNUSED__ int signum, void *data_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = data_ptr;

    bs_dllist_node_t *dlnode_ptr = monitor_ptr->subprocesses.head_ptr;
    while (NULL != dlnode_ptr) {
        wlmaker_subprocess_handle_t *subprocess_handle_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmaker_subprocess_handle_t, dlnode);
        dlnode_ptr = dlnode_ptr->next_ptr;

        int exit_status, signal_number;
        if (bs_subprocess_terminated(subprocess_handle_ptr->subprocess_ptr,
                                     &exit_status, &signal_number)) {
            bs_dllist_remove(
                &monitor_ptr->subprocesses,
                &subprocess_handle_ptr->dlnode);
            wlmaker_subprocess_handle_destroy(subprocess_handle_ptr);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles window creation: Will see if there's a subprocess mapping to the
 * corresponding client's PID, and call the "created" callback, if registered.
 *
 * Note: A client may have an arbitrary number of windows created.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a @ref wlmaker_subprocess_monitor_t.
 */
void _wlmaker_subprocess_monitor_handle_window_created(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_subprocess_monitor_t, window_created_listener);
    wlmtk_window_t *window_ptr = data_ptr;

    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        subprocess_handle_from_window(monitor_ptr, window_ptr);
    if (NULL == subprocess_handle_ptr) return;

    wlmaker_subprocess_window_t *ws_window_ptr =
        wlmaker_subprocess_window_create(window_ptr, subprocess_handle_ptr);
    if (NULL == ws_window_ptr) return;

    bs_avltree_insert(
        monitor_ptr->window_tree_ptr,
        ws_window_ptr->window_ptr,
        &ws_window_ptr->avlnode,
        true);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles window mapping: Will see if there's a window and corresponding
 * subprocess, and calls the "mapped" callback, if registered.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a @ref wlmaker_subprocess_monitor_t.
 */
void _wlmaker_subprocess_monitor_handle_window_mapped(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_subprocess_monitor_t, window_mapped_listener);
    wlmtk_window_t *window_ptr = data_ptr;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        monitor_ptr->window_tree_ptr, window_ptr);
    if (NULL == avlnode_ptr) return;

    wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_subprocess_window_t, avlnode);
    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        ws_window_ptr->subprocess_handle_ptr;
    if (NULL == subprocess_handle_ptr) return;

    if (NULL != subprocess_handle_ptr->window_mapped_callback) {
        subprocess_handle_ptr->window_mapped_callback(
            subprocess_handle_ptr->userdata_ptr,
            subprocess_handle_ptr,
            ws_window_ptr->window_ptr);
    }
    ws_window_ptr->mapped = true;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles window unmapping: Will see if there's a window and corresponding
 * subprocess, and calls the "unmapped" callback, if registered.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a @ref wlmaker_subprocess_monitor_t.
 */
void _wlmaker_subprocess_monitor_handle_window_unmapped(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_subprocess_monitor_t, window_unmapped_listener);
    wlmtk_window_t *window_ptr = data_ptr;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_lookup(
        monitor_ptr->window_tree_ptr, window_ptr);
    if (NULL == avlnode_ptr) return;

    wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_subprocess_window_t, avlnode);
    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        ws_window_ptr->subprocess_handle_ptr;
    if (NULL == subprocess_handle_ptr) return;

    if (NULL != subprocess_handle_ptr->window_unmapped_callback) {
        subprocess_handle_ptr->window_unmapped_callback(
            subprocess_handle_ptr->userdata_ptr,
            subprocess_handle_ptr,
            ws_window_ptr->window_ptr);
    }
    ws_window_ptr->mapped = false;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles window destruction: Will retrieve the wlmaker_subprocess_window_t
 * structure for tracking windows for subprocesses, call the respective
 * callbacks and destroy the associated window tracking structure.
 *
 * @param listener_ptr
 * @param data_ptr            Points to a @ref wlmaker_subprocess_monitor_t.
 */
void _wlmaker_subprocess_monitor_handle_window_destroyed(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_subprocess_monitor_t *monitor_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_subprocess_monitor_t, window_destroyed_listener);
    wlmtk_window_t *window_ptr = data_ptr;

    bs_avltree_node_t *avlnode_ptr = bs_avltree_delete(
        monitor_ptr->window_tree_ptr, window_ptr);
    if (NULL == avlnode_ptr) return;

    wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
        avlnode_ptr, wlmaker_subprocess_window_t, avlnode);
    wlmaker_subprocess_window_destroy(ws_window_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the subprocess matching the window's client, if any.
 *
 * Practically, there should only ever be one subprocess matching, since the
 * PID of a subprocess is supposed to be unique.
 *
 * @param monitor_ptr
 * @param window_ptr
 *
 * @return A pointer to the subprocess handle corresponding to the window's
 *     client, or NULL if not found.
 */
wlmaker_subprocess_handle_t *subprocess_handle_from_window(
    wlmaker_subprocess_monitor_t *monitor_ptr,
    wlmtk_window_t *window_ptr)
{
    const wlmtk_util_client_t *client_ptr = wlmtk_window_get_client_ptr(
        window_ptr);
    // TODO(kaeser@gubbe.ch): Should be a O(1) or O(log(n)) structure.
    for (bs_dllist_node_t *dlnode_ptr = monitor_ptr->subprocesses.head_ptr;
         NULL != dlnode_ptr;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmaker_subprocess_handle_t *subprocess_handle_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmaker_subprocess_handle_t, dlnode);
        if (client_ptr->pid ==
            bs_subprocess_pid(subprocess_handle_ptr->subprocess_ptr)) {
            return subprocess_handle_ptr;
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a structure to track windows for subprocesses.
 *
 * Also calls the `window_created_callback`, if given.
 *
 * @param window_ptr
 * @param subprocess_handle_ptr
 *
 * @return A pointer to @ref wlmaker_subprocess_window_t or NULL on error.
 */
wlmaker_subprocess_window_t *wlmaker_subprocess_window_create(
    wlmtk_window_t *window_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr)
{
    // Guard clause: No need for window handle, if no window nor process.
    if (NULL == window_ptr || NULL == subprocess_handle_ptr) return NULL;

    wlmaker_subprocess_window_t *ws_window_ptr = logged_calloc(
        1, sizeof(wlmaker_subprocess_window_t));
    if (NULL == ws_window_ptr) return NULL;
    ws_window_ptr->window_ptr = window_ptr;
    ws_window_ptr->subprocess_handle_ptr = subprocess_handle_ptr;

    if (NULL != subprocess_handle_ptr->window_created_callback) {
        subprocess_handle_ptr->window_created_callback(
            subprocess_handle_ptr->userdata_ptr,
            subprocess_handle_ptr,
            ws_window_ptr->window_ptr);
    }

    bs_dllist_push_back(&subprocess_handle_ptr->windows,
                        &ws_window_ptr->dlnode);
    return ws_window_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the structure for tracking windows for subprocesses.
 *
 * Calls the `window_destroyed_callback`, if given.
 *
 * @param ws_window_ptr
 */
void wlmaker_subprocess_window_destroy(
    wlmaker_subprocess_window_t *ws_window_ptr)
{
    wlmaker_subprocess_handle_t *subprocess_handle_ptr =
        ws_window_ptr->subprocess_handle_ptr;
    if (ws_window_ptr->mapped &&
        NULL != subprocess_handle_ptr &&
        NULL != subprocess_handle_ptr->window_unmapped_callback) {
        subprocess_handle_ptr->window_unmapped_callback(
            subprocess_handle_ptr->userdata_ptr,
            subprocess_handle_ptr,
            ws_window_ptr->window_ptr);
        ws_window_ptr->mapped = false;
    }

    if (NULL != subprocess_handle_ptr &&
        NULL != subprocess_handle_ptr->window_destroyed_callback) {
        subprocess_handle_ptr->window_destroyed_callback(
            subprocess_handle_ptr->userdata_ptr,
            subprocess_handle_ptr,
            ws_window_ptr->window_ptr);
    }

    if (NULL != ws_window_ptr->subprocess_handle_ptr) {
        bs_dllist_remove(
            &ws_window_ptr->subprocess_handle_ptr->windows,
            &ws_window_ptr->dlnode);
        ws_window_ptr->subprocess_handle_ptr = NULL;
    }
    free(ws_window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Comparator for window registry tree nodes. */
int wlmaker_subprocess_window_node_cmp(const bs_avltree_node_t *node_ptr,
                                       const void *key_ptr)
{
    wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
        node_ptr, wlmaker_subprocess_window_t, avlnode);
    return bs_avltree_cmp_ptr(ws_window_ptr->window_ptr, key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Destructor for window registry tree nodes. */
void wlmaker_subprocess_window_node_destroy(bs_avltree_node_t *node_ptr)
{
    wlmaker_subprocess_window_t *ws_window_ptr = BS_CONTAINER_OF(
        node_ptr, wlmaker_subprocess_window_t, avlnode);
    wlmaker_subprocess_window_destroy(ws_window_ptr);
}

/* == End of subprocess_monitor.c ========================================== */
