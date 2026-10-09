/* ========================================================================= */
/**
 * @file layer_shell.c
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

#include "layer_shell.h"

#include <stdlib.h>

#include "wlclient.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

/* == Declarations ========================================================= */

static void _wlmcl_layer_shell_setup(
    void *bound_interface_ptr,
    void *userdata_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmcl_layer_shell_register(
    wlmcl_client_t *wlclient_ptr,
    struct zwlr_layer_shell_v1 **layer_shell_ptr_ptr)
{
    return NULL != wlmcl_client_register_interface(
        wlclient_ptr,
        &zwlr_layer_shell_v1_interface,
        /* desired_version */ 5,
        /* required */ true,
        _wlmcl_layer_shell_setup,
        layer_shell_ptr_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Callback for @ref wlmcl_client_register_interface. Stores layer shell. */
void _wlmcl_layer_shell_setup(
    void *bound_interface_ptr,
    void *userdata_ptr)
{
    struct zwlr_layer_shell_v1 **layer_shell_ptr_ptr = userdata_ptr;
    *layer_shell_ptr_ptr = bound_interface_ptr;
}

/* == End of layer_shell.c ================================================= */
