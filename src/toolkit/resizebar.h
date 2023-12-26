/* ========================================================================= */
/**
 * @file resizebar.h
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
#ifndef __WLMTK_RESIZEBAR_H__
#define __WLMTK_RESIZEBAR_H__

/** Forward declaration: Title bar. */
typedef struct _wlmtk_resizebar_t wlmtk_resizebar_t;
/** Forward declaration. */
struct wlr_cursor;
/** Forward declaration. */
struct wlr_xcursor_manager;

#include "element.h"
#include "primitives.h"
#include "toplevel.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Style options for the resizebar. */
typedef struct {
    /** Fill style for the complete resizebar. */
    wlmtk_style_fill_t        fill;
    /** Height of the resize bar. */
    unsigned                  height;
    /** Width of the corners. */
    unsigned                  corner_width;
    /** Width of the bezel. */
    uint32_t                  bezel_width;
    /** Style of the margin within the resizebar. */
    wlmtk_margin_style_t      margin_style;
} wlmtk_resizebar_style_t;

/**
 * Creates the resize bar.
 *
 * @param env_ptr
 * @param toplevel_ptr
 * @param style_ptr
 *
 * @return Pointer to the resizebar state, or NULL on error.
 */
wlmtk_resizebar_t *wlmtk_resizebar_create(
    wlmtk_env_t *env_ptr,
    wlmtk_toplevel_t *toplevel_ptr,
    const wlmtk_resizebar_style_t *style_ptr);

/**
 * Destroys the resize bar.
 *
 * @param resizebar_ptr
 */
void wlmtk_resizebar_destroy(wlmtk_resizebar_t *resizebar_ptr);

/**
 * Sets the width of the resize bar.
 *
 * @param resizebar_ptr
 * @param width
 *
 * @return true on success.
 */
bool wlmtk_resizebar_set_width(
    wlmtk_resizebar_t * resizebar_ptr,
    unsigned width);

/**
 * Returns the super Element of the resizebar.
 *
 * @param resizebar_ptr
 *
 * @return Pointer to the element.
 */
wlmtk_element_t *wlmtk_resizebar_element(wlmtk_resizebar_t *resizebar_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_resizebar_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_RESIZEBAR_H__ */
/* == End of resizebar.h ================================================== */
