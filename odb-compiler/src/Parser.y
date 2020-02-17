%code top
{
    #include "odbc/Parser.y.h"
    #include "odbc/Scanner.lex.h"
    #include "odbc/Driver.hpp"
    #include "odbc/ASTNode.hpp"
    #include <stdarg.h>

    void yyerror(YYLTYPE *locp, yyscan_t scanner, const char* msg, ...);

    #define driver (static_cast<odbc::Driver*>(yyget_extra(scanner)))
    #define error(x, ...) yyerror(yypushed_loc, scanner, x, __VA_ARGS__)

    using namespace odbc;
    using namespace ast;
}

%code requires
{
    #include <stdint.h>
    typedef void* yyscan_t;

    namespace odbc {
        class Driver;
        namespace ast {
            union node_t;
        }
    }
}

/*
 * This is the bison equivalent of Flex's %option reentrant, in the sense that it also makes formerly global
 * variables into local ones. Unlike the lexer, there is no state structure for Bison. All the formerly global
 * variables become local to the yyparse() method. Which really begs the question: why were they ever global?
 * Although it is similar in nature to Flex's %option reentrant, this is truly the counterpart of
 * Flex's %option bison-bridge. Adding this declaration is what causes Bison to invoke yylval(YYSTYPE*) instead
 * of yylval(void), which is the same change that %option bison-bridge does in Flex.
 */
%define api.pure full

/*
 * As far as the grammar file goes, this is the only change needed to tell Bison to switch to a push-parser
 * interface instead of a pull-parser interface. Bison has the capability to generate both, but that is a
 * far more advanced case not covered here.
 */
%define api.push-pull push

/*
 * These two options are related to Flex's %option reentrant, These options add an argument to
 * each of the yylex() call and the yyparse() method, respectively.
 * Since the %option reentrant in Flex added an argument to make the yylex(void) method into yylex(yyscan_t),
 * Bison must be told to pass that new argument when it invokes the lexer. This is what the %lex-param declaration does.
 * How Bison obtains an instance of yyscan_t is up to you, but the most sensible way is to pass it into the
 * yyparse(void) method, making the  new signature yyparse(yyscan_t). This is what the %parse-param does.
 */
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}

%locations
%define parse.trace
%define parse.error verbose

/* This is the union that will become known as YYSTYPE in the generated code */
%union {
    bool boolean_value;
    int32_t integer_value;
    double float_value;
    char* string_literal;
    char* symbol;

    odbc::ast::node_t* node;
}

%define api.token.prefix {TOK_}

/* Define the semantic types of our grammar. %token for TERMINALS and %type for non_terminals */
%token END 0 "end of file"
%token TERM

%token CONSTANT

%token<boolean_value> BOOLEAN "boolean";
%token<integer_value> INTEGER "integer";
%token<float_value> FLOAT "float";
%token<string_literal> STRING_LITERAL "string";

%token ADD SUB MUL DIV POW MOD LB RB COMMA INC DEC;
%token BSHL BSHR BOR BAND BXOR BNOT;
%token LT GT LE GE NE EQ OR AND NOT;

%token IF THEN ELSE ELSEIF NO_ELSE ENDIF
%token WHILE ENDWHILE REPEAT UNTIL DO LOOP
%token FOR TO STEP NEXT
%token FUNCTION EXITFUNCTION ENDFUNCTION
%token GOSUB RETURN
%token DIM GLOBAL LOCAL AS

%token<symbol> SYMBOL;
%token DOLLAR HASH NO_SYMBOL_TYPE;

%type<node> stmnts;
%type<node> stmnt;
%type<node> constant_declaration;
%type<node> literal;
%type<node> expr;
%type<node> symbol;
%type<node> symbol_decl;
%type<node> symbol_ref;
%type<node> variable_assignment;
%type<node> function_call;
%type<node> dim_ref;
%type<node> dim_decl;
%type<node> conditional;
%type<node> conditional_singleline;
%type<node> conditional_begin;
%type<node> conditional_next;
%type<node> loop;
%type<node> loop_do;
%type<node> loop_while;
%type<node> loop_until;
%type<node> loop_for;
%type<node> loop_for_next;

