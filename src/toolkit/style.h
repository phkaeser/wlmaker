/* ========================================================================= */
/**
 * @file style.h
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
#ifndef __STYLE_H__
#define __STYLE_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Specifies the type of coloring to use for the fill. */
typedef enum {
    /** Horizontal color gradient. */
    WLMAKER_STYLE_COLOR_SOLID,
    /** Horizontal color gradient. */
    WLMAKER_STYLE_COLOR_HGRADIENT,
    /** Diagonal color gradient, top-left to bottom-right. */
    WLMAKER_STYLE_COLOR_DGRADIENT
    // TODO(kaeser@gubbe.ch): Add VGRADIENT.
} wlmaker_style_fill_type_t;

/** Specifies the color for a solid fill. */
typedef struct {
    /** Color to start from, as ARGB 8888. Left, for the HGRADIENT type. */
    uint32_t                  color;
} wlmaker_style_color_solid_data_t;

/** Specifies the two colors to span a gradient between. */
typedef struct {
    /** Color to start from, as ARGB 8888. Left, for the HGRADIENT type. */
    uint32_t                  from;
    /** Color to end with, as ARGB 8888. Right, for the HGRADIENT type. */
    uint32_t                  to;
} wlmaker_style_color_gradient_data_t;

/** Specification for the fill of the titlebar. */
typedef struct {
    /** The type of fill to apply. */
    wlmaker_style_fill_type_t type;
    /** Parameters for the fill. */
    union {
        /** Solid color. */
        wlmaker_style_color_solid_data_t solid;
        /** Horizontal color gradient. */
        wlmaker_style_color_gradient_data_t hgradient;
        /** Diagonal color gradient. */
        wlmaker_style_color_gradient_data_t dgradient;
    } param;
} wlmaker_style_fill_t;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __STYLE_H__ */
/* == End of style.h ================================================== */
