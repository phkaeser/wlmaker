/* ========================================================================= */
/**
 * @file base.h
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
#ifndef __WLMTK_BASE_H__
#define __WLMTK_BASE_H__

struct _wlmtk_base_t;
/** Forward declaration: State of a base element. */
typedef struct _wlmtk_base_t wlmtk_base_t;

#include <libbase/libbase.h>
#include <stdbool.h>

#include "container.h"  // IWYU pragma: keep
#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Base: Holds a base element with stacked & floating elements on top. */
struct _wlmtk_base_t {
    /** Super class of the base. Holds element and popups. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' element. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Points to the element contained at the base. */
    wlmtk_element_t           *content_element_ptr;
};

/**
 * Initializes the base with the given element.
 *
 * @param base_ptr
 * @param element_ptr         is added to @ref wlmtk_base_t::super_container
 *                            until @ref wlmtk_base_fini is called. Will take
 *                            ownewrship. May be NULL.
 *
 * @return true on success.
 */
bool wlmtk_base_init(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr);

/**
 * Un-initializes the base.
 *
 * @param base_ptr
 */
void wlmtk_base_fini(wlmtk_base_t *base_ptr);

/** @return Pointer to the superclass @ref wlmtk_element_t of the base. */
wlmtk_element_t *wlmtk_base_element(wlmtk_base_t *base_ptr);

/**
 * Sets @ref wlmtk_base_t::content_element_ptr. Any earlier content element
 * will be destroyed.
 *
 * @param base_ptr
 * @param content_element_ptr
 */
void wlmtk_base_set_content_element(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *content_element_ptr);

/** Adds a stacked element (eg. a popup) */
void wlmtk_base_push_element(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr);

/** Removes a stacked element. */
void wlmtk_base_pop_element(
    wlmtk_base_t *base_ptr,
    wlmtk_element_t *element_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_base_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BASE_H__ */
/* == End of base.h ======================================================== */
