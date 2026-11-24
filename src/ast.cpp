/* ============================================================
 * ast.cpp - Type system, declaration builders, struct layout,
 * constant-expression evaluation and error reporting.
 * ============================================================ */
#include "ast.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

map<string, Type *> g_typedefs;
map<string, Type *> g_tags;
map<string, long long> g_enum_consts;
map<string, string> g_macros;
vector<Node *> g_program;
vector<Type *> g_struct_list;

int g_lex_errors = 0, g_syn_errors = 0, g_sem_errors = 0;
static vector<string> g_report;

/* ------------------------------------------------------------
 * Error reporting
 * ------------------------------------------------------------ */
void report_error(const char *kind, int line, const string &msg)
{
    string s = string(kind) + " ERROR [Line " + std::to_string(line) + "]: " + msg;
    fprintf(stderr, "%s\n", s.c_str());
    g_report.push_back(s);
    if (!strcmp(kind, "LEXICAL"))       g_lex_errors++;
    else if (!strcmp(kind, "SYNTAX"))   g_syn_errors++;
    else                                g_sem_errors++;
}

void print_error_block(FILE *out)
{
    if (g_report.empty()) return;
    fprintf(out, "\n========================== ERROR REPORT =============================\n");
    for (size_t i = 0; i < g_report.size(); i++)
        fprintf(out, "%s\n", g_report[i].c_str());
    fprintf(out, "======================================================================\n");
}

/* ------------------------------------------------------------
 * Types
 * ------------------------------------------------------------ */
Type *mkBase(TKind k, bool uns)
{
    static Type *cache[2][16] = {{nullptr}};
    Type *&slot = cache[uns ? 1 : 0][(int)k];
    if (!slot) {
        slot = new Type();
        slot->kind = k;
        slot->is_unsigned = uns;
        slot->complete = true;
    }
    return slot;
}

Type *tVoid()   { return mkBase(T_VOID); }
Type *tBool()   { return mkBase(T_BOOL); }
Type *tChar()   { return mkBase(T_CHAR); }
Type *tShort()  { return mkBase(T_SHORT); }
Type *tInt()    { return mkBase(T_INT); }
Type *tLong()   { return mkBase(T_LONG); }
Type *tFloat()  { return mkBase(T_FLOAT); }
Type *tDouble() { return mkBase(T_DOUBLE); }
Type *tStream() { return mkBase(T_STREAM); }

Type *mkPtr(Type *b)
{
    Type *t = new Type();
    t->kind = T_PTR;
    t->base = b;
    t->complete = true;
    return t;
}

Type *mkArray(Type *b, long long n)
{
    Type *t = new Type();
    t->kind = T_ARRAY;
    t->base = b;
    t->len = n;
    t->complete = true;
    return t;
}

Type *mkFunc(Type *ret, const vector<Type *> &params, bool variadic)
{
    Type *t = new Type();
    t->kind = T_FUNC;
    t->base = ret;
    t->ptys = params;
    t->fvariadic = variadic;
    t->complete = true;
    return t;
}

bool derivedFrom(Type *d, Type *b)
{
    for (Type *t = d; t; t = t->parent)
        if (t == b) return true;
    return false;
}

long long sizeOf(Type *t)
{
    if (!t) return 4;
    switch (t->kind) {
    case T_VOID: case T_BOOL: case T_CHAR: return 1;
    case T_SHORT:  return 2;
    case T_INT:    return 4;
    case T_LONG:   return 8;
    case T_FLOAT:  return 4;
    case T_DOUBLE: return 8;
    case T_PTR:    return 8;
    case T_STREAM: return 8;
    case T_ARRAY:  return t->len < 0 ? 0 : t->len * sizeOf(t->base);
    case T_STRUCT: return t->size;
    case T_FUNC:   return 1;
    }
    return 4;
}

long long alignOf(Type *t)
{
    if (!t) return 4;
    if (t->kind == T_ARRAY)  return alignOf(t->base);
    if (t->kind == T_STRUCT) return t->align < 1 ? 1 : t->align;
    long long s = sizeOf(t);
    return s < 1 ? 1 : s;
}

