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
%parse-param { void* scanner } { wlmcfg_parser_context_t *ctx_ptr }

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
%token  <string> TK_STRING

%%
/* == Grammar rules ======================================================== */
/* See https://code.google.com/archive/p/networkpx/wikis/PlistSpec.wiki. */

start:          object
                ;

object:         string |
                TK_LPAREN TK_RPAREN |
                TK_LBRACE TK_RBRACE
                ;

string:         TK_STRING {
    wlmcfg_string_t *string_ptr = wlmcfg_string_create($1);
    ctx_ptr->top_object_ptr = wlmcfg_object_from_string(string_ptr);
 }
                ;

%%
/* == Epilogue ============================================================= */

#include <libbase/libbase.h>

int yyerror(
    YYLTYPE *loc_ptr,
    void* scanner,
    __UNUSED__ wlmcfg_parser_context_t *ctx_ptr,
    const char* msg_ptr)
{
    bs_log(BS_ERROR, "Parse error: %s, %p, %p", msg_ptr, loc_ptr, scanner);
    return -1;
}

/* == End of grammar.y ===================================================== */
