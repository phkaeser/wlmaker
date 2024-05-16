/* ========================================================================= */
/**
 * @file parser.y
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
#include "parser.h"
#include "scanner.h"
%}

/* == Bison declarations =================================================== */

%locations
%define parse.error verbose

%define api.pure full
%parse-param { void* scanner }

%lex-param { yyscan_t scanner }

%code provides {
    void yyerror(YYLTYPE *loc_ptr, void* scanner, const char* msg_ptr);
}

%token TK_LPAREN
%token TK_RPAREN
%token TK_LBRACE
%token TK_RBRACE
%token TK_COMMA
%token TK_EQUAL
%token TK_SEMICOLON

%%
/* == Grammar rules ======================================================== */

start:          TK_LPAREN TK_RPAREN
                ;

%%
/* == Epilogue ============================================================= */

#include <libbase/libbase.h>

void yyerror(YYLTYPE *loc_ptr, void* scanner, const char* msg_ptr) {
    bs_log(BS_ERROR, "Parse error: %s, %p, %p", msg_ptr, loc_ptr, scanner);
}

/* == End of parser.y ====================================================== */