bool isInteger(Type *t)
{
    return t && (t->kind == T_BOOL || t->kind == T_CHAR || t->kind == T_SHORT ||
                 t->kind == T_INT || t->kind == T_LONG);
}
bool isFloat(Type *t)   { return t && (t->kind == T_FLOAT || t->kind == T_DOUBLE); }
bool isArith(Type *t)   { return isInteger(t) || isFloat(t); }
bool isPointer(Type *t) { return t && t->kind == T_PTR; }
bool isPtrLike(Type *t) { return t && (t->kind == T_PTR || t->kind == T_ARRAY); }

bool sameType(Type *a, Type *b)
{
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
    case T_PTR:    return sameType(a->base, b->base);
    case T_ARRAY:  return a->len == b->len && sameType(a->base, b->base);
    case T_STRUCT: return a->tag == b->tag && a->is_union == b->is_union;
    case T_FUNC:
        if (a->ptys.size() != b->ptys.size() || a->fvariadic != b->fvariadic) return false;
        if (!sameType(a->base, b->base)) return false;
        for (size_t i = 0; i < a->ptys.size(); i++)
            if (!sameType(a->ptys[i], b->ptys[i])) return false;
        return true;
    default:       return a->is_unsigned == b->is_unsigned;
    }
}

string typeStr(Type *t)
{
    if (!t) return "int";
    string u = t->is_unsigned ? "unsigned " : "";
    switch (t->kind) {
    case T_VOID:   return "void";
    case T_BOOL:   return "bool";
    case T_CHAR:   return u + "char";
    case T_SHORT:  return u + "short";
    case T_INT:    return u + "int";
    case T_LONG:   return u + "long";
    case T_FLOAT:  return "float";
    case T_DOUBLE: return "double";
    case T_PTR:    return typeStr(t->base) + "*";
    case T_ARRAY:
        return typeStr(t->base) + "[" + (t->len >= 0 ? std::to_string(t->len) : "") + "]";
    case T_STRUCT: return string(t->is_union ? "union " : (t->is_class ? "class " : "struct ")) + t->tag;
    case T_STREAM: return "stream";
    case T_FUNC: {
        string r = typeStr(t->base) + "(";
        for (size_t i = 0; i < t->ptys.size(); i++) r += (i ? "," : "") + typeStr(t->ptys[i]);
        if (t->fvariadic) r += t->ptys.empty() ? "..." : ",...";
        return r + ")";
    }
    }
    return "int";
}

string declStr(Type *t, const string &name, bool ref)
{
    string dims;
    Type *b = t;
    while (b && b->kind == T_ARRAY) {
        dims += "[" + (b->len >= 0 ? std::to_string(b->len) : string("")) + "]";
        b = b->base;
    }
    return typeStr(b) + (ref ? "& " : " ") + name + dims;
}

const Field *findField(Type *st, const string &name)
{
    if (!st) return nullptr;
    for (size_t i = st->fields.size(); i-- > 0;)      /* last match wins: derived classes hide base members */
        if (st->fields[i].name == name) return &st->fields[i];
    return nullptr;
}

/* ------------------------------------------------------------
 * Nodes
 * ------------------------------------------------------------ */
Node *mkNode(NKind k, int line, const string &s)
{
    Node *n = new Node();
    n->kind = k;
    n->line = line;
    n->s = s;
    return n;
}

Node *mkK(NKind k, int line, const string &s, std::initializer_list<Node *> kids)
{
    Node *n = mkNode(k, line, s);
    for (Node *c : kids) n->kids.push_back(c);
    return n;
}

/* ------------------------------------------------------------
 * Declaration specifiers
 * ------------------------------------------------------------ */
