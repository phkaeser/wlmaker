/* ========================================================================= */
/**
 * @file subprocess_monitor.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

struct wl_event_source;

/* == Declarations ========================================================= */

/** State of the subprocess monitor. */
struct _wlm_util_subprocess_monitor_t {
    /** Reference to the event loop. */
    struct wl_event_loop      *wl_event_loop_ptr;
    /** Event source used for monitoring SIGCHLD. */
    struct wl_event_source    *sigchld_event_source_ptr;

    /** Monitored subprocesses. */
    bs_dllist_t               subprocesses;
};

/** A subprocess. */
struct wlm_util_subprocess {
    /** Element of @ref wlm_util_subprocess_monitor_t `subprocesses`. */
    bs_dllist_node_t          dlnode;
    /** Points to the libbase subprocess. */
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
    wlm_util_subprocess_terminated_callback_t terminated_callback;
    /** Argument to all the callbacks. */
    void                      *userdata_ptr;

    /** Dynamic buffer holding the process' stdout, or NULL if not set. */
    bs_dynbuf_t               *stdout_dynbuf_ptr;

};

static struct wlm_util_subprocess *wlm_util_subprocess_handle_create(
    bs_subprocess_t *subprocess_ptr,
    struct wl_event_loop *wl_event_loop_ptr);
static void wlm_util_subprocess_handle_destroy(
    struct wlm_util_subprocess *sp_handle_ptr);
static int _wlm_util_subprocess_monitor_handle_read_stdout(
    int fd, uint32_t mask, void *data_ptr);
static int _wlm_util_subprocess_monitor_handle_read_stderr(
    int fd, uint32_t mask, void *data_ptr);

static int _wlm_util_subprocess_monitor_process_fd(
    struct wlm_util_subprocess *subprocess_handle_ptr,
    struct wl_event_source **wl_event_source_ptr_ptr,
    int fd,
    uint32_t mask,
    const char *fd_name_ptr,
    bs_dynbuf_t *dynbuf_ptr);

