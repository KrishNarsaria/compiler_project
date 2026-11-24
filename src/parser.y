/* ============================================================
 * parser.y - Syntax analyzer and AST builder for the C++ subset.
 *
 * The grammar constructs an abstract syntax tree; code generation
 * is performed by codegen.cpp once the whole program has parsed.
 * ============================================================ */

%code requires {
#include "ast.h"
}

%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "ast.h"

int  yylex(void);
void yyerror(const char *msg);
extern char *yytext;
extern FILE *yyin;
extern int   line_num;

int generate_ir(const string &infile, const string &outfile, bool printIR);
extern string g_ir_text;
bool generate_asm(const string &ir, const string &outfile, bool runtimeMode);

static long long g_enum_next = 0;

static long long evalDim(Node *e, int line)
{
    CVal c = constEval(e);
    if (!c.ok || c.isf || c.i < 0) {
        report_error("SEMANTIC", line, "array size must be a non-negative integer constant");
        return 1;
    }
    return c.i;
}

static void decodePtr(int code, Decl *d)
{
    d->ptr = code % 1000;
    d->ref = code >= 1000;
}

static Node *makeLambda(vector<Node *> *caps, ParamList *pl, Type *ret, int ptrCode, Node *body, int line)
{
    Node *n = mkNode(N_LAMBDA, line);
    if (caps) { n->extra = *caps; delete caps; }
    if (pl)   { n->params = pl->v; delete pl; }
    n->ty = ret;
    for (int i = 0; ret && i < ptrCode % 1000; i++) n->ty = mkPtr(n->ty);
    n->kids.push_back(body);
    return n;
}

static void fixVoidParam(ParamList *p)
{
    if (p->v.size() == 1 && p->v[0].ty && p->v[0].ty->kind == T_VOID && p->v[0].name.empty())
        p->v.clear();
}
%}

%define parse.error verbose
%expect 0
%locations

%union {
    long long  ival;
    double     fval;
    char      *sval;
    std::string *sstr;
    int        cnt;
    Node      *node;
    vector<Node *>        *nlist;
    vector<long long>     *dims;
    vector<Decl *>        *dlist;
    vector<InitDecl>      *idl;
    InitDecl              *idecl;
    ParamList             *plist;
    Param                 *param;
    Type      *type;
    Spec      *spec;
    Decl      *decl;
}

%token <sval> IDENTIFIER TYPENAME CLASSQ CTORNAME
%token <sstr> STRING_LIT
%destructor { delete $$; } <sstr>
%token <ival> INT_LIT LONG_LIT CHAR_LIT
%token <fval> FLOAT_LIT FLOATF_LIT

%token INT CHAR SHORT LONG FLOAT DOUBLE VOID BOOL SIGNED UNSIGNED
%token CONST STATIC EXTERN TYPEDEF STRUCT UNION ENUM INLINE
%token CLASS PUBLIC PRIVATE PROTECTED AUTO VA_ARG DELETE_ARR
%token IF ELSE WHILE DO FOR UNTIL SWITCH CASE DEFAULT BREAK CONTINUE RETURN GOTO
%token SIZEOF NEW DELETE TRUE FALSE NULLPTR USING NAMESPACE COUT CIN ENDL
%token INC DEC EQ NE LE GE ANDAND OROR LSHIFT RSHIFT ARROW ELLIPSIS
%token ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN

%precedence LOW_TAG
%precedence PTR_REDUCE
%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN AND_ASSIGN OR_ASSIGN XOR_ASSIGN SHL_ASSIGN SHR_ASSIGN
%right '?' ':'
%left  OROR
%left  ANDAND
%left  '|'
%left  '^'
%left  '&'
%left  EQ NE
%left  '<' '>' LE GE
%left  LSHIFT RSHIFT
%left  '+' '-'
%left  '*' '/' '%'
%right UNARY
%precedence LOW_NEW
%left  INC DEC '[' ']' '.' ARROW
%precedence LOWER_THAN_ELSE
%precedence ELSE

