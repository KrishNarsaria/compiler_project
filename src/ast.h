/* ============================================================
 * ast.h - Types, abstract syntax tree and shared declarations
 * for the intermediate code generator.
 * ============================================================ */
#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <initializer_list>

using std::string;
using std::vector;
using std::map;
using std::set;

/* ---------------- Types ---------------- */

enum TKind {
    T_VOID, T_BOOL, T_CHAR, T_SHORT, T_INT, T_LONG, T_FLOAT, T_DOUBLE,
    T_PTR, T_ARRAY, T_STRUCT, T_STREAM, T_FUNC
};

struct Type;

struct Field {
    string name;
    Type  *ty = nullptr;
    long long offset = 0;
    Type  *owner = nullptr;        /* class that declared the member     */
    int    access = 0;             /* 0 public, 1 protected, 2 private   */
};

struct Type {
    TKind kind;
    bool is_unsigned = false;
    Type *base = nullptr;          /* pointee or element type            */
    long long len = 0;             /* array length (-1 = unspecified)    */
    string tag;                    /* struct / union tag                 */
    bool is_union = false;
    bool complete = false;
    vector<Field> fields;
    long long size = 0;            /* struct / union size                */
    long long align = 1;
    Type *parent = nullptr;        /* base class (inheritance)           */
    bool is_class = false;
    vector<Type *> ptys;           /* T_FUNC: parameter types            */
    bool fvariadic = false;        /* T_FUNC: takes '...'                */
};

Type *mkBase(TKind k, bool uns = false);
Type *tVoid();
Type *tBool();
Type *tChar();
Type *tShort();
Type *tInt();
Type *tLong();
Type *tFloat();
Type *tDouble();
Type *tStream();
Type *mkPtr(Type *b);
Type *mkArray(Type *b, long long n);
Type *mkFunc(Type *ret, const vector<Type *> &params, bool variadic);
bool derivedFrom(Type *d, Type *b);

long long sizeOf(Type *t);
long long alignOf(Type *t);
bool isInteger(Type *t);
bool isFloat(Type *t);
bool isArith(Type *t);
bool isPointer(Type *t);
bool isPtrLike(Type *t);
bool sameType(Type *a, Type *b);
string typeStr(Type *t);
string declStr(Type *t, const string &name, bool ref = false);
const Field *findField(Type *st, const string &name);

/* ---------------- Declaration helpers ---------------- */

struct Spec {
    bool is_static = false, is_const = false, is_extern = false;
    bool is_typedef = false, is_unsigned = false, is_signed = false;
    bool is_short = false;
    bool is_auto = false;
    int  longs = 0;
    int  base = -1;                /* a TKind, or -1 if none given       */
    Type *named = nullptr;         /* typedef name / struct / enum type  */
};

struct Param {
    Type  *ty = nullptr;
    string name;
    bool   ref = false;
};

struct ParamList {
    vector<Param> v;
    bool variadic = false;
};

struct Node;

struct Decl {
    string name;
    int    ptr = 0;
    bool   ref = false;
    vector<long long> dims;
    int    fptr = 0;               /* >0: function-pointer declarator    */
    bool   has_ctor = false;       /* Foo f(args);                       */
    vector<Node *> cargs;
    bool   is_func = false;
    vector<Param> params;
    bool   variadic = false;
    int    line = 0;
};

/* ---------------- AST ---------------- */

enum NKind {
    /* expressions */
    N_INT, N_FLOAT, N_CHAR, N_STR, N_BOOL, N_NULL, N_ID,
    N_BIN, N_UN, N_ASSIGN, N_COND, N_COMMA, N_CALL, N_INDEX, N_MEMBER,
    N_CAST, N_SIZEOF_T, N_SIZEOF_E, N_NEW, N_COUT, N_CIN, N_ENDL,
    N_MCALL, N_ICALL, N_LAMBDA, N_VAARG,
    /* statements */
    N_BLOCK, N_EXPRSTMT, N_DECL, N_DECLLIST, N_IF, N_WHILE, N_DOWHILE,
    N_FOR, N_UNTIL, N_DOUNTIL, N_SWITCH, N_CASE, N_DEFAULT, N_BREAK,
    N_CONTINUE, N_RETURN, N_GOTO, N_LABEL, N_DELETE, N_EMPTY, N_INITLIST,
    N_FUNC
};

struct Node {
    NKind kind;
    int   line = 0;
    string s;                      /* operator, identifier, literal text */
    long long iv = 0;
    double dv = 0;
    Type *ty = nullptr;
    vector<Node *> kids;
    vector<Param> params;          /* N_FUNC only                        */
    bool variadic = false;
    bool is_static = false, is_const = false, is_ref = false, flag = false;
    /* classes, constructors, lambdas */
    Type *cls = nullptr;           /* owning class of a method           */
    vector<Node *> extra;          /* ctor args / new args / captures    */
    bool is_auto = false, ctor_init = false;
    bool is_ctor = false, is_dtor = false, has_binit = false;
    int  access = 0;
};

struct InitDecl {
    Decl *d;
    Node *init;
};

Node *mkNode(NKind k, int line, const string &s = "");
Node *mkK(NKind k, int line, const string &s, std::initializer_list<Node *> kids);

/* ---------------- Parser support ---------------- */

Spec *mergeSpec(Spec *a, Spec *b);
Type *specToType(const Spec &s);
Type *applyDecl(Type *base, const Decl &d);
Type *typeFromSpec(Spec *sp, int ptrCode);
Node *makeDeclaration(Spec *sp, vector<InitDecl> *list, int line);
Node *makeFunction(Spec *sp, Decl *d, Node *body, int line);
Type *lookupTypeName(const string &name, int line);
Type *refTag(const char *tag, bool isUnion, int line);
void  beginStruct(const char *tag, bool isUnion, int line);
void  addMembers(Spec *sp, vector<Decl *> *list, int line);
Type *endStruct();
void  setClassMode(bool isClass);
void  setAccess(int a);
void  setBase(Type *base, int line);
void  addMethodDef(Spec *sp, Decl *d, Node *body, int line);
void  addCtor(const char *name, ParamList *pl, vector<Node *> *inits, Node *body, int line);
void  addDtor(const char *name, Node *body, int line);
Node *makeOuterMethod(Spec *sp, int ptrCode, const char *cls, const char *name,
                      ParamList *pl, Node *body, int line);
Node *makeOuterCtor(const char *cls, ParamList *pl, vector<Node *> *inits, Node *body, int line);
Node *makeOuterDtor(const char *cls, Node *body, int line);
Node *makeCtorInit(const char *name, vector<Node *> *args, bool isBase, int line);
void  init_builtin_types();

/* ---------------- Constant evaluation ---------------- */

struct CVal {
    bool ok = false;
    bool isf = false;
    long long i = 0;
    double d = 0;
};
CVal constEval(Node *n);

/* ---------------- Global registries ---------------- */

extern map<string, Type *> g_typedefs;
extern map<string, Type *> g_tags;
extern map<string, long long> g_enum_consts;
extern map<string, string> g_macros;
extern vector<Node *> g_program;
extern vector<Type *> g_struct_list;

extern int g_lex_errors, g_syn_errors, g_sem_errors;
void report_error(const char *kind, int line, const string &msg);
void print_error_block(FILE *out);

#include <cstdio>

#endif
