/* ========================================================================= */
/**
 * @file workspace.c
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

/** State of the workspace. */
struct _wlmtk_workspace_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;
};

static void workspace_container_destroy(wlmtk_container_t *container_ptr);

/** Method table for the container's virtual methods. */
const wlmtk_container_impl_t  workspace_container_impl = {
    .destroy = workspace_container_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_workspace_create(void)
{
    wlmtk_workspace_t *workspace_ptr =
        logged_calloc(1, sizeof(wlmtk_workspace_t));
    if (NULL == workspace_ptr) return NULL;

    if (!wlmtk_container_init(&workspace_ptr->super_container,
                              &workspace_container_impl)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }

    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr)
{
    wlmtk_container_fini(&workspace_ptr->super_container);
    free(workspace_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void workspace_container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_workspace_t, super_container);
    wlmtk_workspace_destroy(workspace_ptr);
}

/* == End of workspace.c =================================================== */