/* precedence rules */
%nonassoc NO_ELSE
%nonassoc ELSE ELSEIF
%left COMMA
%left EQ
%left ADD SUB
%left MUL DIV
%left POW MOD
%right NOT
%left LB RB

%destructor { free($$); } <string_literal>
%destructor { free($$); } <symbol>
%destructor { freeNodeRecursive($$); } <node>

%start program

%%
program
  : stmnts                                       { driver->appendBlock($1); }
  | stmnts TERM                                  { driver->appendBlock($1); }
  | END
  ;
stmnts
  : stmnts TERM stmnt                            { $$ = appendStatementToBlock($1, $3); }
  | stmnt                                        { $$ = newBlock($1, nullptr); }
  ;
stmnt
  : variable_assignment                          { $$ = $1; }
  | constant_declaration                         { $$ = $1; }
  | dim_decl                                     { $$ = $1; }
  | function_call                                { $$ = $1; }
  | conditional                                  { $$ = $1; }
  | loop                                         { $$ = $1; }
  ;
variable_assignment
  : symbol_ref EQ expr                           { $$ = newAssignment($1, $3); }
  | dim_ref EQ expr                              { $$ = newAssignment($1, $3); }
  ;
expr
  : expr ADD expr                                { $$ = newOp($1, $3, OP_ADD); }
  | expr SUB expr                                { $$ = newOp($1, $3, OP_SUB); }
  | expr MUL expr                                { $$ = newOp($1, $3, OP_MUL); }
  | expr DIV expr                                { $$ = newOp($1, $3, OP_DIV); }
  | expr POW expr                                { $$ = newOp($1, $3, OP_POW); }
  | expr MOD expr                                { $$ = newOp($1, $3, OP_MOD); }
  | LB expr RB                                   { $$ = $2; }
  | expr COMMA expr                              { $$ = newOp($1, $3, OP_COMMA); }
  | expr EQ expr                                 { $$ = newOp($1, $3, OP_EQ); }
  | literal                                      { $$ = $1; }
  | symbol_ref                                   { $$ = $1; }
  | function_call                                { $$ = $1; }
  ;
constant_declaration
  : CONSTANT symbol_decl literal {
        $$ = $2;
        $2->symbol.literal = $3;
        switch ($3->literal.type)
        {
            case LT_BOOLEAN : $2->symbol.flag.datatype = SDT_BOOLEAN; break;
            case LT_INTEGER : $2->symbol.flag.datatype = SDT_INTEGER; break;
            case LT_FLOAT   : $2->symbol.flag.datatype = SDT_FLOAT; break;
            case LT_STRING  : $2->symbol.flag.datatype = SDT_STRING; break;
            default: break;
        }
    }
  ;
literal
  : BOOLEAN                                      { $$ = newBooleanLiteral($1); }
  | INTEGER                                      { $$ = newIntegerLiteral($1); }
  | FLOAT                                        { $$ = newFloatLiteral($1); }
  | STRING_LITERAL                               { $$ = newStringLiteral($1); free($1); }
  ;
function_call
  : symbol LB expr RB {
        $$ = $1;
        $$->symbol.flag.type = ST_FUNC;
        $$->symbol.arglist = $3;
    }
  | symbol LB RB {
        $$ = $1;
        $$->symbol.flag.type = ST_FUNC;
    }
  ;
dim_decl
  : DIM symbol LB expr RB {
        $$ = $2;
        $$->symbol.flag.type = ST_DIM;
        $$->symbol.flag.declaration = SD_DECL;
        $$->symbol.arglist = $4;
    }
  ;
dim_ref
  : symbol LB expr RB {
        $$ = $1;
        $$->symbol.flag.type = ST_DIM;
        $$->symbol.arglist = $3;
    }
  ;