%type <node>  expr comma_expr opt_comma_expr stmt compound_stmt block_item
%type <node>  postfix_expr primary_expr
%type <node>  declaration for_init initializer function_definition
%type <nlist> block_items arg_list arg_list_opt init_items
%type <spec>  decl_specs decl_spec
%type <type>  type_name struct_spec enum_spec opt_base simple_type
%type <nlist> capture_list ctor_init_opt ctor_inits
%type <node>  capture ctor_init lambda
%type <cnt>   access_opt
%type <decl>  declarator
%type <idecl> init_declarator
%type <idl>   init_declarator_list
%type <plist> param_list param_list_opt
%type <param> param
%type <cnt>   ptr_opt ptr_list
%type <dims>  dims_opt
%type <dlist> member_decl_list
%type <sval>  tag_name
%type <sstr>  string_lit

%start program

%%

program
    : %empty
    | program external
    ;

external
    : function_definition               { g_program.push_back($1); }
    | declaration                       { for (size_t k = 0; k < $1->kids.size(); k++) g_program.push_back($1->kids[k]); }
    | USING NAMESPACE IDENTIFIER ';'    { free($3); }
    | ';'
    | error ';'                         { yyerrok; }
    | error '}'                         { yyerrok; }
    ;

function_definition
    : decl_specs declarator compound_stmt
        { $$ = makeFunction($1, $2, $3, @2.first_line); }
    | decl_specs ptr_opt CLASSQ IDENTIFIER '(' param_list_opt ')' compound_stmt
        { $$ = makeOuterMethod($1, $2, $3, $4, $6, $8, @4.first_line); free($3); free($4); }
    | decl_specs ptr_opt CLASSQ IDENTIFIER '(' param_list_opt ')' CONST compound_stmt
        { $$ = makeOuterMethod($1, $2, $3, $4, $6, $9, @4.first_line); free($3); free($4); }
    | CLASSQ TYPENAME '(' param_list_opt ')' ctor_init_opt compound_stmt
        { $$ = makeOuterCtor($1, $4, $6, $7, @1.first_line); free($1); free($2); }
    | CLASSQ '~' TYPENAME '(' ')' compound_stmt
        { $$ = makeOuterDtor($1, $6, @1.first_line); free($1); free($3); }
    ;

ctor_init_opt
    : %empty                    { $$ = nullptr; }
    | ':' ctor_inits            { $$ = $2; }
    ;

ctor_inits
    : ctor_init                 { $$ = new vector<Node *>(); $$->push_back($1); }
    | ctor_inits ',' ctor_init  { $$ = $1; $$->push_back($3); }
    ;

ctor_init
    : IDENTIFIER '(' arg_list_opt ')'
        { $$ = makeCtorInit($1, $3, false, @1.first_line); free($1); }
    | TYPENAME '(' arg_list_opt ')'
        { $$ = makeCtorInit($1, $3, true, @1.first_line); free($1); }
    ;

/* ---------------- Declarations ---------------- */

declaration
    : decl_specs init_declarator_list ';'   { $$ = makeDeclaration($1, $2, @1.first_line); }
    | decl_specs ';'                        { $$ = makeDeclaration($1, nullptr, @1.first_line); }
    ;

decl_specs
    : decl_spec                 { $$ = $1; }
    | decl_specs decl_spec      { $$ = mergeSpec($1, $2); }
    ;

decl_spec
    : STATIC     { $$ = new Spec(); $$->is_static = true; }
    | EXTERN     { $$ = new Spec(); $$->is_extern = true; }
    | TYPEDEF    { $$ = new Spec(); $$->is_typedef = true; }
    | CONST      { $$ = new Spec(); $$->is_const = true; }
    | INLINE     { $$ = new Spec(); }
    | INT        { $$ = new Spec(); $$->base = T_INT; }
    | CHAR       { $$ = new Spec(); $$->base = T_CHAR; }
    | FLOAT      { $$ = new Spec(); $$->base = T_FLOAT; }
    | DOUBLE     { $$ = new Spec(); $$->base = T_DOUBLE; }
    | VOID       { $$ = new Spec(); $$->base = T_VOID; }
    | BOOL       { $$ = new Spec(); $$->base = T_BOOL; }
    | SHORT      { $$ = new Spec(); $$->is_short = true; }
    | LONG       { $$ = new Spec(); $$->longs = 1; }
    | SIGNED     { $$ = new Spec(); $$->is_signed = true; }
    | UNSIGNED   { $$ = new Spec(); $$->is_unsigned = true; }
    | AUTO       { $$ = new Spec(); $$->is_auto = true; }
    | TYPENAME   { $$ = new Spec(); $$->named = lookupTypeName($1, @1.first_line); free($1); }
    | struct_spec { $$ = new Spec(); $$->named = $1; }
    | enum_spec   { $$ = new Spec(); $$->named = $1; }
    ;

