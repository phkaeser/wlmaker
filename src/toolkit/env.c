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

#include <libbase/libbase.h>
#include <toolkit/env.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the environment. */
struct _wlmtk_env_t {
    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;
    /** Points to a `wlr_seat`. */
    struct wlr_seat           *wlr_seat_ptr;
};

/** Struct to identify a @ref wlmtk_env_cursor_t with the xcursor name. */
typedef struct {
    /** The cursor. */
    wlmtk_env_cursor_t        cursor;
    /** And the xcursor name. */
    const char                *xcursor_name_ptr;
} wlmtk_env_cursor_lookup_t;

/** Lookup table for xcursor names. */
static const wlmtk_env_cursor_lookup_t _wlmtk_env_cursor_lookup[] = {
    { WLMTK_CURSOR_DEFAULT, "default" },
    { WLMTK_CURSOR_RESIZE_S, "s-resize" },
    { WLMTK_CURSOR_RESIZE_SE, "se-resize" },
    { WLMTK_CURSOR_RESIZE_SW, "sw-resize" },
    { 0, NULL },
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_env_t *wlmtk_env_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr,
    struct wlr_seat *wlr_seat_ptr)
{
    wlmtk_env_t *env_ptr = logged_calloc(1, sizeof(wlmtk_env_t));
    if (NULL == env_ptr) return NULL;

    env_ptr->wlr_cursor_ptr = wlr_cursor_ptr;
    env_ptr->wlr_xcursor_manager_ptr = wlr_xcursor_manager_ptr;
    env_ptr->wlr_seat_ptr = wlr_seat_ptr;

    return env_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_env_destroy(wlmtk_env_t *env_ptr)
{
    free(env_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_env_set_cursor(wlmtk_env_t *env_ptr, wlmtk_env_cursor_t cursor)
{
    const wlmtk_env_cursor_lookup_t *lookup_ptr = &_wlmtk_env_cursor_lookup[0];
    for (;
         NULL != lookup_ptr->xcursor_name_ptr && cursor != lookup_ptr->cursor;
         ++lookup_ptr) ;
    if (NULL == lookup_ptr->xcursor_name_ptr) {
        bs_log(BS_FATAL, "No name for cursor %d", cursor);
        return;
    }

    if (NULL != env_ptr &&
        NULL != env_ptr->wlr_cursor_ptr &&
        NULL != env_ptr->wlr_xcursor_manager_ptr) {
        wlr_cursor_set_xcursor(
            env_ptr->wlr_cursor_ptr,
            env_ptr->wlr_xcursor_manager_ptr,
            lookup_ptr->xcursor_name_ptr);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_seat *wlmtk_env_wlr_seat(wlmtk_env_t *env_ptr)
{
    return env_ptr->wlr_seat_ptr;
}

/* == End of env.c ========================================================= */
