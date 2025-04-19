/* ========================================================================= */
/**
 * @file test.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __WLMTK_TEST_H__
#define __WLMTK_TEST_H__

#include <libbase/libbase.h>  // IWYU pragma: keep
#include <stdbool.h>

/** Forward declaration. */
struct wlr_output;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Unit test comparator: Whether _box matches expected dimensions. */
#define WLMTK_TEST_VERIFY_WLRBOX_EQ(_test, _x, _y, _width, _height, _box) \
    do {                                                                \
        struct wlr_box __box = (_box);                                  \
        int __x = (_x), __y = (_y), __width = (_width), __height = (_height); \
        if (__x != __box.x || __y != __box.y ||                         \
            __height != __box.height || __width != __box.width) {       \
            bs_test_fail_at(                                            \
                (_test), __FILE__, __LINE__,                            \
                "Expecting {%d, %d, %d, %d}, got '%s' {%d, %d, %d, %d}", \
                __x, __y, __width, __height,                            \
                #_box, __box.x, __box.y, __box.width, __box.height);    \
        }                                                               \
    } while (false)

/** Initializes a struct wlr_output sufficient for testing. */
void wlmtk_test_wlr_output_init(struct wlr_output *wlr_output_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_TEST_H__ */
/* == End of test.h ================================================== */