Spec *mergeSpec(Spec *a, Spec *b)
{
    a->is_static   |= b->is_static;
    a->is_const    |= b->is_const;
    a->is_extern   |= b->is_extern;
    a->is_typedef  |= b->is_typedef;
    a->is_unsigned |= b->is_unsigned;
    a->is_signed   |= b->is_signed;
    a->is_short    |= b->is_short;
    a->is_auto     |= b->is_auto;
    a->longs       += b->longs;
    if (b->base != -1) a->base = b->base;
    if (b->named) a->named = b->named;
    delete b;
    return a;
}

Type *specToType(const Spec &s)
{
    if (s.named) return s.named;
    TKind k;
    switch (s.base) {
    case T_VOID:   k = T_VOID;   break;
    case T_BOOL:   k = T_BOOL;   break;
    case T_CHAR:   k = T_CHAR;   break;
    case T_FLOAT:  k = T_FLOAT;  break;
    case T_DOUBLE: k = T_DOUBLE; break;
    default:
        if (s.is_short)       k = T_SHORT;
        else if (s.longs > 0) k = T_LONG;
        else                  k = T_INT;
    }
    bool uns = s.is_unsigned && (k == T_CHAR || k == T_SHORT || k == T_INT || k == T_LONG);
    return mkBase(k, uns);
}

Type *applyDecl(Type *base, const Decl &d)
{
    Type *t = base;
    for (int i = 0; i < d.ptr; i++) t = mkPtr(t);
    if (d.fptr > 0) {                              /* int (*name)(params) */
        vector<Type *> pt;
        for (size_t i = 0; i < d.params.size(); i++) pt.push_back(d.params[i].ty);
        t = mkFunc(t, pt, d.variadic);
        for (int i = 0; i < d.fptr; i++) t = mkPtr(t);
    }
    for (int i = (int)d.dims.size() - 1; i >= 0; i--) t = mkArray(t, d.dims[i]);
    return t;
}

Type *typeFromSpec(Spec *sp, int ptrCode)
{
    Type *t = specToType(*sp);
    int n = ptrCode % 1000;
    for (int i = 0; i < n; i++) t = mkPtr(t);
    return t;
}

Type *lookupTypeName(const string &name, int line)
{
    map<string, Type *>::iterator it = g_typedefs.find(name);
    if (it != g_typedefs.end()) return it->second;
    it = g_tags.find(name);
    if (it != g_tags.end()) return it->second;
    report_error("SYNTAX", line, "unknown type name '" + name + "'");
    return tInt();
}

/* Builds the declaration nodes of one declaration statement from its type specifier and declarator list. */
Node *makeDeclaration(Spec *sp, vector<InitDecl> *list, int line)
{
    Node *group = mkNode(N_DECLLIST, line);
    if (!sp || !list) return group;
    Type *base = specToType(*sp);
    for (size_t i = 0; i < list->size(); i++) {
        Decl *d = (*list)[i].d;
        Type *ty = applyDecl(base, *d);
        if (sp->is_typedef) {
            if (!d->is_func) g_typedefs[d->name] = ty;
            continue;
        }
        if (d->is_func) {
            Node *f = mkNode(N_FUNC, d->line, d->name);
            f->ty = ty;
            f->params = d->params;
            f->variadic = d->variadic;
            f->is_static = sp->is_static;
            f->kids.push_back(nullptr);
            group->kids.push_back(f);
            continue;
        }
        Node *v = mkNode(N_DECL, d->line, d->name);
        v->ty = ty;
        v->is_static = sp->is_static;
        v->is_const = sp->is_const;
        v->is_ref = d->ref;
        v->is_auto = sp->is_auto;
        v->ctor_init = d->has_ctor;
        v->extra = d->cargs;
        v->flag = sp->is_extern;
        v->kids.push_back((*list)[i].init);
        group->kids.push_back(v);
    }
    return group;
}

Node *makeFunction(Spec *sp, Decl *d, Node *body, int line)
{
    Node *f = mkNode(N_FUNC, line, d->name);
    if (!d->is_func) {
        report_error("SYNTAX", line, "function body given for '" + d->name + "' which is not a function");
    }
    f->ty = applyDecl(specToType(*sp), *d);
    f->params = d->params;
    f->variadic = d->variadic;
    f->is_static = sp->is_static;
    f->is_ref = d->ref;
    f->kids.push_back(body);
    return f;
}

