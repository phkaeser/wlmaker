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

/** Forward declaration: Box. */
typedef struct _wlmtk_box_t wlmtk_box_t;

#include "container.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Orientation of the box. */
typedef enum {
    WLMTK_BOX_HORIZONTAL,
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
};

/**
 * Initializes the box with the provided virtual method table.
 *
 * @param box_ptr
 * @param orientation
 *
 * @return true on success.
 */
bool wlmtk_box_init(
    wlmtk_box_t *box_ptr,
    wlmtk_box_orientation_t orientation);

/**
 * Un-initializes the box.
 *
 * @param box_ptr
 */
void wlmtk_box_fini(wlmtk_box_t *box_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmtk_box_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_BOX_H__ */
/* == End of box.h ========================================================= */
