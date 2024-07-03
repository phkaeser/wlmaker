/* ========================================================================= */
/**
 * @file background.c
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

#include "background.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** Background state. */
struct _wlmaker_background_t {
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_background_t *wlmaker_background_create(void)
{
    wlmaker_background_t *background_ptr = logged_calloc(
        1, sizeof(wlmaker_background_t));
    if (NULL == background_ptr) return NULL;

    return background_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_background_destroy(wlmaker_background_t *background_ptr)
{
    free(background_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of background.c ================================================== */
