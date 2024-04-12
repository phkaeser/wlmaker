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

    /** Whether the root is currently locked. */
    bool                      locked;
    /** Reference to the lock, see @ref wlmaker_root_lock. */
    wlmaker_lock_t            *lock_ptr;

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

/* ------------------------------------------------------------------------- */
bool wlmaker_root_lock(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr)
{
    if (root_ptr->locked) {
        bs_log(BS_WARNING, "Root already locked by %p", root_ptr->lock_ptr);
        return false;
    }

    wlmtk_container_add_element(
        &root_ptr->container,
        wlmaker_lock_element(lock_ptr));
    root_ptr->lock_ptr = lock_ptr;

    root_ptr->locked = true;
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmaker_root_unlock(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr)
{
    // Guard clause: Not locked => nothing to do.
    if (!root_ptr->locked) return false;
    if (lock_ptr != root_ptr->lock_ptr) {
        bs_log(BS_ERROR, "Lock held by %p, but attempted to unlock by %p",
               root_ptr->lock_ptr, lock_ptr);
        return false;
    }

    wlmaker_root_lock_unreference(root_ptr, lock_ptr);
    root_ptr->locked = false;
    return true;

}

/* ------------------------------------------------------------------------- */
void wlmaker_root_lock_unreference(
    wlmaker_root_t *root_ptr,
    wlmaker_lock_t *lock_ptr)
{
    if (lock_ptr != root_ptr->lock_ptr) return;

    wlmtk_container_remove_element(
        &root_ptr->container,
        wlmaker_lock_element(root_ptr->lock_ptr));
    root_ptr->lock_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_set_lock_surface(
    __UNUSED__ wlmaker_root_t *root_ptr,
    wlmtk_surface_t *surface_ptr)
{
    wlmtk_surface_set_activated(surface_ptr, true);
}

/* == Local (static) methods =============================================== */

/* == End of root.c ======================================================== */
