/* ========================================================================= */
/**
 * @file grammar.y
 *
 * See https://www.gnu.org/software/bison/manual/bison.html.
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

/* == Prologue ============================================================= */
%{
#include "grammar.h"
#include "analyzer.h"

#include <libbase/libbase.h>

#include "model.h"
%}

/* == Bison declarations =================================================== */

%union{
    char *string;
}

%locations
%define parse.error verbose

%define api.pure full
%parse-param { void* scanner }
%parse-param { wlmcfg_parser_context_t *ctx_ptr }
%lex-param { yyscan_t scanner }

%code requires {
#include "parser.h"
}

%code provides {
    extern int yyerror(
        YYLTYPE *loc_ptr,
        void* scanner,
        wlmcfg_parser_context_t *ctx_ptr,
        const char* msg_ptr);
}

%token TK_LPAREN
%token TK_RPAREN
%token TK_LBRACE
%token TK_RBRACE
%token TK_COMMA
%token TK_EQUAL
%token TK_SEMICOLON
%token <string> TK_STRING
%token <string> TK_QUOTED_STRING

%destructor { free($$); } <string>

%%
/* == Grammar rules ======================================================== */
/* See https://code.google.com/archive/p/networkpx/wikis/PlistSpec.wiki. */

start:          object
                ;

object:         string |
                dict |
                array |
                ;

string:         TK_STRING {
    wlmcfg_string_t *string_ptr = wlmcfg_string_create($1);
    free($1);
    bs_ptr_stack_push(&ctx_ptr->object_stack,
                      wlmcfg_object_from_string(string_ptr));
                } |
                TK_QUOTED_STRING {
    size_t len = strlen($1);
    BS_ASSERT(2 <= len);  // It is supposed to be quoted.
    $1[len - 1] = '\0';
    wlmcfg_string_t *string_ptr = wlmcfg_string_create($1 + 1);
    free($1);
    bs_ptr_stack_push(&ctx_ptr->object_stack,
                      wlmcfg_object_from_string(string_ptr));
                }
                ;

dict:           TK_LBRACE {
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_create();
    bs_ptr_stack_push(
        &ctx_ptr->object_stack,
        wlmcfg_object_from_dict(dict_ptr));
                } TK_RBRACE |
                TK_LBRACE {
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_create();
    bs_ptr_stack_push(
        &ctx_ptr->object_stack,
        wlmcfg_object_from_dict(dict_ptr));
                } kv_list TK_RBRACE
                ;

kv_list:        kv_list TK_SEMICOLON kv |
                kv
                ;

kv:             TK_STRING TK_EQUAL object {
    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
        bs_ptr_stack_peek(&ctx_ptr->object_stack, 1));
    wlmcfg_object_t *object_ptr = bs_ptr_stack_pop(&ctx_ptr->object_stack);
    bool rv = wlmcfg_dict_add(dict_ptr, $1, object_ptr);
    if (!rv) {
        // TODO(kaeser@gubbe.ch): Keep this as error in context.
        bs_log(BS_WARNING, "Failed wlmcfg_dict_add(%p, %s, %p)",
               dict_ptr, $1, object_ptr);
    }
    wlmcfg_object_unref(object_ptr);
    free($1);
    if (!rv) return -1;
                }
                ;

array:          TK_LPAREN {
    wlmcfg_array_t *array_ptr = wlmcfg_array_create();
    bs_ptr_stack_push(
        &ctx_ptr->object_stack,
        wlmcfg_object_from_array(array_ptr));
                } element_list TK_RPAREN;

element_list:   element |
                element_list TK_COMMA element;

element:        object {
    wlmcfg_array_t *array_ptr = wlmcfg_array_from_object(
        bs_ptr_stack_peek(&ctx_ptr->object_stack, 1));
    wlmcfg_object_t *object_ptr = bs_ptr_stack_pop(&ctx_ptr->object_stack);
    bool rv = wlmcfg_array_push_back(array_ptr, object_ptr);
    if (!rv) {
        // TODO(kaeser@gubbe.ch): Keep this as error in context.
        bs_log(BS_WARNING, "Failed wlmcfg_array_push_back(%p, %p)",
               array_ptr, object_ptr);
    }
    wlmcfg_object_unref(object_ptr);
                };

%%
/* == Epilogue ============================================================= */

#include <libbase/libbase.h>

int yyerror(
    YYLTYPE *loc_ptr,
    __UNUSED__ void* scanner,
    __UNUSED__ wlmcfg_parser_context_t *ctx_ptr,
    const char* msg_ptr)
{
    bs_log(BS_ERROR, "Parse error at %d,%d: %s",
           loc_ptr->first_line, loc_ptr->first_column,
           msg_ptr);
    return -1;
}

/* == End of grammar.y ===================================================== */