tag_name
    : IDENTIFIER   { $$ = $1; }
    | TYPENAME     { $$ = $1; }
    ;

opt_base
    : %empty                        { $$ = nullptr; }
    | ':' access_opt TYPENAME       { $$ = lookupTypeName($3, @3.first_line); free($3); }
    ;

access_opt
    : %empty        { $$ = 0; }
    | PUBLIC        { $$ = 0; }
    | PROTECTED     { $$ = 1; }
    | PRIVATE       { $$ = 2; }
    ;

struct_spec
    : STRUCT tag_name %prec LOW_TAG
        { $$ = refTag($2, false, @1.first_line); free($2); }
    | UNION tag_name %prec LOW_TAG
        { $$ = refTag($2, true, @1.first_line); free($2); }
    | CLASS tag_name %prec LOW_TAG
        { $$ = refTag($2, false, @1.first_line); free($2); }
    | STRUCT tag_name opt_base '{' { beginStruct($2, false, @1.first_line); setBase($3, @1.first_line); free($2); } member_list '}'
        { $$ = endStruct(); }
    | CLASS tag_name opt_base '{' { beginStruct($2, false, @1.first_line); setClassMode(true); setBase($3, @1.first_line); free($2); } member_list '}'
        { $$ = endStruct(); }
    | UNION tag_name '{' { beginStruct($2, true, @1.first_line); free($2); } member_list '}'
        { $$ = endStruct(); }
    | STRUCT '{' { beginStruct("", false, @1.first_line); } member_list '}'
        { $$ = endStruct(); }
    | UNION '{' { beginStruct("", true, @1.first_line); } member_list '}'
        { $$ = endStruct(); }
    ;

member_list
    : %empty
    | member_list member
    ;

member
    : decl_specs member_decl_list ';'   { addMembers($1, $2, @1.first_line); }
    | decl_specs declarator compound_stmt
        { addMethodDef($1, $2, $3, @2.first_line); }
    | decl_specs declarator CONST compound_stmt
        { addMethodDef($1, $2, $4, @2.first_line); }
    | CTORNAME '(' param_list_opt ')' ctor_init_opt compound_stmt
        { addCtor($1, $3, $5, $6, @1.first_line); free($1); }
    | '~' CTORNAME '(' ')' compound_stmt
        { addDtor($2, $5, @2.first_line); free($2); }
    | CTORNAME '(' param_list_opt ')' ';'
        { addCtor($1, $3, nullptr, nullptr, @1.first_line); free($1); }
    | '~' CTORNAME '(' ')' ';'
        { addDtor($2, nullptr, @2.first_line); free($2); }
    | PUBLIC ':'                        { setAccess(0); }
    | PROTECTED ':'                     { setAccess(1); }
    | PRIVATE ':'                       { setAccess(2); }
    | ';'
    ;

member_decl_list
    : declarator                        { $$ = new vector<Decl *>(); $$->push_back($1); }
    | member_decl_list ',' declarator   { $$ = $1; $$->push_back($3); }
    ;

enum_spec
    : ENUM tag_name
        { $$ = tInt(); free($2); }
    | ENUM tag_name '{' { g_enum_next = 0; } enumerator_list opt_comma '}'
        { g_tags[$2] = tInt(); free($2); $$ = tInt(); }
    | ENUM '{' { g_enum_next = 0; } enumerator_list opt_comma '}'
        { $$ = tInt(); }
    ;

enumerator_list
    : enumerator
    | enumerator_list ',' enumerator
    ;

enumerator
    : IDENTIFIER
        { g_enum_consts[$1] = g_enum_next++; free($1); }
    | IDENTIFIER '=' expr
        {
            CVal c = constEval($3);
            if (!c.ok || c.isf) {
                report_error("SEMANTIC", @1.first_line, "enumerator value must be an integer constant");
                c.i = g_enum_next;
            }
            g_enum_consts[$1] = c.i;
            g_enum_next = c.i + 1;
            free($1);
        }
    ;

opt_comma
    : %empty
    | ','
    ;

ptr_list
    : '*'               { $$ = 1; }
    | '&'               { $$ = 1000; }
    | ptr_list '*'      { $$ = $1 + 1; }
    | ptr_list '&'      { $$ = $1 + 1000; }
    | ptr_list CONST    { $$ = $1; }
    ;

