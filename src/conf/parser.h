/* ========================================================================= */
/**
 * @file parser.h
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
#ifndef __WLMCFG_PARSER_H__
#define __WLMCFG_PARSER_H__

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Context for parser, to be used in the grammar file. */
typedef struct {
    /** Top-level dict. */
    wlmcfg_dict_t             *top_dict_ptr;

    /** Stack of objects, used throughout parsing. */
    bs_ptr_stack_t            *object_stack_ptr;
} wlmcfg_parser_context_t;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_PARSER_H__ */
/* == End of parser.h ====================================================== */
