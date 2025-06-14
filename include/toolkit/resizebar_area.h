/* ========================================================================= */
/**
 * @file resizebar_area.h
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
#ifndef __WLMTK_RESIZEBAR_AREA_H__
#define __WLMTK_RESIZEBAR_AREA_H__

#include <libbase/libbase.h>
#include <stdbool.h>          // for bool
#include <stdint.h>           // for uint32_t

/** Forward declaration: Element of the resizebar. */
typedef struct _wlmtk_resizebar_area_t wlmtk_resizebar_area_t ;

#include "element.h"
#include "style.h"
#include "window.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a resizebar button.
 *
 * @param window_ptr
 * @param edges
 *
 * @return Pointer to the resizebar button.
 */
wlmtk_resizebar_area_t *wlmtk_resizebar_area_create(
    wlmtk_window_t *window_ptr,
    uint32_t edges);

/**
 * Destroys the resizebar element.
 *
 * @param resizebar_area_ptr
 */
void wlmtk_resizebar_area_destroy(
    wlmtk_resizebar_area_t *resizebar_area_ptr);

/**
 * Redraws the element, with updated position and width.
 *
 * @param resizebar_area_ptr
 * @param gfxbuf_ptr
 * @param position
 * @param width
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_resizebar_area_redraw(
    wlmtk_resizebar_area_t *resizebar_area_ptr,
    bs_gfxbuf_t *gfxbuf_ptr,
    unsigned position,
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr);

/** Returns the button's super_buffer.super_element address. */
wlmtk_element_t *wlmtk_resizebar_area_element(
    wlmtk_resizebar_area_t *resizebar_area_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_resizebar_area_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_RESIZEBAR_AREA_H__ */
/* == End of resizebar_area.h ============================================== */
