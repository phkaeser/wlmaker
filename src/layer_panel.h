/* ========================================================================= */
/**
 * @file layer_panel.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __LAYER_PANEL_H__
#define __LAYER_PANEL_H__

/** Handler for a layer panel. */
typedef struct _wlmaker_layer_panel_t wlmaker_layer_panel_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration for wlroots layer surface. */
struct wlr_layer_surface_v1;

/**
 * Creates a layer panel from the given layer surface.
 *
 * @param wlr_layer_surface_v1_ptr
 * @param server_ptr
 *
 * @return The handler for the layer surface or NULL on error.
 */
wlmaker_layer_panel_t *wlmaker_layer_panel_create(
    struct wlr_layer_surface_v1 *wlr_layer_surface_v1_ptr,
    wlmaker_server_t *server_ptr);

/** Unit test cases of layer panel. */
extern const bs_test_case_t wlmaker_layer_panel_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LAYER_PANEL_H__ */
/* == End of layer_panel.h ================================================= */