symbol_decl
  : symbol {
        $$ = $1;
        $$->symbol.flag.declaration = SD_DECL;
    }
  ;
symbol_ref
  : symbol {
        $$ = $1;
    }
  ;
symbol
  : SYMBOL                                       { $$ = newSymbol($1, nullptr, nullptr, ST_UNKNOWN, SDT_UNKNOWN, SS_LOCAL, SD_REF); free($1); }
  | SYMBOL HASH                                  { $$ = newSymbol($1, nullptr, nullptr, ST_UNKNOWN, SDT_FLOAT, SS_LOCAL, SD_REF); free($1); }
  | SYMBOL DOLLAR                                { $$ = newSymbol($1, nullptr, nullptr, ST_UNKNOWN, SDT_STRING, SS_LOCAL, SD_REF); free($1); }
  ;
conditional
  : conditional_singleline                       { $$ = $1; }
  | conditional_begin                            { $$ = $1; }
  ;
conditional_singleline
  : IF expr THEN stmnt %prec NO_ELSE             { $$ = newBranch($2, $4, nullptr); }
  | IF expr THEN stmnt ELSE stmnt                { $$ = newBranch($2, $4, $6); }
  | IF expr THEN ELSE stmnt                      { $$ = newBranch($2, nullptr, $5); }
  ;
conditional_begin
  : IF expr TERM conditional_next                { $$ = newBranch($2, nullptr, $4); }
  | IF expr TERM stmnts TERM conditional_next    { $$ = newBranch($2, $4, $6); }
  ;
conditional_next
  : ENDIF                                        { $$ = nullptr; }
  | ELSE TERM stmnts TERM ENDIF                  { $$ = $3; }
  | ELSE TERM ENDIF                              { $$ = nullptr; }
  | ELSEIF expr TERM conditional_next            { $$ = newBranch($2, nullptr, $4); }
  | ELSEIF expr TERM stmnts TERM conditional_next { $$ = newBranch($2, $4, $6); }
  ;
loop
  : loop_do                                      { $$ = $1; }
  | loop_while                                   { $$ = $1; }
  | loop_until                                   { $$ = $1; }
  | loop_for                                     { $$ = $1; }
  ;
loop_do
  : DO TERM stmnts TERM LOOP                     { $$ = newLoop($3); }
  | DO TERM LOOP                                 { $$ = newLoop(nullptr); }
  ;
loop_while
  : WHILE expr TERM stmnts TERM ENDWHILE         { $$ = newLoopWhile($2, $4); }
  | WHILE expr TERM ENDWHILE                     { $$ = newLoopWhile($2, nullptr); }
  ;
loop_until
  : REPEAT TERM stmnts TERM UNTIL expr           { $$ = newLoopUntil($6, $3); }
  | REPEAT TERM UNTIL expr                       { $$ = newLoopUntil($4, nullptr); }
  ;
loop_for
  : FOR symbol_ref EQ expr TO expr STEP expr TERM stmnts TERM loop_for_next { $$ = newLoopFor($2, $4, $6, $8, $12, $10); }
  | FOR symbol_ref EQ expr TO expr STEP expr TERM loop_for_next             { $$ = newLoopFor($2, $4, $6, $8, $10, nullptr); }
  | FOR symbol_ref EQ expr TO expr TERM stmnts TERM loop_for_next           { $$ = newLoopFor($2, $4, $6, nullptr, $10, $8); }
  | FOR symbol_ref EQ expr TO expr TERM loop_for_next                       { $$ = newLoopFor($2, $4, $6, nullptr, $8, nullptr); }
  ;
loop_for_next
  : NEXT                                         { $$ = nullptr; }
  | NEXT symbol_ref                              { $$ = $2; }
  ;
%%

void yyerror(YYLTYPE *locp, yyscan_t scanner, const char* fmt, ...)
{
    va_list args;
    printf("Error: ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
