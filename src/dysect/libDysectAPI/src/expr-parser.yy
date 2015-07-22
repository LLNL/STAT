%{
#include <LibDysectAPI.h>
#include <DysectAPI/Expr.h>
#include <expr-parser.h>
#include <stdarg.h>

int yyerror(const char *p, ...) {
  
  va_list args;
  va_start(args, p);
  vfprintf(stderr, p, args);
  va_end(args);
}

int yylex(YYSTYPE* type);

using namespace std;
using namespace DysectAPI;

extern ExprNode* parsedRoot;

%}

%union {
  char  *str;
  int   ival;
  DysectAPI::ExprNode *enode; 
}

%pure_parser

%token<ival>    IVAL;
%token<str>     IDENTIFIER;
%token          LE_OP GE_OP EQ_OP NE_OP AND_OP OR_OP

%type<enode>    expr_list expression;
%type<enode>    primary_expression postfix_expression unary_expression cast_expression multiplicative_expression;
%type<enode>    additive_expression shift_expression relational_expression equality_expression;
%type<enode>    and_expression exclusive_or_expression inclusive_or_expression;
%type<enode>    logical_and_expression logical_or_expression;

%start          expr_list;

%%

expr_list : expression {$$ = $1; parsedRoot = $1; };

//expr   : expr CMP expr { $$ = new ExprNode($1, $3, $2); }
////       | '(' expr CMP expr ')' { $$ = new ExprNode($2, $4, $3); }
//       | expr LCMP expr { $$ = new ExprNode($1, $3, $2); }
////       | LBIN expr { $$ = new ExprNode($2, DysectAPI::ExprNode::TestNot); }
//       | local_var { $$ = new ExprNode($1); }
//       | IVAL { $$ = new ExprNode($1); };


primary_expression
        : IDENTIFIER { $$ = new ExprNode(new TargetVar($1)); };
        | IVAL { $$ = new ExprNode($1); }
        | '(' expression ')' { $$ = $2; }
        ;

postfix_expression
        :  primary_expression { $$ = $1; }
//        |  postfix_expression '[' unary_expression ']' {}
//        |  postfix_expression '.' IDENTIFIER {}
//        |  postfix_expression PTR_OP IDENTIFIER {}
//        |  postfix_expression INC_OP {}
//        |  postfix_expression DEC_OP {}
        ;

unary_expression
        : postfix_expression { $$ = $1; }
//        | INC_OP unary_expression {}
//        | DEC_OP unary_expression {}
        | '!' cast_expression { $$ = new ExprNode($2, ExprNode::TestNot); }
        ;

//unary_operator
//        : '*' {}
//        : '&' {}
//        : '!' {}
//        : '~' {}
//        : '!' {}
        ;

cast_expression   
        :  unary_expression { $$ = $1; }
//        | '(' type_name ')' cast_expression {}
        ;

multiplicative_expression 
        : cast_expression { $$ = $1; }
//	      | multiplicative_expression '*' cast_expression
//	      | multiplicative_expression '/' cast_expression
//	      | multiplicative_expression '%' cast_expression 
        ;

additive_expression 
        : multiplicative_expression { $$ = $1; }
//	      | additive_expression '+' multiplicative_expression
//	      | additive_expression '-' multiplicative_expression 
        ;

shift_expression
	      : additive_expression { $$ = $1; }
//	      | shift_expression LEFT_OP additive_expression
//        | shift_expression RIGHT_OP additive_expression
	      ;

relational_expression
	      : shift_expression { $$ = $1; }
	      | relational_expression '<' shift_expression { $$ = new ExprNode($1, $3, ExprNode::TestLessThan); }
	      | relational_expression '>' shift_expression { $$ = new ExprNode($1, $3, ExprNode::TestGreaterThan); }
	      | relational_expression LE_OP shift_expression { $$ = new ExprNode($1, $3, ExprNode::TestLessThanEqual); }
	      | relational_expression GE_OP shift_expression { $$ = new ExprNode($1, $3, ExprNode::TestGreaterThanEqual); }
	      ;
          
equality_expression
	      : relational_expression { $$ = $1; }
	      | equality_expression EQ_OP relational_expression { $$ = new ExprNode($1, $3, ExprNode::TestEqual); }
	      | equality_expression NE_OP relational_expression { $$ = new ExprNode($1, $3, ExprNode::TestNotEqual); }
	      ;

and_expression
	      : equality_expression { $$ = $1; }
//	      | and_expression '&' equality_expression
	      ;

exclusive_or_expression
	      : and_expression { $$ = $1; }
//	      | exclusive_or_expression '^' and_expression
	      ;

inclusive_or_expression
	      : exclusive_or_expression { $$ = $1; }
//      	| inclusive_or_expression '|' exclusive_or_expression
	      ;

logical_and_expression
	      : inclusive_or_expression { $$ = $1; }
	      | logical_and_expression AND_OP inclusive_or_expression { $$ = new ExprNode($1, $3, ExprNode::TestAnd); }
	      ;

logical_or_expression
	      : logical_and_expression { $$ = $1; }
	      | logical_or_expression OR_OP logical_and_expression { $$ = new ExprNode($1, $3, ExprNode::TestOr); }
	      ;

expression
        : logical_or_expression { $$ = $1; }
        ;

%%
