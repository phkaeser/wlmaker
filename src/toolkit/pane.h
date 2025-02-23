/* ========================================================================= */
/**
 * @file pane.h
 *
 * @copyright
 * Copyright 2025 Google LLC
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
#ifndef __WLMTK_PANE_H__
#define __WLMTK_PANE_H__

/** Forward declaration: State of a (window or popup) pane. */
typedef struct _wlmtk_pane_t wlmtk_pane_t;

#include "container.h"
#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of a window or popup pane. */
struct _wlmtk_pane_t {
    /** Super class of the pane: A container, holds element and popups. */
    wlmtk_container_t         super_container;

    /** Points to the element contained in this pane. */
    wlmtk_element_t           *element_ptr;
    /** Container for the popups. */
    wlmtk_container_t         popup_container;
};

/**
 * Initializes the pane with the given element.
 *
 * @param pane_ptr
 * @param element_ptr         is added to @ref wlmtk_pane_t::super_container
 *                            until @ref wlmtk_pane_fini is called. Will *NOT*
 *                            take ownership.
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_pane_init(
    wlmtk_pane_t *pane_ptr,
    wlmtk_element_t *element_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the pane.
 *
 * @param pane_ptr
 */
void wlmtk_pane_fini(wlmtk_pane_t *pane_ptr);

/** @return Pointer to the superclass @ref wlmtk_element_t of the pane. */
wlmtk_element_t *wlmtk_pane_element(wlmtk_pane_t *pane_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_pane_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_PANE_H__ */
/* == End of pane.h ======================================================== */
