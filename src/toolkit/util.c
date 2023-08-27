/* ========================================================================= */
/**
 * @file util.c
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

#include "util.h"

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmtk_util_connect_listener_signal(
    struct wl_signal *signal_ptr,
    struct wl_listener *listener_ptr,
    void (*notifier_func)(struct wl_listener *, void *))
{
    listener_ptr->notify = notifier_func;
    wl_signal_add(signal_ptr, listener_ptr);
}

/* == End of util.c ======================================================== */
