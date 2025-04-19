/* ========================================================================= */
/**
 * @file box.h
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
#ifndef __WLMTK_BOX_H__
#define __WLMTK_BOX_H__

#include <stdbool.h>

#include "libbase/libbase.h"

#include "container.h"
#include "element.h"
#include "env.h"
#include "style.h"

struct _wlmtk_box_t;
/** Forward declaration: Box. */
typedef struct _wlmtk_box_t wlmtk_box_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Orientation of the box. */
typedef enum {
    /** Horizontal box layout. The container's "front" is on the left. */
    WLMTK_BOX_HORIZONTAL,
    /** Vertical box layout. The container's "front" is the top. */
    WLMTK_BOX_VERTICAL,
} wlmtk_box_orientation_t;

/** State of the box. */
struct _wlmtk_box_t {
    /** Super class of the box. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;
    /** Orientation of the box. */
    wlmtk_box_orientation_t   orientation;

    /** Environment. */
    wlmtk_env_t               *env_ptr;

    /** Container for the box's elements. */
    wlmtk_container_t         element_container;
    /** Container for margin elements. */
    wlmtk_container_t         margin_container;

    /** Margin style. */
    wlmtk_margin_style_t      style;
};

/**
 * Initializes the box with the provided virtual method table.
 *
 * @param box_ptr
 * @param orientation
 * @param env_ptr
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_box_init(
    wlmtk_box_t *box_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_box_orientation_t orientation,
    const wlmtk_margin_style_t *style_ptr);

/**
 * Un-initializes the box.
 *
 * @param box_ptr
 */
void wlmtk_box_fini(wlmtk_box_t *box_ptr);

/**
 * Adds `element_ptr` to the front of the box.
 *
 * @param box_ptr
 * @param element_ptr
 */
void wlmtk_box_add_element_front(wlmtk_box_t *box_ptr, wlmtk_element_t *element_ptr);

/**
 * Adds `element_ptr` to the back of the box.
 *
 * @param box_ptr
 * @param element_ptr
 */
void wlmtk_box_add_element_back(wlmtk_box_t *box_ptr, wlmtk_element_t *element_ptr);

/**
 * Removes `element_ptr` from the box.
 *
 * Requires that element_ptr is an element of the box.
 *
 * @param box_ptr
 * @param element_ptr
 */
void wlmtk_box_remove_element(wlmtk_box_t *box_ptr, wlmtk_element_t *element_ptr);

/** @return Pointer to the superclass' @ref wlmtk_element_t of `box_ptr`. */
wlmtk_element_t *wlmtk_box_element(wlmtk_box_t *box_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmtk_box_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BOX_H__ */
/* == End of box.h ========================================================= */
