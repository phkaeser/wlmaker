/* ========================================================================= */
/**
 * @file pubase.h
 * Base container for popups.
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
#ifndef __WLMTK_PUBASE_H__
#define __WLMTK_PUBASE_H__

/** Forward declaration: State of popup base. */
typedef struct _wlmtk_pubase_t wlmtk_pubase_t;

#include "container.h"
#include "env.h"

/** State of the popup base. */
struct _wlmtk_pubase_t {
    /** Super class of the popup base: Container, holding popups. */
    wlmtk_container_t         super_container;
};

#include "popup.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Initialize the popup base.
 *
 * @param pubase_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_pubase_init(wlmtk_pubase_t *pubase_ptr, wlmtk_env_t *env_ptr);

/**
 * Uninitialize the popup base.
 *
 * @param pubase_ptr
 */
void wlmtk_pubase_fini(wlmtk_pubase_t *pubase_ptr);

/**
 * Adds `popup_ptr` to `pubase_ptr`.
 *
 * @param pubase_ptr
 * @param popup_ptr
 */
void wlmtk_pubase_add_popup(wlmtk_pubase_t *pubase_ptr,
                            wlmtk_popup_t *popup_ptr);

/**
 * Removes `popup_ptr` from `pubase_ptr`.
 *
 * @param pubase_ptr
 * @param popup_ptr
 */
void wlmtk_pubase_remove_popup(wlmtk_pubase_t *pubase_ptr,
                               wlmtk_popup_t *popup_ptr);

/** Returns the base @ref wlmtk_element_t for `pubase_ptr`. */
wlmtk_element_t *wlmtk_pubase_element(wlmtk_pubase_t *pubase_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_PUBASE_H__ */
/* == End of pubase.h ====================================================== */
