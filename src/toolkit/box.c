/* ========================================================================= */
/**
 * @file box.c
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

#include "box.h"

/* == Declarations ========================================================= */

static void container_destroy(wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_container_impl_t container_impl = {
    .destroy = container_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_box_init(
    wlmtk_box_t *box_ptr,
    __UNUSED__ const wlmtk_box_impl_t *box_impl_ptr)
{
    if (!wlmtk_container_init(&box_ptr->super_container, &container_impl)) {
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_box_fini(__UNUSED__ wlmtk_box_t *box_ptr)
{
    // nothing to do.
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_box_t *box_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_box_t, super_container);
    wlmtk_box_fini(box_ptr);
}

/* == End of box.c ========================================================= */