ptr_opt
    : %empty %prec PTR_REDUCE   { $$ = 0; }
    | ptr_list %prec PTR_REDUCE   { $$ = $1; }
    ;

dims_opt
    : %empty               { $$ = new vector<long long>(); }
    | dims_opt '[' ']'          { $$ = $1; $$->push_back(-1); }
    | dims_opt '[' expr ']'     { $$ = $1; $$->push_back(evalDim($3, @2.first_line)); }
    ;

declarator
    : ptr_opt IDENTIFIER dims_opt
        {
            Decl *d = new Decl();
            d->name = $2; free($2);
            decodePtr($1, d);
            d->dims = *$3; delete $3;
            d->line = @2.first_line;
            $$ = d;
        }
    | ptr_opt '(' ptr_list IDENTIFIER dims_opt ')' '(' param_list_opt ')'
        {
            Decl *d = new Decl();
            d->name = $4; free($4);
            decodePtr($1, d);
            d->fptr = $3 % 1000;
            if (d->fptr < 1) d->fptr = 1;
            d->dims = *$5; delete $5;
            d->params = $8->v;
            d->variadic = $8->variadic;
            delete $8;
            d->line = @4.first_line;
            $$ = d;
        }
    | ptr_opt IDENTIFIER '(' arg_list ')'
        {
            Decl *d = new Decl();
            d->name = $2; free($2);
            decodePtr($1, d);
            d->has_ctor = true;
            d->cargs = *$4; delete $4;
            d->line = @2.first_line;
            $$ = d;
        }
    | ptr_opt IDENTIFIER '(' param_list_opt ')'
        {
            Decl *d = new Decl();
            d->name = $2; free($2);
            decodePtr($1, d);
            d->is_func = true;
            d->params = $4->v;
            d->variadic = $4->variadic;
            delete $4;
            d->line = @2.first_line;
            $$ = d;
        }
    ;

param_list_opt
    : %empty               { $$ = new ParamList(); }
    | param_list                { $$ = $1; fixVoidParam($$); }
    | param_list ',' ELLIPSIS   { $$ = $1; $$->variadic = true; }
    ;

param_list
    : param                     { $$ = new ParamList(); $$->v.push_back(*$1); delete $1; }
    | param_list ',' param      { $$ = $1; $$->v.push_back(*$3); delete $3; }
    ;

param
    : decl_specs ptr_opt
        {
            Param *p = new Param();
            p->ty = typeFromSpec($1, $2);
            p->ref = $2 >= 1000;
            $$ = p;
        }
    | decl_specs ptr_opt IDENTIFIER dims_opt
        {
            Param *p = new Param();
            Type *t = typeFromSpec($1, $2);
            for (int i = (int)$4->size() - 1; i >= 0; i--) t = mkArray(t, (*$4)[i]);
            if (t->kind == T_ARRAY) t = mkPtr(t->base);
            p->ty = t;
            p->name = $3; free($3);
            p->ref = $2 >= 1000;
            delete $4;
            $$ = p;
        }
    | decl_specs ptr_opt '(' ptr_list IDENTIFIER ')' '(' param_list_opt ')'
        {
            Param *p = new Param();
            Type *t = typeFromSpec($1, $2);
            vector<Type *> pt;
            for (size_t i = 0; i < $8->v.size(); i++) pt.push_back($8->v[i].ty);
            t = mkFunc(t, pt, $8->variadic);
            int stars = $4 % 1000;
            for (int i = 0; i < (stars < 1 ? 1 : stars); i++) t = mkPtr(t);
            p->ty = t;
            p->name = $5; free($5);
            delete $8;
            $$ = p;
        }
    ;

type_name
    : decl_specs ptr_opt        { $$ = typeFromSpec($1, $2); }
    ;

init_declarator_list
    : init_declarator                           { $$ = new vector<InitDecl>(); $$->push_back(*$1); delete $1; }
    | init_declarator_list ',' init_declarator  { $$ = $1; $$->push_back(*$3); delete $3; }
    ;

init_declarator
    : declarator                    { $$ = new InitDecl(); $$->d = $1; $$->init = nullptr; }
    | declarator '=' initializer    { $$ = new InitDecl(); $$->d = $1; $$->init = $3; }
    ;

