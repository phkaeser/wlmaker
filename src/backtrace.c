/* ========================================================================= */
/**
 * @file backtrace.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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
 * Copyright (c) 2025 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "backtrace.h"

#include <inttypes.h>
#include <libbase/libbase.h>
#include <signal.h>
#include <stdlib.h>

#if defined(WLMAKER_HAVE_LIBBACKTRACE)
#include <backtrace.h>
#endif // defined(WLMAKER_HAVE_LIBBACKTRACE)

/* == Declarations ========================================================= */

#if defined(WLMAKER_HAVE_LIBBACKTRACE)

/** State for libbacktrace. */
static struct backtrace_state *_wlmaker_bt_state_ptr;

static void _backtrace_error_callback(
    __UNUSED__ void *data_ptr,
    const char *msg_ptr,
    int errnum);
static int _backtrace_full_callback(
    __UNUSED__ void *data,
    uintptr_t pc,
    const char *filename_ptr,
    int line_num,
    const char *function_ptr);
static void _signal_backtrace(int signum);

#endif  // defined(WLMAKER_HAVE_LIBBACKTRACE)

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmaker_backtrace_setup(const char *filename_ptr)
{
#if defined(WLMAKER_HAVE_LIBBACKTRACE)

    _wlmaker_bt_state_ptr = backtrace_create_state(
        filename_ptr, 0, _backtrace_error_callback, NULL);
    if (NULL == _wlmaker_bt_state_ptr) {
        bs_log(BS_ERROR, "Failed backtrace_create_state()");
        return false;
    }
    signal(SIGABRT, _signal_backtrace);
    signal(SIGBUS, _signal_backtrace);
    signal(SIGFPE, _signal_backtrace);
    signal(SIGILL, _signal_backtrace);
    signal(SIGSEGV, _signal_backtrace);

#else

    bs_log(BS_DEBUG, "No libbacktrace, ignoring setup for %s", filename_ptr);

#endif  // defined(WLMAKER_HAVE_LIBBACKTRACE)

    return true;
}

/* == Local (static) methods =============================================== */

#if defined(WLMAKER_HAVE_LIBBACKTRACE)

/* ------------------------------------------------------------------------- */
/** Error callback for libbacktrace calls. */
void _backtrace_error_callback(
    __UNUSED__ void *data_ptr,
    const char *msg_ptr,
    int errnum)
{
    bs_log_severity_t severity = BS_ERROR;
    if (0 != errnum && -1 != errnum) severity |= BS_ERRNO;
    bs_log(severity, "Backtrace error: %s", msg_ptr);
}

/* ------------------------------------------------------------------------- */
/** Full callback for printing a libbacktrace frame. */
int _backtrace_full_callback(
    __UNUSED__ void *data,
    uintptr_t program_counter,
    const char *filename_ptr,
    int line_num,
    const char *function_ptr)
{
    bs_log(BS_ERROR, "%"PRIxPTR" in %s () at %s:%d",
           program_counter,
           function_ptr ? function_ptr : "(unknown)",
           filename_ptr ? filename_ptr : "(unknown)",
           line_num);
    return 0;
}

/* ------------------------------------------------------------------------- */
/** Signal handler: Prints a backtrace. */
void _signal_backtrace(int signum)
{
    bs_log(BS_ERROR, "Caught signal %d", signum);
    backtrace_full(
        _wlmaker_bt_state_ptr, 0,
        _backtrace_full_callback, _backtrace_error_callback, NULL);

    signal(SIGABRT, SIG_DFL);
    abort();
}

#endif // defined(WLMAKER_HAVE_LIBBACKTRACE)

/* == End of backtrace.c =================================================== */
