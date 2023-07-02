/* ========================================================================= */
/**
 * @file xwl.h
 *
 * Interface layer to Wlroots XWayland.
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
#ifndef __XWL_H__
#define __XWL_H__

/** Forward declaration: XWayland interface. */
typedef struct _wlmaker_xwl_t wlmaker_xwl_t;

#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the XWayland interface.
 *
 * @param server_ptr
 *
 * @return NULL on error, or a pointer to a @ref wlmaker_xwl_t. Must be free-d
 *     by calling @ref wlmaker_xwl_destroy.
 */
wlmaker_xwl_t *wlmaker_xwl_create(wlmaker_server_t *server_ptr);

/**
 * Destroys the XWayland interface.
 *
 * @param xwl_ptr
 */
void wlmaker_xwl_destroy(wlmaker_xwl_t *xwl_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XWL_H__ */
/* == End of xwl.h ========================================================= */
