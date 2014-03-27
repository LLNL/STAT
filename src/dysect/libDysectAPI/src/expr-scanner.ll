%option noyywrap 
%{

#include <stdio.h>
#include <DysectAPI.h>
#include <libDysectAPI/src/expr-parser.tab.hh>

extern int yyerror(const char *p, ...);

using namespace DysectAPI;

%}

%option bison-bridge 

%%

[ \t\n] {}
[_a-zA-Z][a-zA-Z0-9_]*  { yylval->str = strdup(yytext); return IDENTIFIER; }
-?[0-9]+                { yylval->ival = atol(yytext); return IVAL; }

"=="                    { return EQ_OP; }

"!="                    { return NE_OP; }


">="                    { return GE_OP; }

"<="                    { return LE_OP; }

"&&"                    { return AND_OP; }

"||"                    { return OR_OP; }


.                       { }

%%