/* ------------------------------------------------------------
 * Struct / union definitions
 * ------------------------------------------------------------ */
static vector<Type *> s_stack;
static vector<int> s_access;               /* current access per open class */
static int s_anon = 0;

static Type *newStruct(const string &tag, bool isUnion)
{
    Type *t = new Type();
    t->kind = T_STRUCT;
    t->tag = tag;
    t->is_union = isUnion;
    t->complete = false;
    g_struct_list.push_back(t);
    return t;
}

Type *refTag(const char *tag, bool isUnion, int line)
{
    (void)line;
    map<string, Type *>::iterator it = g_tags.find(tag);
    if (it != g_tags.end()) return it->second;
    Type *t = newStruct(tag, isUnion);
    g_tags[tag] = t;
    return t;
}

void beginStruct(const char *tag, bool isUnion, int line)
{
    string name = tag ? tag : "";
    Type *t = nullptr;
    if (name.empty()) {
        name = "__anon" + std::to_string(++s_anon);
        t = newStruct(name, isUnion);
        g_tags[name] = t;
    } else {
        map<string, Type *>::iterator it = g_tags.find(name);
        if (it != g_tags.end() && it->second->kind == T_STRUCT && !it->second->complete) {
            t = it->second;
            t->is_union = isUnion;
        } else {
            if (it != g_tags.end())
                report_error("SEMANTIC", line, "redefinition of '" + name + "'");
            t = newStruct(name, isUnion);
            g_tags[name] = t;
        }
    }
    t->size = 0;
    t->align = 1;
    t->fields.clear();
    t->parent = nullptr;
    t->is_class = false;
    s_stack.push_back(t);
    s_access.push_back(0);
}

void setClassMode(bool isClass)
{
    if (s_stack.empty()) return;
    s_stack.back()->is_class = isClass;
    s_access.back() = isClass ? 2 : 0;
}

void setAccess(int a)
{
    if (!s_access.empty()) s_access.back() = a;
}

void setBase(Type *base, int line)
{
    if (s_stack.empty() || !base) return;
    Type *t = s_stack.back();
    if (base->kind != T_STRUCT || base->is_union) {
        report_error("SEMANTIC", line, "base class '" + typeStr(base) + "' is not a class or struct");
        return;
    }
    if (!base->complete) {
        report_error("SEMANTIC", line, "base class '" + typeStr(base) + "' has incomplete type");
        return;
    }
    t->parent = base;
    t->fields = base->fields;                  /* base subobject comes first */
    t->size = base->size;
    t->align = base->align;
}

static void fixVoid(ParamList *p)
{
    if (p->v.size() == 1 && p->v[0].ty && p->v[0].ty->kind == T_VOID && p->v[0].name.empty())
        p->v.clear();
}


/* ------------------------------------------------------------
 * Member functions, constructors and destructors.  Each becomes an
 * ordinary function named "Class::name" with a hidden first
 * parameter 'this'.
 * ------------------------------------------------------------ */
static Param thisParam(Type *cls)
{
    Param p;
    p.ty = mkPtr(cls);
    p.name = "this";
    return p;
}

static Node *buildMethod(Type *cls, int access, Type *base, const Decl &d, Node *body, int line)
{
    Node *f = mkNode(N_FUNC, line, cls->tag + "::" + d.name);
    f->ty = applyDecl(base, d);
    f->params.push_back(thisParam(cls));
    for (size_t i = 0; i < d.params.size(); i++) f->params.push_back(d.params[i]);
    f->variadic = d.variadic;
    f->cls = cls;
    f->access = access;
    f->is_ref = d.ref;
    f->kids.push_back(body);
    return f;
}

