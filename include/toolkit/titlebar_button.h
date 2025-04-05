/* ========================================================================= */
/**
 * @file titlebar_button.h
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
#ifndef __WLMTK_TITLEBAR_BUTTON_H__
#define __WLMTK_TITLEBAR_BUTTON_H__

#include <stdbool.h>
#include <libbase/libbase.h>

/** Forward declaration. */
typedef struct _wlmtk_titlebar_button_t wlmtk_titlebar_button_t;

#include "titlebar.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Function pointer to method for drawing the button contents. */
typedef void (*wlmtk_titlebar_button_draw_t)(
    cairo_t *cairo_ptr, int siye, uint32_t color);

/**
 * Creates a button for the titlebar.
 *
 * @param env_ptr
 * @param click_handler
 * @param window_ptr
 * @param draw
 *
 * @return Pointer to the titlebar button, or NULL on error.
 */
wlmtk_titlebar_button_t *wlmtk_titlebar_button_create(
    wlmtk_env_t *env_ptr,
    void (*click_handler)(wlmtk_window_t *window_ptr),
    wlmtk_window_t *window_ptr,
    wlmtk_titlebar_button_draw_t draw);

/**
 * Destroys the titlebar button.
 *
 * @param titlebar_button_ptr
 */
void wlmtk_titlebar_button_destroy(
    wlmtk_titlebar_button_t *titlebar_button_ptr);

/**
 * Sets the activation status (focussed / blurred) of the titlebar button.
 *
 * @param titlebar_button_ptr
 * @param activated
 */
void wlmtk_titlebar_button_set_activated(
    wlmtk_titlebar_button_t *titlebar_button_ptr,
    bool activated);

/**
 * Redraws the titlebar button for given textures, position and style.
 *
 * @param titlebar_button_ptr
 * @param focussed_gfxbuf_ptr
 * @param blurred_gfxbuf_ptr
 * @param position
 * @param style_ptr
 *
 * @return true on success.
 */
bool wlmtk_titlebar_button_redraw(
    wlmtk_titlebar_button_t *titlebar_button_ptr,
    bs_gfxbuf_t *focussed_gfxbuf_ptr,
    bs_gfxbuf_t *blurred_gfxbuf_ptr,
    int position,
    const wlmtk_titlebar_style_t *style_ptr);

/**
 * Returns the titlebar button's super element.
 *
 * @param titlebar_button_ptr
 *
 * @return Pointer to the superclass @ref wlmtk_element_t.
 */
wlmtk_element_t *wlmtk_titlebar_button_element(
    wlmtk_titlebar_button_t *titlebar_button_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_titlebar_button_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TITLEBAR_BUTTON_H__ */
/* == End of titlebar_button.h ============================================= */
