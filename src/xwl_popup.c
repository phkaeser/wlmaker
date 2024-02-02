/* ========================================================================= */
/**
 * @file xwl_popup.c
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

#include "xwl_popup.h"

/* == Declarations ========================================================= */

/** State of an XWayland popup (child window). */
struct _wlmaker_xwl_popup_t {
    /** Content that this popup embeds. */
    wlmaker_xwl_content_t     *xwl_content_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_popup_t *wlmaker_xwl_popup_create(
    wlmaker_xwl_content_t *xwl_content_ptr)
{
    wlmaker_xwl_popup_t *xwl_popup_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_popup_t));
    if (NULL == xwl_popup_ptr) return NULL;
    xwl_popup_ptr->xwl_content_ptr = xwl_content_ptr;

    return xwl_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_popup_destroy(wlmaker_xwl_popup_t *xwl_popup_ptr)
{
    free(xwl_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of xwl_popup.c =================================================== */