initializer
    : expr                          { $$ = $1; }
    | '{' '}'                       { $$ = mkNode(N_INITLIST, @1.first_line); }
    | '{' init_items opt_comma '}'  {
            Node *n = mkNode(N_INITLIST, @1.first_line);
            n->kids = *$2; delete $2;
            $$ = n;
        }
    ;

init_items
    : initializer                   { $$ = new vector<Node *>(); $$->push_back($1); }
    | init_items ',' initializer    { $$ = $1; $$->push_back($3); }
    ;

/* ---------------- Statements ---------------- */

compound_stmt
    : '{' block_items '}'
        {
            Node *n = mkNode(N_BLOCK, @1.first_line);
            n->kids = *$2; delete $2;
            $$ = n;
        }
    ;

block_items
    : %empty               { $$ = new vector<Node *>(); }
    | block_items block_item    { $$ = $1; $$->push_back($2); }
    ;

block_item
    : declaration               { $$ = $1; }
    | stmt                      { $$ = $1; }
    ;

stmt
    : compound_stmt                         { $$ = $1; }
    | comma_expr ';'                        { $$ = mkK(N_EXPRSTMT, @1.first_line, "", {$1}); }
    | ';'                                   { $$ = mkNode(N_EMPTY, @1.first_line); }
    | IF '(' comma_expr ')' stmt %prec LOWER_THAN_ELSE
        { $$ = mkK(N_IF, @1.first_line, "", {$3, $5, nullptr}); }
    | IF '(' comma_expr ')' stmt ELSE stmt
        { $$ = mkK(N_IF, @1.first_line, "", {$3, $5, $7}); }
    | WHILE '(' comma_expr ')' stmt
        { $$ = mkK(N_WHILE, @1.first_line, "", {$3, $5}); }
    | DO stmt WHILE '(' comma_expr ')' ';'
        { $$ = mkK(N_DOWHILE, @1.first_line, "", {$2, $5}); }
    | DO stmt UNTIL '(' comma_expr ')' ';'
        { $$ = mkK(N_DOUNTIL, @1.first_line, "", {$2, $5}); }
    | UNTIL '(' comma_expr ')' stmt
        { $$ = mkK(N_UNTIL, @1.first_line, "", {$3, $5}); }
    | FOR '(' for_init opt_comma_expr ';' opt_comma_expr ')' stmt
        { $$ = mkK(N_FOR, @1.first_line, "", {$3, $4, $6, $8}); }
    | SWITCH '(' comma_expr ')' stmt
        { $$ = mkK(N_SWITCH, @1.first_line, "", {$3, $5}); }
    | CASE expr ':' block_item
        { $$ = mkK(N_CASE, @1.first_line, "", {$2, $4}); }
    | DEFAULT ':' block_item
        { $$ = mkK(N_DEFAULT, @1.first_line, "", {$3}); }
    | BREAK ';'                             { $$ = mkNode(N_BREAK, @1.first_line); }
    | CONTINUE ';'                          { $$ = mkNode(N_CONTINUE, @1.first_line); }
    | RETURN ';'                            { $$ = mkK(N_RETURN, @1.first_line, "", {nullptr}); }
    | RETURN comma_expr ';'                 { $$ = mkK(N_RETURN, @1.first_line, "", {$2}); }
    | GOTO IDENTIFIER ';'                   { $$ = mkNode(N_GOTO, @1.first_line, $2); free($2); }
    | IDENTIFIER ':' block_item             { $$ = mkK(N_LABEL, @1.first_line, $1, {$3}); free($1); }
    | DELETE expr ';'                       { $$ = mkK(N_DELETE, @1.first_line, "", {$2}); }
    | DELETE_ARR expr ';'                   { $$ = mkK(N_DELETE, @1.first_line, "", {$2}); $$->flag = true; }
    | USING NAMESPACE IDENTIFIER ';'        { $$ = mkNode(N_EMPTY, @1.first_line); free($3); }
    | error ';'                             { yyerrok; $$ = mkNode(N_EMPTY, @1.first_line); }
    ;

for_init
    : declaration                           { $$ = $1; }
    | comma_expr ';'                        { $$ = mkK(N_EXPRSTMT, @1.first_line, "", {$1}); }
    | ';'                                   { $$ = nullptr; }
    ;

