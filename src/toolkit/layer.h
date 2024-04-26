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
#ifndef __LAYER_H__
#define __LAYER_H__

/** Forward declaration: Layer state. */
typedef struct _wlmtk_layer_t wlmtk_layer_t;

#include "element.h"
#include "env.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a layer. Layers contain panels, such as layer shells.
 *
 * @param env_ptr
 *
 * @return Pointer to the layer handle or NULL on error.
 */
wlmtk_layer_t *wlmtk_layer_create(wlmtk_env_t *env_ptr);

/**
 * Destroys the layer.
 *
 * @param layer_ptr
 */
void wlmtk_layer_destroy(wlmtk_layer_t *layer_ptr);

/** @return Pointer to @ref wlmtk_layer_t::super_container::super_element. */
wlmtk_element_t *wlmtk_layer_element(wlmtk_layer_t *layer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __LAYER_H__ */
/* == End of layer.h ======================================================= */