static int _wlm_util_subprocess_monitor_handle_sigchld(
    int signum,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlm_util_subprocess_monitor_t* wlm_util_subprocess_monitor_create(
    struct wl_event_loop *wl_event_loop_ptr)
{
    if (NULL == wl_event_loop_ptr) return NULL;

    wlm_util_subprocess_monitor_t *monitor_ptr = logged_calloc(
        1, sizeof(*monitor_ptr));
    if (NULL == monitor_ptr) return NULL;
    monitor_ptr->wl_event_loop_ptr = wl_event_loop_ptr;

    monitor_ptr->sigchld_event_source_ptr = wl_event_loop_add_signal(
        monitor_ptr->wl_event_loop_ptr,
        SIGCHLD,
        _wlm_util_subprocess_monitor_handle_sigchld,
        monitor_ptr);

    return monitor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlm_util_subprocess_monitor_destroy(
    wlm_util_subprocess_monitor_t *monitor_ptr)
{
    if (NULL != monitor_ptr->sigchld_event_source_ptr) {
        wl_event_source_remove(monitor_ptr->sigchld_event_source_ptr);
        monitor_ptr->sigchld_event_source_ptr = NULL;
    }

    monitor_ptr->wl_event_loop_ptr = NULL;
    free(monitor_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlm_util_subprocess *wlm_util_subprocess_monitor_entrust(
    wlm_util_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr,
    wlm_util_subprocess_terminated_callback_t terminated_callback,
    void *userdata_ptr,
    bs_dynbuf_t *stdout_dynbuf_ptr)
{
    struct wlm_util_subprocess *subprocess_handle_ptr =
        wlm_util_subprocess_handle_create(
            subprocess_ptr, monitor_ptr->wl_event_loop_ptr);
    if (NULL == subprocess_handle_ptr) return NULL;
    bs_dllist_push_back(&monitor_ptr->subprocesses,
                        &subprocess_handle_ptr->dlnode);

    subprocess_handle_ptr->terminated_callback = terminated_callback;
    subprocess_handle_ptr->userdata_ptr = userdata_ptr;
    subprocess_handle_ptr->stdout_dynbuf_ptr = stdout_dynbuf_ptr;

    return subprocess_handle_ptr;
}

/* ------------------------------------------------------------------------- */
void wlm_util_subprocess_monitor_cede(
    __UNUSED__ wlm_util_subprocess_monitor_t *monitor_ptr,
    struct wlm_util_subprocess *subprocess_handle_ptr)
{
    subprocess_handle_ptr->terminated_callback = NULL;
}

/* ------------------------------------------------------------------------- */
bool wlm_util_subprocess_monitor_run(
    wlm_util_subprocess_monitor_t *monitor_ptr,
    bs_subprocess_t *subprocess_ptr)
{
    if (NULL == subprocess_ptr) return false;
    if (!bs_subprocess_start(subprocess_ptr)) goto error;

    struct wlm_util_subprocess *subprocess_handle_ptr =
        wlm_util_subprocess_monitor_entrust(
            monitor_ptr,
            subprocess_ptr,
            NULL,
            NULL,
            NULL);
    if (NULL == subprocess_handle_ptr) goto error;

    wlm_util_subprocess_monitor_cede(monitor_ptr, subprocess_handle_ptr);
    return true;

error:
    if (NULL != subprocess_ptr) bs_subprocess_destroy(subprocess_ptr);
    return NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Creates a @ref struct wlm_util_subprocess and connects to subprocess_ptr.
 *
 * @param subprocess_ptr
 * @param wl_event_loop_ptr
 *
 * @return The subprocess handle or NULL on error.
 */
struct wlm_util_subprocess *wlm_util_subprocess_handle_create(
    bs_subprocess_t *subprocess_ptr,
    struct wl_event_loop *wl_event_loop_ptr)
{
    struct wlm_util_subprocess *subprocess_handle_ptr = logged_calloc(
        1, sizeof(struct wlm_util_subprocess));
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
        _wlm_util_subprocess_monitor_handle_read_stdout,
        subprocess_handle_ptr);
    subprocess_handle_ptr->stderr_wl_event_source_ptr = wl_event_loop_add_fd(
        wl_event_loop_ptr,
        subprocess_handle_ptr->stderr_read_fd,
        WL_EVENT_READABLE,
        _wlm_util_subprocess_monitor_handle_read_stderr,
        subprocess_handle_ptr);

    return subprocess_handle_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the subprocess handle and frees up associated resources.
 *
 * @param sp_handle_ptr
 */
void wlm_util_subprocess_handle_destroy(
    struct wlm_util_subprocess *sp_handle_ptr)
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
        _wlm_util_subprocess_monitor_handle_read_stdout(
            sp_handle_ptr->stdout_read_fd,
            WL_EVENT_READABLE,
            sp_handle_ptr);
        _wlm_util_subprocess_monitor_handle_read_stderr(
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
 * @param data_ptr            Points ot a @ref struct wlm_util_subprocess.
 *
 * @return 0.
 */
int _wlm_util_subprocess_monitor_handle_read_stdout(
    int fd, uint32_t mask, void *data_ptr)
{
    char buf[1024];
    bs_dynbuf_t dynbuf, *dynbuf_ptr;

    struct wlm_util_subprocess *subprocess_handle_ptr = data_ptr;
    BS_ASSERT(fd == subprocess_handle_ptr->stdout_read_fd);

    dynbuf_ptr = subprocess_handle_ptr->stdout_dynbuf_ptr;
    if (NULL == dynbuf_ptr) {
        bs_dynbuf_init_unmanaged(&dynbuf, buf, sizeof(buf) - 1);
        dynbuf_ptr = &dynbuf;
    }

    int rv = _wlm_util_subprocess_monitor_process_fd(
        subprocess_handle_ptr,
        &subprocess_handle_ptr->stdout_wl_event_source_ptr,
        subprocess_handle_ptr->stdout_read_fd,
        mask,
        "stdout",
        dynbuf_ptr);
    // Log subprocess stdout, but only if not using an explicit stdout dynbuf.
    if (dynbuf_ptr != subprocess_handle_ptr->stdout_dynbuf_ptr &&
        0 < dynbuf_ptr->length) {
        size_t len = BS_MIN(sizeof(buf) - 1, dynbuf.length);
        if (0 < len && buf[len - 1] == '\n') {
            buf[--len] = '\0';
        } else {
            buf[len] = '\0';
        }
        if (0 < len) {
            bs_log_write(
                BS_DEBUG, "subprocess.stdout",
                (intmax_t)bs_subprocess_pid(
                    subprocess_handle_ptr->subprocess_ptr),
                "%s", buf);
        }
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
 * @param data_ptr            Points to a @ref struct wlm_util_subprocess.
 *
 * @return 0.
 */
int _wlm_util_subprocess_monitor_handle_read_stderr(
    int fd, uint32_t mask, void *data_ptr)
{
    char buf[1024];
    bs_dynbuf_t dynbuf;

    struct wlm_util_subprocess *subprocess_handle_ptr = data_ptr;
    BS_ASSERT(fd == subprocess_handle_ptr->stderr_read_fd);

    bs_dynbuf_init_unmanaged(&dynbuf, buf, sizeof(buf));
    int rv = _wlm_util_subprocess_monitor_process_fd(
        subprocess_handle_ptr,
        &subprocess_handle_ptr->stderr_wl_event_source_ptr,
        subprocess_handle_ptr->stderr_read_fd,
        mask,
        "stdout",
        &dynbuf);
    if (0 < dynbuf.length) {
        size_t len = BS_MIN(sizeof(buf) - 1, dynbuf.length);
        if (0 < len && buf[len - 1] == '\n') {
            buf[--len] = '\0';
        } else {
            buf[len] = '\0';
        }
        if (0 < len) {
            bs_log_write(
                BS_INFO, "subprocess.stderr",
                (intmax_t)bs_subprocess_pid(
                    subprocess_handle_ptr->subprocess_ptr),
                "%s", buf);
        }
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
int _wlm_util_subprocess_monitor_process_fd(
    struct wlm_util_subprocess *subprocess_handle_ptr,
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
 * @param data_ptr            Points to @ref wlm_util_subprocess_monitor_t.
 */
int _wlm_util_subprocess_monitor_handle_sigchld(
    __UNUSED__ int signum, void *data_ptr)
{
    wlm_util_subprocess_monitor_t *monitor_ptr = data_ptr;

    bs_dllist_node_t *dlnode_ptr = monitor_ptr->subprocesses.head_ptr;
    while (NULL != dlnode_ptr) {
        struct wlm_util_subprocess *subprocess_handle_ptr = BS_CONTAINER_OF(
            dlnode_ptr, struct wlm_util_subprocess, dlnode);
        dlnode_ptr = dlnode_ptr->next_ptr;

        int exit_status, signal_number;
        if (bs_subprocess_terminated(subprocess_handle_ptr->subprocess_ptr,
                                     &exit_status, &signal_number)) {
            bs_dllist_remove(
                &monitor_ptr->subprocesses,
                &subprocess_handle_ptr->dlnode);
            wlm_util_subprocess_handle_destroy(subprocess_handle_ptr);
        }
    }

    return 0;
}

/* == End of subprocess_monitor.c ========================================== */
