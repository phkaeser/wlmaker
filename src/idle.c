/* ========================================================================= */
/**
 * @file idle.c
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

#include "idle.h"

/* == Declarations ========================================================= */

/** State of the idle monitor. */
struct _wlmaker_idle_monitor_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_idle_monitor_t *wlmaker_idle_monitor_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_idle_monitor_t *idle_monitor_ptr = logged_calloc(
        1, sizeof(wlmaker_idle_monitor_t));
    if (NULL == idle_monitor_ptr) return NULL;
    idle_monitor_ptr->server_ptr = server_ptr;

    return idle_monitor_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_idle_monitor_destroy(wlmaker_idle_monitor_t *idle_monitor_ptr)
{
    free(idle_monitor_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of idle.c ======================================================== */
