/* ========================================================================= */
/**
 * @file wlm_graph_utildefines.h
 *
 * Utility macros for wlmaker graph dock-apps.
 *
 * @copyright
 * Copyright 2026 Google LLC
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
#ifndef WLM_GRAPH_UTILDEFINES_H
#define WLM_GRAPH_UTILDEFINES_H

/** Returns true if character is a decimal digit ('0'-'9'). */
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')

/** Returns the minimum of two values. */
#define MIN2(a, b) ((a) < (b) ? (a) : (b))

/** Returns the maximum of two values. */
#define MAX2(a, b) ((a) > (b) ? (a) : (b))

/** Clamps value up to a minimum (in-place). */
#define CLAMP_MIN(value, min) \
  do { \
    if ((value) < (min)) { \
      (value) = (min); \
    } \
  } while (0)

/** Clamps value down to a maximum (in-place). */
#define CLAMP_MAX(value, max) \
  do { \
    if ((value) > (max)) { \
      (value) = (max); \
    } \
  } while (0)

#endif /* WLM_GRAPH_UTILDEFINES_H */
