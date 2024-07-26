/* ========================================================================= */
/**
 * @file popup.h
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
#ifndef __WLMTK_POPUP_H__
#define __WLMTK_POPUP_H__

/** Forward declaration: Popup. */
typedef struct _wlmtk_popup_t wlmtk_popup_t;

#include "container.h"
#include "env.h"
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * State of a popup.
 *
 * A popup contains a @ref wlmtk_element_t, and may contain further popups.
 * These further popups will be stacked above the principal element, in order
 * of them being added.
 */
struct _wlmtk_popup_t {
    /** Super class of the panel. */
    wlmtk_container_t         super_container;

    /** And the popup container. Popups can contain child popups. */
    wlmtk_container_t         popup_container;

    /** The contained element. */
    wlmtk_element_t           *element_ptr;
};

/**
 * Initializes the popup.
 *
 * @param popup_ptr
 * @param env_ptr
 * @param surface_ptr
 *
 * @return true on success.
 */
bool wlmtk_popup_init(
    wlmtk_popup_t *popup_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_surface_t *surface_ptr);

/**
 * Un-initializes the popup. Will remove it from the parent container.
 *
 * @param popup_ptr
 */
void wlmtk_popup_fini(wlmtk_popup_t *popup_ptr);

/** Returns the base @ref wlmtk_element_t. */
wlmtk_element_t *wlmtk_popup_element(wlmtk_popup_t *popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_POPUP_H__ */
/* == End of popup.h ======================================================= */
