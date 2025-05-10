/* ========================================================================= */
/**
 * @file layer.h
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
#ifndef __WLMTK_LAYER_H__
#define __WLMTK_LAYER_H__

#include <stdbool.h>
#include <libbase/libbase.h>

/** Forward declaration: Layer state. */
typedef struct _wlmtk_layer_t wlmtk_layer_t;
/** Forward declaration: Layer state. */
typedef struct _wlmtk_layer_output_t wlmtk_layer_output_t;

#include "element.h"
#include "env.h"
#include "panel.h"  // IWYU pragma: keep
#include "workspace.h"  // IWYU pragma: keep

/** Forward declaration: wlr output layout. */
struct wlr_output_layout;
/** Forward declaration: wlr output. */
struct wlr_output;


#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a layer. Layers contain panels, such as layer shells.
 *
 * @param wlr_output_layout_ptr The output layout.
 * @param env_ptr
 *
 * @return Pointer to the layer handle or NULL on error.
 */
wlmtk_layer_t *wlmtk_layer_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the layer.
 *
 * @param layer_ptr
 */
void wlmtk_layer_destroy(wlmtk_layer_t *layer_ptr);

/** @return Pointer to super @ref wlmtk_element_t of the layer. */
wlmtk_element_t *wlmtk_layer_element(wlmtk_layer_t *layer_ptr);

/**
 * Adds the panel to the output within the specified layer. This will trigger
 * an update to the layer's layout, and a call to request_size of each panel
 * of that output.
 *
 * @param layer_ptr
 * @param panel_ptr
 * @param wlr_output_ptr
 */
bool wlmtk_layer_add_panel(
    wlmtk_layer_t *layer_ptr,
    wlmtk_panel_t *panel_ptr,
    struct wlr_output *wlr_output_ptr);

/**
 * Removes the panel from the layer.
 *
 * @param layer_ptr
 * @param panel_ptr
 */
void wlmtk_layer_remove_panel(wlmtk_layer_t *layer_ptr,
                              wlmtk_panel_t *panel_ptr);

/**
 * Calls @ref wlmtk_panel_compute_dimensions for each contained panel.
 *
 * The Wayland protocol spells it is 'undefined' how panels (layer shells)
 * are stacked and configured within a layer. For wlmaker, we'll configure
 * the panels in sequence as they were added (found in the container, back
 * to front).
 *
 * @param layer_ptr
 */
void wlmtk_layer_reconfigure(wlmtk_layer_t *layer_ptr);

/**
 * Calls @ref wlmtk_panel_compute_dimensions for each contained panel.
 *
 * The Wayland protocol spells it is 'undefined' how panels (layer shells)
 * are stacked and configured within a layer. For wlmaker, we'll configure
 * the panels in sequence as they were added (found in the container, back
 * to front).
 *
 * @param layer_output_ptr
 */
void wlmtk_layer_output_reconfigure(wlmtk_layer_output_t *layer_output_ptr);

/**
 * Sets the parent workspace for the layer.
 *
 * Should only be called from @ref wlmtk_workspace_t methods.
 *
 * @param layer_ptr
 * @param workspace_ptr       NULL to clear the workspace reference.
 */
void wlmtk_layer_set_workspace(wlmtk_layer_t *layer_ptr,
                               wlmtk_workspace_t *workspace_ptr);


/** Layer unit test. */
extern const bs_test_case_t wlmtk_layer_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_LAYER_H__ */
/* == End of layer.h ======================================================= */
