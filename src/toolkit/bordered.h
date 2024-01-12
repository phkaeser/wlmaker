/* ========================================================================= */
/**
 * @file bordered.h
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
#ifndef __WLMTK_BORDERED_H__
#define __WLMTK_BORDERED_H__

#include "container.h"
#include "rectangle.h"
#include "style.h"

/** Forward declaration: Bordered container state. */
typedef struct _wlmtk_bordered_t wlmtk_bordered_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of the bordered container. */
struct _wlmtk_bordered_t {
    /** Super class of the bordered. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the super container before extending it. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Points to the element that will be enclosed by the border. */
    wlmtk_element_t           *element_ptr;
    /** Style of the border. */
    wlmtk_margin_style_t      style;

    /** Border element at the northern side. Includes east + west corners. */
    wlmtk_rectangle_t         *northern_border_rectangle_ptr;
    /** Border element at the eastern side. */
    wlmtk_rectangle_t         *eastern_border_rectangle_ptr;
    /** Border element at the southern side. Includes east + west corners. */
    wlmtk_rectangle_t         *southern_border_rectangle_ptr;
    /** Border element at the western side. */
    wlmtk_rectangle_t         *western_border_rectangle_ptr;
};

/**
 * Initializes the bordered element.
 *
 * The bordered element positions the element within such that north-western
 * corner is at (0, 0).
 *
 * @param bordered_ptr
 * @param env_ptr
 * @param element_ptr
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_bordered_init(wlmtk_bordered_t *bordered_ptr,
                         wlmtk_env_t *env_ptr,
                         wlmtk_element_t *element_ptr,
                         const wlmtk_margin_style_t *style_ptr);

/**
 * Un-initializes the bordered element.
 *
 * @param bordered_ptr
 */
void wlmtk_bordered_fini(wlmtk_bordered_t *bordered_ptr);

/**
 * Updates the style.
 *
 * @param bordered_ptr
 * @param style_ptr
 */
void wlmtk_bordered_set_style(wlmtk_bordered_t *bordered_ptr,
                              const wlmtk_margin_style_t *style_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_bordered_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BORDERED_H__ */
/* == End of bordered.h ==================================================== */
