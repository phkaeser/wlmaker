/* ========================================================================= */
/**
 * @file analyzer.l
 *
 * See https://westes.github.io/flex/manual/.
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

%{
#include <libbase/libbase.h>

#include "grammar.h"
%}

/* == Definitions section ===================================================*/

%option batch
%option bison-bridge
%option bison-locations
%option never-interactive
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option reentrant
%option yylineno

delim               [[:blank:]\r\n]

/* == Rules section ======================================================== */
%%

{delim}*            { /* whitespace */ }

"//".*\n            { /* comment. */ }

[a-zA-A0-9_.$]+     { yylval->string = logged_strdup(yytext);
                      return TK_STRING; }
"("                 { return TK_LPAREN; }
")"                 { return TK_RPAREN; }
"{"                 { return TK_LBRACE; }
"}"                 { return TK_RBRACE; }
","                 { return TK_COMMA; }
"="                 { return TK_EQUAL; }
";"                 { return TK_SEMICOLON; }

.                   { bs_log(BS_ERROR, "Unexpected character '%s'.", yytext); }

%%
/* == User code section ==================================================== */


/* == End of analyzer.l ==================================================== */
