/* ========================================================================= */
/**
 * @file dock.h
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
 * Creates the wlmaker dock: A view, with server-bound surfaces, that acts as
 * launcher for Apps.
 *
 * Corresponding Window Maker documentation:
 * http://www.windowmaker.org/docs/guidedtour/dock.html
 */
#ifndef __DOCK_H__
#define __DOCK_H__

/** Forward definition: Dock handle. */
typedef struct _wlmaker_dock_t wlmaker_dock_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the Dock handle. Needs the server to be up with workspaces running.
 *
 * @param server_ptr
 *
 * @return Pointer to the Dock handle, or NULL on error.
 */
wlmaker_dock_t *wlmaker_dock_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the Dock handle.
 *
 * @param dock_ptr
 */
void wlmaker_dock_destroy(wlmaker_dock_t *dock_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __DOCK_H__ */
/* == End of dock.h ======================================================== */
