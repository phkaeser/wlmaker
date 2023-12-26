/* ========================================================================= */
/**
 * @file surface.h
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
#ifndef __WLMTK_SURFACE_H__
#define __WLMTK_SURFACE_H__

#include <libbase/libbase.h>

#include "element.h"
#include "env.h"

/** Forward declaration. */
struct wlr_surface;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of a toolkit's WLR surface. */
typedef struct _wlmtk_surface_t wlmtk_surface_t;

/**
 * Creates a surface.
 *
 * @param wlr_surface_ptr
 * @param env_ptr
 */
wlmtk_surface_t *wlmtk_surface_create(
    struct wlr_surface *wlr_surface_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the surface.
 *
 * @param surface_ptr
 */
void wlmtk_surface_destroy(wlmtk_surface_t *surface_ptr);

/**
 * Returns a pointer to the surface's element superclass instance.
 *
 * @param surface_ptr
 *
 * @return Pointer to the corresponding @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_surface_element(wlmtk_surface_t *surface_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_surface_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_SURFACE_H__ */
/* == End of surface.h ===================================================== */
