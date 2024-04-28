/* ========================================================================= */
/**
 * @file panel.h
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
#ifndef __PANEL_H__
#define __PANEL_H__

#include <libbase/libbase.h>

/** Forward declaration: An element of a layer, we call it: Panel. */
typedef struct _wlmtk_panel_t wlmtk_panel_t;

#include "container.h"
#include "layer.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the panel. */
struct _wlmtk_panel_t {
    /** Super class of the panel. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** The layer that this panel belongs to. NULL if none. */
    wlmtk_layer_t             *layer_ptr;
};

/**
 * Initializes the panel.
 *
 * @param panel_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_panel_init(wlmtk_panel_t *panel_ptr,
                      wlmtk_env_t *env_ptr);

/**
 * Un-initializes the panel.
 *
 * @param panel_ptr
 */
void wlmtk_panel_fini(wlmtk_panel_t *panel_ptr);

/** @return pointer to the super @ref wlmtk_element_t of the panel. */
wlmtk_element_t *wlmtk_panel_element(wlmtk_panel_t *panel_ptr);

/**
 * Sets the layer for the `panel_ptr`.
 *
 * @protected This method must only be called from @ref wlmtk_layer_t.
 *
 * @param panel_ptr
 * @param layer_ptr
 */
void wlmtk_panel_set_layer(wlmtk_panel_t *panel_ptr,
                           wlmtk_layer_t *layer_ptr);

/** @return the wlmtk_layer_t this panel belongs to. Or NULL, if unmapped. */
wlmtk_layer_t *wlmtk_panel_get_layer(wlmtk_panel_t *panel_ptr);

/** Unit test cases of panel. */
extern const bs_test_case_t wlmtk_panel_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __PANEL_H__ */
/* == End of panel.h ======================================================= */
