/* ========================================================================= */
/**
 * @file env.c
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

#include "env.h"

#include <libbase/libbase.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE
#include <stdlib.h>

/* == Declarations ========================================================= */

/** State of the environment. */
struct _wlmtk_env_t {
    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_env_t *wlmtk_env_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr)
{
    wlmtk_env_t *env_ptr = logged_calloc(1, sizeof(wlmtk_env_t));
    if (NULL == env_ptr) return NULL;

    env_ptr->wlr_cursor_ptr = wlr_cursor_ptr;
    env_ptr->wlr_xcursor_manager_ptr = wlr_xcursor_manager_ptr;

    return env_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_env_destroy(wlmtk_env_t *env_ptr)
{
    free(env_ptr);
}

/* == End of env.c ========================================================= */
