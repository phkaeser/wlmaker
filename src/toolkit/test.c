/* ========================================================================= */
/**
 * @file test.c
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
 */

#include "test.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#undef WLR_USE_UNSTABLE

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmtk_test_wlr_output_init(struct wlr_output *wlr_output_ptr)
{
    wlr_addon_set_init(&wlr_output_ptr->addons);
    wl_list_init(&wlr_output_ptr->display_destroy.link);
    wl_list_init(&wlr_output_ptr->resources);
    wl_signal_init(&wlr_output_ptr->events.commit);
    wl_signal_init(&wlr_output_ptr->events.damage);
    wl_signal_init(&wlr_output_ptr->events.needs_frame);
    wlr_output_ptr->frame_pending = true;
}

/* == End of test.c ======================================================== */