opt_comma_expr
    : %empty                           { $$ = nullptr; }
    | comma_expr                            { $$ = $1; }
    ;

comma_expr
    : expr                                  { $$ = $1; }
    | comma_expr ',' expr                   { $$ = mkK(N_COMMA, @2.first_line, "", {$1, $3}); }
    ;

/* ---------------- Expressions ---------------- */

lambda
    : '[' capture_list ']' '(' param_list_opt ')' compound_stmt
        { $$ = makeLambda($2, $5, nullptr, 0, $7, @1.first_line); }
    | '[' ']' '(' param_list_opt ')' compound_stmt
        { $$ = makeLambda(nullptr, $4, nullptr, 0, $6, @1.first_line); }
    | '[' capture_list ']' '(' param_list_opt ')' ARROW simple_type ptr_opt compound_stmt
        { $$ = makeLambda($2, $5, $8, $9, $10, @1.first_line); }
    | '[' ']' '(' param_list_opt ')' ARROW simple_type ptr_opt compound_stmt
        { $$ = makeLambda(nullptr, $4, $7, $8, $9, @1.first_line); }
    | '[' capture_list ']' compound_stmt
        { $$ = makeLambda($2, nullptr, nullptr, 0, $4, @1.first_line); }
    ;

simple_type
    : INT       { $$ = tInt(); }
    | CHAR      { $$ = tChar(); }
    | FLOAT     { $$ = tFloat(); }
    | DOUBLE    { $$ = tDouble(); }
    | VOID      { $$ = tVoid(); }
    | BOOL      { $$ = tBool(); }
    | LONG      { $$ = tLong(); }
    | SHORT     { $$ = tShort(); }
    | TYPENAME  { $$ = lookupTypeName($1, @1.first_line); free($1); }
    ;

capture_list
    : capture                   { $$ = new vector<Node *>(); $$->push_back($1); }
    | capture_list ',' capture  { $$ = $1; $$->push_back($3); }
    ;

capture
    : '='                       { $$ = mkNode(N_ID, @1.first_line, "="); }
    | '&'                       { $$ = mkNode(N_ID, @1.first_line, "&"); }
    | IDENTIFIER                { $$ = mkNode(N_ID, @1.first_line, $1); free($1); }
    | '&' IDENTIFIER            { $$ = mkNode(N_ID, @1.first_line, $2); $$->flag = true; free($2); }
    ;

string_lit
    : STRING_LIT                { $$ = $1; }
    | string_lit STRING_LIT     {
            *$1 += *$2;
            delete $2;
            $$ = $1;
        }
    ;

arg_list_opt
    : %empty               { $$ = new vector<Node *>(); }
    | arg_list                  { $$ = $1; }
    ;

arg_list
    : expr                      { $$ = new vector<Node *>(); $$->push_back($1); }
    | arg_list ',' expr         { $$ = $1; $$->push_back($3); }
    ;

