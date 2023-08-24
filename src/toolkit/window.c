/* ========================================================================= */
/**
 * @file window.c
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

#include "toolkit.h"

/* == Declarations ========================================================= */

/** State of the window. */
struct _wlmtk_window_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;
};

static void window_container_destroy(wlmtk_container_t *container_ptr);

/** Method table for the container's virtual methods. */
const wlmtk_container_impl_t  window_container_impl = {
    .destroy = window_container_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create(void)
{
    wlmtk_window_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window_t));
    if (NULL == window_ptr) return NULL;

    if (!wlmtk_container_init(&window_ptr->super_container,
                              &window_container_impl)) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr)
{
    wlmtk_container_fini(&window_ptr->super_container);
    free(window_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void window_container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window_t, super_container);
    wlmtk_window_destroy(window_ptr);
}

/* == End of window.c ====================================================== */
