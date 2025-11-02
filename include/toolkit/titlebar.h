/* ========================================================================= */
/**
 * @file titlebar.h
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
#ifndef __WLMTK_TITLEBAR_H__
#define __WLMTK_TITLEBAR_H__

#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdint.h>

/** Forward declaration: Title bar. */
typedef struct _wlmtk_titlebar_t wlmtk_titlebar_t;

#include "element.h"
#include "style.h"
#include "window2.h"  // IWYU pragma: keep

/** Properties of the titlebar: Which buttons to show. */
typedef enum {
    /** Whether the 'iconify' button is shown. */
    WLMTK_TITLEBAR_PROPERTY_ICONIFY = UINT32_C(1) << 0,
    /** Whether the 'close' button is shown. */
    WLMTK_TITLEBAR_PROPERTY_CLOSE = UINT32_C(1) << 1
} wlmtk_titlebar_property_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a title bar, suitable as a window title.
 *
 * @param window_ptr
 * @param style_ptr
 *
 * @return Pointer to the title bar state, or NULL on error. Must be free'd
 *     by calling @ref wlmtk_titlebar_destroy.
 */
wlmtk_titlebar_t *wlmtk_titlebar2_create(
    wlmtk_window2_t *window_ptr,
    const wlmtk_titlebar_style_t *style_ptr);

/**
 * Destroys the title bar.
 *
 * @param titlebar_ptr
 */
void wlmtk_titlebar_destroy(wlmtk_titlebar_t *titlebar_ptr);

/**
 * Sets the width of the title bar.
 *
 * @param titlebar_ptr
 * @param width
 *
 * @return Whether the operation was successful.
 */
bool wlmtk_titlebar_set_width(
    wlmtk_titlebar_t *titlebar_ptr,
    unsigned width);

/**
 * Sets the properties of the title bar.
 *
 * @param titlebar_ptr
 * @param properties          See @ref wlmtk_titlebar_property_t.
 */
void wlmtk_titlebar_set_properties(
    wlmtk_titlebar_t *titlebar_ptr,
    uint32_t properties);

/**
 * Sets whether the title bar is activated.
 *
 * @param titlebar_ptr
 * @param activated
 */
void wlmtk_titlebar_set_activated(
    wlmtk_titlebar_t *titlebar_ptr,
    bool activated);

/** Returns whether the title bar is activated. */
bool wlmtk_titlebar_is_activated(wlmtk_titlebar_t *titlebar_ptr);

/**
 * Updates the title text of the titlebar.
 *
 * @param titlebar_ptr
 * @param title_ptr           Expected to remain valid until the next call of
 *                            @ref wlmtk_titlebar_set_title or until the
 *                            `titlebar_ptr` is destroyed.
 */
void wlmtk_titlebar_set_title(
    wlmtk_titlebar_t *titlebar_ptr,
    const char *title_ptr);

/**
 * Returns the super Element of the titlebar.
 *
 * @param titlebar_ptr
 *
 * @return Pointer to the @ref wlmtk_element_t base instantiation to
 *     titlebar_ptr.
 */
wlmtk_element_t *wlmtk_titlebar_element(wlmtk_titlebar_t *titlebar_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_titlebar_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TITLEBAR_H__ */
/* == End of titlebar.h ==================================================== */