static Node *buildSpecial(Type *cls, int access, bool dtor, ParamList *pl, Node *body, int line)
{
    Node *f = mkNode(N_FUNC, line, cls->tag + "::" + (dtor ? "~" : "") + cls->tag);
    f->ty = tVoid();
    f->params.push_back(thisParam(cls));
    if (pl) for (size_t i = 0; i < pl->v.size(); i++) f->params.push_back(pl->v[i]);
    f->cls = cls;
    f->access = access;
    f->is_ctor = !dtor;
    f->is_dtor = dtor;
    f->kids.push_back(body);
    return f;
}

/* Prepends member / base initialisers to a constructor body. */
static void applyInits(Node *ctor, vector<Node *> *inits)
{
    if (!inits || !ctor->kids[0]) return;
    Node *body = ctor->kids[0];
    vector<Node *> pre;
    for (size_t i = 0; i < inits->size(); i++) {
        pre.push_back((*inits)[i]);
        if ((*inits)[i]->flag) ctor->has_binit = true;
    }
    body->kids.insert(body->kids.begin(), pre.begin(), pre.end());
}

/* Builds one entry of a constructor initialiser list (a member or the base class). */
Node *makeCtorInit(const char *name, vector<Node *> *args, bool isBase, int line)
{
    Node *self = mkNode(N_ID, line, "this");
    Node *st;
    if (isBase) {
        Node *call = mkNode(N_CALL, line, string(name) + "::" + name);
        call->kids.push_back(self);
        for (size_t i = 0; i < args->size(); i++) call->kids.push_back((*args)[i]);
        st = mkK(N_EXPRSTMT, line, "", {call});
        st->flag = true;                               /* marks a base initialiser */
    } else {
        Node *mem = mkK(N_MEMBER, line, name, {self});
        mem->flag = true;                              /* this->name */
        if (args->size() != 1) {
            const Field *f = s_stack.empty() ? nullptr : findField(s_stack.back(), name);
            if (f && f->ty->kind == T_STRUCT && !args->empty()) {
                /* multi-argument member init calls the member's constructor */
                Node *call = mkNode(N_CALL, line, f->ty->tag + "::" + f->ty->tag);
                call->kids.push_back(mkK(N_UN, line, "&", {mem}));
                for (size_t i = 0; i < args->size(); i++) call->kids.push_back((*args)[i]);
                st = mkK(N_EXPRSTMT, line, "", {call});
            } else {
                report_error("SEMANTIC", line, "initializer for member '" + string(name) +
                                               "' needs exactly one value");
                st = mkNode(N_EMPTY, line);
            }
        } else {
            st = mkK(N_EXPRSTMT, line, "", {mkK(N_ASSIGN, line, "=", {mem, (*args)[0]})});
        }
    }
    delete args;
    return st;
}

void addMethodDef(Spec *sp, Decl *d, Node *body, int line)
{
    if (s_stack.empty()) return;
    if (!d->is_func) {
        report_error("SYNTAX", line, "function body given for '" + d->name + "' which is not a function");
        return;
    }
    g_program.push_back(buildMethod(s_stack.back(), s_access.back(), specToType(*sp), *d, body, line));
}

void addCtor(const char *name, ParamList *pl, vector<Node *> *inits, Node *body, int line)
{
    if (s_stack.empty()) return;
    Type *cls = s_stack.back();
    if (cls->tag != name) {
        report_error("SEMANTIC", line, "constructor name '" + string(name) + "' does not match class '" + cls->tag + "'");
        return;
    }
    if (pl) fixVoid(pl);
    Node *f = buildSpecial(cls, s_access.back(), false, pl, body, line);
    applyInits(f, inits);
    g_program.push_back(f);
}

void addDtor(const char *name, Node *body, int line)
{
    if (s_stack.empty()) return;
    Type *cls = s_stack.back();
    if (cls->tag != name) {
        report_error("SEMANTIC", line, "destructor name '~" + string(name) + "' does not match class '" + cls->tag + "'");
        return;
    }
    g_program.push_back(buildSpecial(cls, s_access.back(), true, nullptr, body, line));
}