expr
    : postfix_expr              { $$ = $1; }
    | INC expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "++", {$2}); }
    | DEC expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "--", {$2}); }
    | '-' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "-", {$2}); }
    | '+' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "+", {$2}); }
    | '!' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "!", {$2}); }
    | '~' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "~", {$2}); }
    | '*' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "*", {$2}); }
    | '&' expr %prec UNARY      { $$ = mkK(N_UN, @1.first_line, "&", {$2}); }
    | SIZEOF expr %prec UNARY   { $$ = mkK(N_SIZEOF_E, @1.first_line, "", {$2}); }
    | SIZEOF '(' type_name ')' %prec UNARY
        { $$ = mkNode(N_SIZEOF_T, @1.first_line); $$->ty = $3; }
    | '(' type_name ')' expr %prec UNARY
        { $$ = mkK(N_CAST, @1.first_line, "", {$4}); $$->ty = $2; }
    | NEW type_name %prec LOW_NEW
        { $$ = mkK(N_NEW, @1.first_line, "", {nullptr, nullptr}); $$->ty = $2; }
    | NEW type_name '[' expr ']'
        { $$ = mkK(N_NEW, @1.first_line, "", {$4, nullptr}); $$->ty = $2; $$->flag = true; }
    | NEW type_name '(' arg_list_opt ')'
        {
            Node *first = $4->empty() ? nullptr : (*$4)[0];
            $$ = mkK(N_NEW, @1.first_line, "", {nullptr, first});
            $$->ty = $2;
            $$->extra = *$4;
            delete $4;
        }
    | expr '+' expr             { $$ = mkK(N_BIN, @2.first_line, "+", {$1, $3}); }
    | expr '-' expr             { $$ = mkK(N_BIN, @2.first_line, "-", {$1, $3}); }
    | expr '*' expr             { $$ = mkK(N_BIN, @2.first_line, "*", {$1, $3}); }
    | expr '/' expr             { $$ = mkK(N_BIN, @2.first_line, "/", {$1, $3}); }
    | expr '%' expr             { $$ = mkK(N_BIN, @2.first_line, "%", {$1, $3}); }
    | expr '&' expr             { $$ = mkK(N_BIN, @2.first_line, "&", {$1, $3}); }
    | expr '|' expr             { $$ = mkK(N_BIN, @2.first_line, "|", {$1, $3}); }
    | expr '^' expr             { $$ = mkK(N_BIN, @2.first_line, "^", {$1, $3}); }
    | expr LSHIFT expr          { $$ = mkK(N_BIN, @2.first_line, "<<", {$1, $3}); }
    | expr RSHIFT expr          { $$ = mkK(N_BIN, @2.first_line, ">>", {$1, $3}); }
    | expr '<' expr             { $$ = mkK(N_BIN, @2.first_line, "<", {$1, $3}); }
    | expr '>' expr             { $$ = mkK(N_BIN, @2.first_line, ">", {$1, $3}); }
    | expr LE expr              { $$ = mkK(N_BIN, @2.first_line, "<=", {$1, $3}); }
    | expr GE expr              { $$ = mkK(N_BIN, @2.first_line, ">=", {$1, $3}); }
    | expr EQ expr              { $$ = mkK(N_BIN, @2.first_line, "==", {$1, $3}); }
    | expr NE expr              { $$ = mkK(N_BIN, @2.first_line, "!=", {$1, $3}); }
    | expr ANDAND expr          { $$ = mkK(N_BIN, @2.first_line, "&&", {$1, $3}); }
    | expr OROR expr            { $$ = mkK(N_BIN, @2.first_line, "||", {$1, $3}); }
    | expr '?' comma_expr ':' expr
        { $$ = mkK(N_COND, @2.first_line, "", {$1, $3, $5}); }
    | expr '=' expr             { $$ = mkK(N_ASSIGN, @2.first_line, "=", {$1, $3}); }
    | expr ADD_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "+=", {$1, $3}); }
    | expr SUB_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "-=", {$1, $3}); }
    | expr MUL_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "*=", {$1, $3}); }
    | expr DIV_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "/=", {$1, $3}); }
    | expr MOD_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "%=", {$1, $3}); }
    | expr AND_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "&=", {$1, $3}); }
    | expr OR_ASSIGN expr       { $$ = mkK(N_ASSIGN, @2.first_line, "|=", {$1, $3}); }
    | expr XOR_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "^=", {$1, $3}); }
    | expr SHL_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, "<<=", {$1, $3}); }
    | expr SHR_ASSIGN expr      { $$ = mkK(N_ASSIGN, @2.first_line, ">>=", {$1, $3}); }
    ;

postfix_expr
    : primary_expr
    | postfix_expr '[' comma_expr ']'   { $$ = mkK(N_INDEX, @2.first_line, "", {$1, $3}); }
    | postfix_expr '(' arg_list_opt ')'
        {
            if ($1->kind == N_ID) {
                Node *c = mkNode(N_CALL, @2.first_line, $1->s);
                c->kids = *$3; delete $3;
                $$ = c;
            } else {
                Node *c = mkK(N_ICALL, @2.first_line, "", {$1});
                for (size_t i = 0; i < $3->size(); i++) c->kids.push_back((*$3)[i]);
                delete $3;
                $$ = c;
            }
        }
    | postfix_expr '.' IDENTIFIER       { $$ = mkK(N_MEMBER, @2.first_line, $3, {$1}); free($3); }
    | postfix_expr ARROW IDENTIFIER     { $$ = mkK(N_MEMBER, @2.first_line, $3, {$1}); $$->flag = true; free($3); }
    | postfix_expr INC                  { $$ = mkK(N_UN, @2.first_line, "p++", {$1}); }
    | postfix_expr DEC                  { $$ = mkK(N_UN, @2.first_line, "p--", {$1}); }
    ;

