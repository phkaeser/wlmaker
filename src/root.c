/* ========================================================================= */
/**
 * @file root.c
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

#include "root.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the root element. */
struct _wlmaker_root_t {
    /** The root's container: Holds workspaces and the curtain. */
    wlmtk_container_t         container;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_t *wlmaker_root_create(
    struct wlr_scene *wlr_scene_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_root_t *root_ptr = logged_calloc(1, sizeof(wlmaker_root_t));
    if (NULL == root_ptr) return NULL;

    if (!wlmtk_container_init_attached(
            &root_ptr->container,
            env_ptr,
            &wlr_scene_ptr->tree)) {
        wlmaker_root_destroy(root_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(&root_ptr->container.super_element, true);

    return root_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_destroy(wlmaker_root_t *root_ptr)
{
    wlmtk_container_fini(&root_ptr->container);

    free(root_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of root.c ======================================================== */