static Type *classByName(const char *name, int line)
{
    map<string, Type *>::iterator it = g_tags.find(name);
    if (it == g_tags.end() || it->second->kind != T_STRUCT || !it->second->complete) {
        report_error("SEMANTIC", line, "'" + string(name) + "' is not a defined class");
        return nullptr;
    }
    return it->second;
}

Node *makeOuterMethod(Spec *sp, int ptrCode, const char *cls, const char *name,
                      ParamList *pl, Node *body, int line)
{
    Type *c = classByName(cls, line);
    if (!c) return mkNode(N_EMPTY, line);
    Decl d;
    d.name = name;
    d.ptr = ptrCode % 1000;
    d.is_func = true;
    if (pl) { fixVoid(pl); d.params = pl->v; d.variadic = pl->variadic; }
    Node *f = buildMethod(c, 0, specToType(*sp), d, body, line);
    return f;
}

Node *makeOuterCtor(const char *cls, ParamList *pl, vector<Node *> *inits, Node *body, int line)
{
    Type *c = classByName(cls, line);
    if (!c) return mkNode(N_EMPTY, line);
    if (pl) fixVoid(pl);
    Node *f = buildSpecial(c, 0, false, pl, body, line);
    applyInits(f, inits);
    return f;
}

Node *makeOuterDtor(const char *cls, Node *body, int line)
{
    Type *c = classByName(cls, line);
    if (!c) return mkNode(N_EMPTY, line);
    return buildSpecial(c, 0, true, nullptr, body, line);
}

/* Adds the member declarations of a class or struct to its specifier and computes the layout. */
void addMembers(Spec *sp, vector<Decl *> *list, int line)
{
    if (s_stack.empty() || !sp || !list) return;
    Type *st = s_stack.back();
    Type *base = specToType(*sp);
    for (size_t i = 0; i < list->size(); i++) {
        Decl *d = (*list)[i];
        if (d->is_func) {                              /* in-class prototype */
            Node *m = buildMethod(st, s_access.back(), base, *d, nullptr, line);
            g_program.push_back(m);
            continue;
        }
        Type *ft = applyDecl(base, *d);
        if (ft->kind == T_VOID) {
            report_error("SEMANTIC", line, "member '" + d->name + "' declared void");
            continue;
        }
        if (ft->kind == T_STRUCT && !ft->complete) {
            report_error("SEMANTIC", line, "member '" + d->name + "' has incomplete type");
            continue;
        }
        for (size_t k = 0; k < st->fields.size(); k++) {
            if (st->fields[k].owner == st && st->fields[k].name == d->name) {
                report_error("SEMANTIC", line, "duplicate member '" + d->name + "'");
                break;
            }
        }
        long long a = alignOf(ft);
        long long off;
        if (st->is_union) {
            off = 0;
            st->size = std::max(st->size, sizeOf(ft));
        } else {
            off = (st->size + a - 1) / a * a;
            st->size = off + sizeOf(ft);
        }
        st->align = std::max(st->align, a);
        Field f;
        f.name = d->name;
        f.ty = ft;
        f.offset = off;
        f.owner = st;
        f.access = s_access.empty() ? 0 : s_access.back();
        st->fields.push_back(f);
    }
}

Type *endStruct()
{
    Type *t = s_stack.back();
    s_stack.pop_back();
    s_access.pop_back();
    long long a = t->align < 1 ? 1 : t->align;
    t->size = (t->size + a - 1) / a * a;
    t->complete = true;
    return t;
}

void init_builtin_types()
{
    g_typedefs["string"] = mkPtr(tChar());
    g_typedefs["size_t"] = mkBase(T_LONG, true);
    g_typedefs["va_list"] = mkPtr(tChar());
    Type *f = newStruct("FILE", false);
    f->complete = true;
    f->size = 8;
    g_typedefs["FILE"] = f;
    g_macros["EOF"] = "(-1)";
    g_macros["INT_MAX"] = "2147483647";
    g_macros["INT_MIN"] = "(-2147483647-1)";
    g_macros["RAND_MAX"] = "2147483647";
    g_macros["M_PI"] = "3.14159265358979";
}

