%{
#include "codegen/output.hpp"
#include "ast/visitor.hpp"
#include "ast/nodes.hpp"
#include "parser.tab.h"
#include <iostream>
 
%}
%option yylineno
%option noyywrap


alldig            ([0-9])
digs              ([1-9])

letter           ([a-zA-Z])
letanddig      ([a-zA-Z0-9])
whitespace       ([ \t\n\r ])
%%
"void"          {return VOID;}
"int"           {return INT;}
"byte"          {return BYTE;}
"bool"          {return BOOL;}
"and"           {return AND;}
"or"            {return OR;}
"not"           {return NOT;}
"true"          {return TRUE;}
"false"         {return FALSE;}
"return"        {return RETURN;}
"if"            {return IF;}
"else"          {return ELSE;}
"while"         {return WHILE;}
"break"         {return BREAK;}
"continue"      {return CONTINUE;}
";"             {return SC;}
","             {return COMMA;}
"("             {return LPAREN;}
")"             {return RPAREN;}
"{"             {return LBRACE;}
"}"             {return RBRACE;}
"["             {return LBRACK;}
"]"             {return RBRACK;}
"="             {return ASSIGN;}
"=="            {return EQEQ;}
"!="            {return NOTEQ;}
"<"             {return LT;}
">"             {return GT;}
"<="            {return LET;}
">="            {return GET;}
"+"             {return ADD;}
"-"             {return SUB;}
"*"             {return MUL;}
"/"             {return DIV;}
"//"[^\r\n]*[\r|\n|\r\n]?   {/*ignore and according to chat, "//"[^\r\n]*[\r|\n|\r\n]? -- works better than //[^\r\n]*[\r|\n|\r\n]?*/}
{letter}{letanddig}*    {yylval=std::make_shared<ast::ID>(yytext); return ID;}
0|{digs}{alldig}*       {yylval=std::make_shared<ast::Num>(yytext); return NUM;}
0b|{digs}{alldig}*b     {yylval=std::make_shared<ast::NumB>(yytext); return NUM_B;}
\"([^\n\r\"\\]|\\[rnt\"\\])+\" {yylval=std::make_shared<ast::String>(yytext); return STRING;}
{whitespace} {/*ignore as written*/}

.                   {output::errorLex(yylineno);}
%%