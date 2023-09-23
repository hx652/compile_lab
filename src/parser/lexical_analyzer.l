%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines = 1;
int pos_start = 1;
int pos_end = 1;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}

letter [a-zA-Z]
digit  [0-9]
/* Regular definition for later reference */

%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}
"-" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return SUB; }
"*" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return MUL; }
"/" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return DIV; }
 /* Operators */

"<" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return LT; }
"<=" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return LTE; }
">" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return GT; }
">=" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return GTE; }
"==" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return EQ; }
"!=" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return NEQ; }
 /* Relation operators */

"=" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return ASSIGN; }
 /* Assign */

";" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return SEMICOLON; }
"," { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return COMMA; }
"(" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return OPENPAREN; }
")" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return CLOSEPAREN; }
"[" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return OPENBRACKET; }
"]" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return CLOSEBRACKET; }
"{" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return OPENBRACE; }
"}" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return CLOSEBRACE; }
 /* Special characters */

"/*"([^*]|[*]+[^*/])*([*]*)"*/" {
    for(int i = 0; i < strlen(yytext); i++) {
        if(yytext[i] == '\n') {
            lines++;
            pos_end = 1;
        }
        else {
            pos_end++;
        }
    }
}
 /* Comments, note that they can't contain right side of the comment sign */

"else" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return ELSE; }
"if" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return IF; }
"int" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return INT; }
"return" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return RETURN; }
"void" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return VOID; }
"while" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return WHILE; }
"float" { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return FLOAT; }
 /* Keywords */

{digit}+ { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return INTEGERNUM; }
{digit}+\.|{digit}*\.{digit}+ { pos_start = pos_end; pos_end += yyleng; pass_node(yytext); return FLOATNUM; }
 /* Numbers */

{letter}({letter}|{digit})* {
    for(int i = 0; i < strlen(yytext); i++){
        /* printf("[Debug] Checking if identifier contains numbers\n"); */
        if('0' <= yytext[i] && yytext[i] <= '9') {
            /* Identifiers can't contain number */
            return ERROR;
        }
    }
    pos_start = pos_end;
    pos_end += yyleng;
    pass_node(yytext);
    return ID;
}
 /* Identifier */

[\n] { lines++; pos_end = 1; }
 /* Newline */

[\t ]+ { pos_end += yyleng; }
 /* Whitespace */

. { pos_start = pos_end; pos_end++; return ERROR; }

 /****请在此补全所有flex的模式与动作  end******/
%%
