/* ========================================================================= */
/**
 * @file layer_shell.h
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
#ifndef __LAYER_SHELL_H__
#define __LAYER_SHELL_H__

/** Handle for the layer shell. */
typedef struct _wlmaker_layer_shell_t wlmaker_layer_shell_t;

#include "server.h"  // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a layer shell handler.
 *
 * @param server_ptr
 *
 * @return A handle to the layer shell handler, or NULL on error.
 */
wlmaker_layer_shell_t *wlmaker_layer_shell_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the layer shell handler.
 *
 * @param layer_shell_ptr
 */
void wlmaker_layer_shell_destroy(wlmaker_layer_shell_t *layer_shell_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LAYER_SHELL_H__ */
/* == End of layer_shell.h ================================================= */
