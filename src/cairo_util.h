/* ========================================================================= */
/**
 * @file cairo_util.h
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
#ifndef __CAIRO_UTIL_H__
#define __CAIRO_UTIL_H__

#include <cairo.h>

#include <stdint.h>
#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a `cairo_t` with an ARGB32 surface.
 *
 * This is merely an utility function that ensures error handling is all done
 * well. Errors will be logged.
 *
 * @param width
 * @param height
 *
 * @return A `cairo_t` with a configured ARGB target surface, or NULL on error.
 * TODO(kaeser@gubbe.ch): Eliminate.
 */
cairo_t *wlmaker_cairo_util_create_with_surface(
    uint32_t width,
    uint32_t height);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CAIRO_UTIL_H__ */
/* == End of cairo_util.h ================================================== */
