/* ========================================================================= */
/**
 * @file titlebar_title.h
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
#ifndef __WLMTK_TITLEBAR_TITLE_H__
#define __WLMTK_TITLEBAR_TITLE_H__

/** Forward declaration. */
typedef struct _wlmtk_titlebar_title_t wlmtk_titlebar_title_t;

#include <stdbool.h>
#include <libbase/libbase.h>

#include "titlebar.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a title bar title.
 *
 * @param window_ptr
 *
 * @return Title handle.
 */
wlmtk_titlebar_title_t *wlmtk_titlebar_title_create(
    wlmtk_window_t *window_ptr);

/**
 * Destroys the titlebar title.
 *
 * @param titlebar_title_ptr
 */
void wlmtk_titlebar_title_destroy(
    wlmtk_titlebar_title_t *titlebar_title_ptr);

/**
 * Redraws the title section of the title bar.
 *
 * @param titlebar_title_ptr
 * @param focussed_gfxbuf_ptr Titlebar background when focussed.
 * @param blurred_gfxbuf_ptr  Titlebar background when blurred.
 * @param position            Position of title telative to titlebar.
 * @param width               Width of title.
 * @param activated           Whether the title bar should start focussed.
 * @param title_ptr           Title, or NULL.
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_titlebar_title_redraw(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    int width,
    bool activated,
    const char *title_ptr,
    const wlmtk_titlebar_style_t *style_ptr);

/**
 * Sets activation status of the titlebar's title.
 *
 * @param titlebar_title_ptr
 * @param activated
 */
void wlmtk_titlebar_title_set_activated(
    wlmtk_titlebar_title_t *titlebar_title_ptr,
    bool activated);

/**
 * Returns the superclass @ref wlmtk_element_t for the titlebar title.
 *
 * @param titlebar_title_ptr
 *
 * @return Pointer to the super element.
 */
wlmtk_element_t *wlmtk_titlebar_title_element(
    wlmtk_titlebar_title_t *titlebar_title_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_titlebar_title_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TITLEBAR_TITLE_H__ */
/* == End of titlebar_title.h ============================================== */