/* ------------------------------------------------------------
 * Constant expression evaluation (array sizes, enumerators,
 * case labels, global initialisers)
 * ------------------------------------------------------------ */
static CVal cvi(long long v) { CVal c; c.ok = true; c.i = v; return c; }
static CVal cvd(double v)    { CVal c; c.ok = true; c.isf = true; c.d = v; return c; }
static double asD(const CVal &c) { return c.isf ? c.d : (double)c.i; }

/* Evaluates a constant expression tree (array sizes, case labels, global initialisers). */
CVal constEval(Node *n)
{
    CVal bad;
    if (!n) return bad;
    switch (n->kind) {
    case N_INT: case N_CHAR: case N_BOOL: return cvi(n->iv);
    case N_NULL:  return cvi(0);
    case N_FLOAT: return cvd(n->dv);
    case N_ID: {
        map<string, long long>::iterator it = g_enum_consts.find(n->s);
        if (it != g_enum_consts.end()) return cvi(it->second);
        return bad;
    }
    case N_SIZEOF_T: return cvi(sizeOf(n->ty));
    case N_CAST: {
        CVal a = constEval(n->kids[0]);
        if (!a.ok) return bad;
        if (isFloat(n->ty)) return cvd(asD(a));
        if (isInteger(n->ty)) return cvi(a.isf ? (long long)a.d : a.i);
        return a;
    }
    case N_UN: {
        CVal a = constEval(n->kids[0]);
        if (!a.ok) return bad;
        if (n->s == "-") return a.isf ? cvd(-a.d) : cvi(-a.i);
        if (n->s == "+") return a;
        if (n->s == "!") return cvi(asD(a) == 0);
        if (n->s == "~" && !a.isf) return cvi(~a.i);
        return bad;
    }
    case N_COND: {
        CVal c = constEval(n->kids[0]);
        if (!c.ok) return bad;
        return constEval(asD(c) != 0 ? n->kids[1] : n->kids[2]);
    }
    case N_BIN: {
        CVal a = constEval(n->kids[0]);
        CVal b = constEval(n->kids[1]);
        if (!a.ok || !b.ok) return bad;
        const string &op = n->s;
        if (op == "&&") return cvi(asD(a) != 0 && asD(b) != 0);
        if (op == "||") return cvi(asD(a) != 0 || asD(b) != 0);
        if (a.isf || b.isf) {
            double x = asD(a), y = asD(b);
            if (op == "+") return cvd(x + y);
            if (op == "-") return cvd(x - y);
            if (op == "*") return cvd(x * y);
            if (op == "/") return y == 0 ? bad : cvd(x / y);
            if (op == "<") return cvi(x < y);
            if (op == ">") return cvi(x > y);
            if (op == "<=") return cvi(x <= y);
            if (op == ">=") return cvi(x >= y);
            if (op == "==") return cvi(x == y);
            if (op == "!=") return cvi(x != y);
            return bad;
        }
        long long x = a.i, y = b.i;
        if (op == "+") return cvi(x + y);
        if (op == "-") return cvi(x - y);
        if (op == "*") return cvi(x * y);
        if (op == "/") return y == 0 ? bad : cvi(x / y);
        if (op == "%") return y == 0 ? bad : cvi(x % y);
        if (op == "&") return cvi(x & y);
        if (op == "|") return cvi(x | y);
        if (op == "^") return cvi(x ^ y);
        if (op == "<<") return (y < 0 || y > 62) ? bad : cvi(x << y);
        if (op == ">>") return (y < 0 || y > 62) ? bad : cvi(x >> y);
        if (op == "<") return cvi(x < y);
        if (op == ">") return cvi(x > y);
        if (op == "<=") return cvi(x <= y);
        if (op == ">=") return cvi(x >= y);
        if (op == "==") return cvi(x == y);
        if (op == "!=") return cvi(x != y);
        return bad;
    }
    default:
        return bad;
    }
}
