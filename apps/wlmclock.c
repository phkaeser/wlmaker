/* ========================================================================= */
/**
 * @file wlmclock.c
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

#include "wlclient.h"

#include <libbase/libbase.h>

/* == Main program ========================================================= */
/** Main progrm. */
int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    bs_log_severity = BS_DEBUG;

    wlclient_t *wlclient_ptr = wlclient_create();
    if (NULL == wlclient_ptr) return EXIT_FAILURE;

    wlclient_destroy(wlclient_ptr);
    return EXIT_SUCCESS;
}
/* == End of wlmclock.c ==================================================== */
