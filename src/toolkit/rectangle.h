/* ========================================================================= */
/**
 * @file rectangle.h
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
#ifndef __WLMTK_RECTANGLE_H__
#define __WLMTK_RECTANGLE_H__

#include "element.h"
#include "env.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Rectangle state. */
typedef struct _wlmtk_rectangle_t wlmtk_rectangle_t;

/**
 * Creates a rectangle. Useful for margins and borders.
 *
 * @param env_ptr
 * @param width
 * @param height
 * @param color
 *
 * @return Pointer to the rectangle state or NULL on error.
 */
wlmtk_rectangle_t *wlmtk_rectangle_create(
    wlmtk_env_t *env_ptr,
    int width,
    int height,
    uint32_t color);

/**
 * Destroys the rectangle.
 *
 * @param rectangle_ptr
 */
void wlmtk_rectangle_destroy(wlmtk_rectangle_t *rectangle_ptr);

/**
 * Sets (or updates) the size of the rectangle.
 *
 * @param rectangle_ptr
 * @param width
 * @param height
 */
void wlmtk_rectangle_set_size(
    wlmtk_rectangle_t *rectangle_ptr,
    int width,
    int height);

/** Returns the superclass @ref wlmtk_element_t of the rectangle. */
wlmtk_element_t *wlmtk_rectangle_element(wlmtk_rectangle_t *rectangle_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmtk_rectangle_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_RECTANGLE_H__ */
/* == End of rectangle.h =================================================== */
