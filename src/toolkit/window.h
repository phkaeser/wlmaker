/* ========================================================================= */
/**
 * @file window.h
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
#ifndef __WLMTK_WINDOW_H__
#define __WLMTK_WINDOW_H__

/** Forward declaration. */
typedef struct _wlmtk_window_t wlmtk_window_t;

#include "container.h"
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Window state. */
struct _wlmtk_window_t {
    /** Super class of the window. */
    wlmtk_container_t         super_container;
    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** The principal surface of the window. */
    wlmtk_surface_t           *surface_ptr;
};

/**
 * Initializes the window with the provided surface.
 *
 * @param window_ptr
 * @param surface_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_window_init(
    wlmtk_window_t *window_ptr,
    wlmtk_surface_t *surface_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Un-initializes the window.
 *
 * @param window_ptr
 */
void wlmtk_window_fini(wlmtk_window_t *window_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_window_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_WINDOW_H__ */
/* == End of window.h ====================================================== */
