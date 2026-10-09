/* ========================================================================= */
/**
 * @file layer_shell.h
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright 2026 Google LLC
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
#ifndef __WLMAKER_WLCLIENT_LAYER_SHELL_H__
#define __WLMAKER_WLCLIENT_LAYER_SHELL_H__

#include <stdbool.h>

#include "wlclient.h"

struct zwlr_layer_shell_v1;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Registers the layer shell interface with the client.
 *
 * @param wlclient_ptr
 * @param layer_shell_ptr_ptr
 *
 * @return true on success.
 */
bool wlmcl_layer_shell_register(
    wlmcl_client_t *wlclient_ptr,
    struct zwlr_layer_shell_v1 **layer_shell_ptr_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_WLCLIENT_LAYER_SHELL_H__ */
/* == End of layer_shell.h ================================================= */
