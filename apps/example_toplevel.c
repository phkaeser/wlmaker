/* ========================================================================= */
/**
 * @file wlmclock.c
 *
 * Example app for libwlclient.
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

#include <libbase/libbase.h>
#include <libwlclient/libwlclient.h>

#include <math.h>
#include <sys/time.h>

/* == Main program ========================================================= */
/** Main program. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_DEBUG;

    wlclient_t *wlclient_ptr = wlclient_create("example_toplevel");
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    if (wlclient_xdg_supported(wlclient_ptr)) {
        wlclient_xdg_toplevel_t *toplevel_ptr = wlclient_xdg_toplevel_create(
            wlclient_ptr);
        if (NULL != toplevel_ptr) {
            wlclient_run(wlclient_ptr);
            wlclient_xdg_toplevel_destroy(toplevel_ptr);
        } else {
            bs_log(BS_ERROR, "Failed wlclient_xdg_toplevel_create(%p)",
                   wlclient_ptr);
        }
    } else {
        bs_log(BS_ERROR, "XDG shell is not supported.");
    }

    wlclient_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of wlmclock.c ==================================================== */
