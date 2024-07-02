/* ========================================================================= */
/**
 * @file workspace.h
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
 *
 * Interface for a workspace. A server has one or multiple workspaces, and
 * each workspace may hold an arbitrary number of views.
 */
#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__

/** Forward definition: Workspace state. */
typedef struct _wlmaker_workspace_t wlmaker_workspace_t;

#include "server.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a workspace.
 *
 * @param server_ptr
 * @param color
 * @param index
 * @param name_ptr
 *
 * @return Workspace handle or NULL on error. Must be destroyed with
 *     wlmaker_workspace_destroy().
 */
wlmaker_workspace_t *wlmaker_workspace_create(wlmaker_server_t *server_ptr,
                                              uint32_t color,
                                              int index,
                                              const char *name_ptr);

/**
 * Destroys a workspace.
 *
 * @param workspace_ptr
 */
void wlmaker_workspace_destroy(wlmaker_workspace_t *workspace_ptr);

/**
 * Sets this workspace as enabled.
 *
 * Expects that any other workspace has been disabled beforehand, otherwise
 * focus expectations will get wonky.
 *
 * @param workspace_ptr
 * @param enabled
 */
void wlmaker_workspace_set_enabled(wlmaker_workspace_t *workspace_ptr,
                                   bool enabled);

/**
 * Cast: Returns a pointer to @ref wlmaker_workspace_t holding `dlnode_ptr`.
 *
 * @param dlnode_ptr          A pointer to the `dlnode` element.
 *
 * @return Pointer to the workspace holding the dlnode.
 */
wlmaker_workspace_t *wlmaker_workspace_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Cast: Returns a pointer to the `dlnode` element of `workspace_ptr`.
 *
 * @param workspace_ptr
 *
 * @return Pointer to the bs_dllist_node_t `dlnode` of `workspace_ptr`.
 */
bs_dllist_node_t *wlmaker_dlnode_from_workspace(
    wlmaker_workspace_t *workspace_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WORKSPACE_H__ */
/* == End of workspace.h =================================================== */
