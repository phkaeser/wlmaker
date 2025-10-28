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
#if defined(WLMAKER_HAVE_XWAYLAND)

#include "xwl_popup.h"

#include <libbase/libbase.h>
#include <stdlib.h>

/* == Declarations ========================================================= */

/** State of an XWayland popup (child window). */
struct _wlmaker_xwl_popup_t {
    /** Content that this popup embeds. */
    wlmaker_xwl_surface_t     *xwl_surface_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xwl_popup_t *wlmaker_xwl_popup_create(
    wlmaker_xwl_surface_t *xwl_surface_ptr)
{
    wlmaker_xwl_popup_t *xwl_popup_ptr = logged_calloc(
        1, sizeof(wlmaker_xwl_popup_t));
    if (NULL == xwl_popup_ptr) return NULL;
    xwl_popup_ptr->xwl_surface_ptr = xwl_surface_ptr;

    return xwl_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_popup_destroy(wlmaker_xwl_popup_t *xwl_popup_ptr)
{
    free(xwl_popup_ptr);
}

/* == Local (static) methods =============================================== */

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
/* == End of xwl_popup.c =================================================== */
