%{
    #define YYSTYPE KWSTYPE
    #define YYLTYPE KWLTYPE
    #define YY_USER_ACTION \
        yylloc->first_line = yylloc->last_line; \
        yylloc->first_column = yylloc->last_column; \
        for(int i = 0; yytext[i] != '\0'; i++) { \
            if(yytext[i] == '\n') { \
                yylloc->last_line++; \
                yylloc->last_column = 1; \
            } \
            else { \
                yylloc->last_column++; \
            } \
        }

    #include "odbc/parsers/keywords/Parser.y.h"
    #include "odbc/parsers/keywords/Scanner.hpp"
    #include "odbc/util/Str.hpp"

    #if defined(ODBC_VERBOSE_FLEX)
    #   define dbg(text) printf(text ": \"%s\"\n", yytext)
    #else
    #   define dbg(text)
    #endif
%}

%option nodefault
%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations
%option extra-type="odbc::kw::Driver*"
%option prefix="kw"

%%
"="                                   { dbg("delim"); return TOK_EQ; }
"("                                   { dbg("lb"); return TOK_LB; }
")"                                   { dbg("rb"); return TOK_RB; }
"["                                   { dbg("ls"); return TOK_LS; }
"]"                                   { dbg("rs"); return TOK_RS; }
","                                   { dbg("comma"); return TOK_COMMA; }
"|"                                   { dbg("comma"); return TOK_PIPE; }
[\t ]+                                { dbg("whitespace"); }
"\n"                                  { dbg("newline"); return TOK_NEWLINE; }
(?i:"no parameters")                  { dbg("no params"); return TOK_NO_PARAMS; }
[a-zA-Z0-9_/\\$# ]+\.html?            { dbg("help file"); yylval->string = odbc::newCStr(yytext); return TOK_HELPFILE; }
[a-zA-Z0-9_$#\- ]+                    { dbg("words"); yylval->string = odbc::newCStr(yytext); return TOK_WORDS; }
.                                     {}
%%
