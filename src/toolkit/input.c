/* ========================================================================= */
/**
 * @file input.c
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

#include "input.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Pointer handle. Wraps to wlr_cursor. */
struct _wlmtk_pointer_t {
    /** Points to a `wlr_cursor`. */
    struct wlr_cursor         *wlr_cursor_ptr;
    /** Points to a `wlr_xcursor_manager`. */
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr;
};

/** Lookup table for XCursor names. */
static const char* _wlmtk_pointer_cursor_names[] = {
    [WLMTK_POINTER_CURSOR_DEFAULT] = "default",
    [WLMTK_POINTER_CURSOR_RESIZE_S] = "s-resize",
    [WLMTK_POINTER_CURSOR_RESIZE_SE] = "se-resize",
    [WLMTK_POINTER_CURSOR_RESIZE_SW] = "sw-resize",
    [WLMTK_POINTER_CURSOR_MAX] = "default",
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_pointer_t *wlmtk_pointer_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr)
{
    wlmtk_pointer_t *pointer_ptr = logged_calloc(1, sizeof(wlmtk_pointer_t));
    if (NULL == pointer_ptr) return NULL;

    pointer_ptr->wlr_cursor_ptr = wlr_cursor_ptr;
    pointer_ptr->wlr_xcursor_manager_ptr = wlr_xcursor_manager_ptr;
    return pointer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_pointer_destroy(wlmtk_pointer_t *pointer_ptr)
{
    free(pointer_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_pointer_set_cursor(
    wlmtk_pointer_t *pointer_ptr,
    wlmtk_pointer_cursor_t cursor)
{
    if (NULL == pointer_ptr &&
        NULL == pointer_ptr->wlr_cursor_ptr &&
        NULL == pointer_ptr->wlr_xcursor_manager_ptr) return;
    if (cursor < 0 || cursor >= WLMTK_POINTER_CURSOR_MAX) return;

    wlr_cursor_set_xcursor(
        pointer_ptr->wlr_cursor_ptr,
        pointer_ptr->wlr_xcursor_manager_ptr,
        _wlmtk_pointer_cursor_names[cursor]);
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == End of input.c ======================================================= */
