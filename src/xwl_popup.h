/* ========================================================================= */
/**
 * @file xwl_popup.h
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
#ifndef __XWL_POPUP_H__
#define __XWL_POPUP_H__
#if defined(WLMAKER_HAVE_XWAYLAND)

#include "xwl_content.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of an XWayland child window. */
typedef struct _wlmaker_xwl_popup_t wlmaker_xwl_popup_t;

/**
 * Creates a XWayland popup from `xwl_content_ptr`.
 *
 * @param xwl_content_ptr
 *
 * @return A pointer to the created @ref wlmaker_xwl_popup_t or NULL on error.
 */
wlmaker_xwl_popup_t *wlmaker_xwl_popup_create(
    wlmaker_xwl_content_t *xwl_content_ptr);

/**
 * Destroys the XWayland popup.
 *
 * @param xwl_popup_ptr
 */
void wlmaker_xwl_popup_destroy(wlmaker_xwl_popup_t *xwl_popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
#endif /* __XWL_POPUP_H__ */
/* == End of xwl_popup.h =================================================== */