primary_expr
    : INT_LIT                   { $$ = mkNode(N_INT, @1.first_line); $$->iv = $1; }
    | LONG_LIT                  { $$ = mkNode(N_INT, @1.first_line); $$->iv = $1; $$->s = "L"; }
    | FLOAT_LIT                 { $$ = mkNode(N_FLOAT, @1.first_line); $$->dv = $1; }
    | FLOATF_LIT                { $$ = mkNode(N_FLOAT, @1.first_line); $$->dv = $1; $$->s = "f"; }
    | CHAR_LIT                  { $$ = mkNode(N_CHAR, @1.first_line); $$->iv = $1; }
    | string_lit                { $$ = mkNode(N_STR, @1.first_line, *$1); delete $1; }
    | TRUE                      { $$ = mkNode(N_BOOL, @1.first_line); $$->iv = 1; }
    | FALSE                     { $$ = mkNode(N_BOOL, @1.first_line); $$->iv = 0; }
    | NULLPTR                   { $$ = mkNode(N_NULL, @1.first_line); }
    | IDENTIFIER                { $$ = mkNode(N_ID, @1.first_line, $1); free($1); }
    | COUT                      { $$ = mkNode(N_COUT, @1.first_line); }
    | CIN                       { $$ = mkNode(N_CIN, @1.first_line); }
    | ENDL                      { $$ = mkNode(N_ENDL, @1.first_line); }
    | '(' comma_expr ')'        { $$ = $2; }
    | VA_ARG '(' expr ',' type_name ')'
        { $$ = mkK(N_VAARG, @1.first_line, "", {$3}); $$->ty = $5; }
    | lambda                    { $$ = $1; }
    ;

%%

void yyerror(const char *msg)
{
    std::string m(msg);
    const std::string pre = "syntax error, ";
    if (m.compare(0, pre.size(), pre) == 0)
        m = m.substr(pre.size());
    else if (m == "syntax error")
        m = "unexpected token";
    report_error("SYNTAX", yylloc.first_line, m + " (near '" + std::string(yytext ? yytext : "") + "')");
}

static bool is_directory(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static string default_output(const string &in)
{
    string base = in;
    size_t slash = base.find_last_of("/\\");
    if (slash != string::npos) base = base.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != string::npos) base = base.substr(0, dot);
    mkdir("output", 0755);
    return "output/" + base + ".s";
}

/* Command-line driver: parse, generate IR, translate to assembly and write the output file. */
int main(int argc, char **argv)
{
    string input, output;
    bool printIR = false, keepIR = false, runtimeMode = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-p" || a == "--print") printIR = true;
        else if (a == "-ir" || a == "--ir") keepIR = true;
        else if (a == "--runtime") runtimeMode = true;
        else if (input.empty()) input = a;
        else if (output.empty()) output = a;
    }
    if (input.empty()) {
        fprintf(stderr, "Usage: %s [-ir] [-p] <source-file> [output.s]\n", argv[0]);
        return 2;
    }
    if (is_directory(input.c_str())) {
        fprintf(stderr, "Error: '%s' is a directory, not a source file\n", input.c_str());
        return 2;
    }
    FILE *f = fopen(input.c_str(), "r");
    if (!f) {
        perror(input.c_str());
        return 2;
    }
    if (output.empty()) output = default_output(input);

    yyin = f;
    init_builtin_types();
    int rc = yyparse();
    fclose(f);

    if (rc != 0 || g_lex_errors > 0 || g_syn_errors > 0) {
        print_error_block(stdout);
        printf("\n>> %d lexical error(s), %d syntax error(s). No code generated.\n",
               g_lex_errors, g_syn_errors);
        remove(output.c_str());
        return 1;
    }

    string irPath;
    if (keepIR) {
        irPath = output;
        size_t dot = irPath.find_last_of('.');
        irPath = (dot == string::npos ? irPath : irPath.substr(0, dot)) + ".ir";
    }
    int sem = generate_ir(input, irPath, printIR);
    if (sem > 0) {
        print_error_block(stdout);
        printf("\n>> 0 lexical error(s), 0 syntax error(s), %d semantic error(s). No code generated.\n", sem);
        remove(output.c_str());
        return 1;
    }
    if (!generate_asm(g_ir_text, output, runtimeMode)) return 1;
    printf(">> Assembly written to %s\n", output.c_str());
    return 0;
}
