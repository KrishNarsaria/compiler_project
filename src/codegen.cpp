/* ============================================================
 * codegen.cpp - Intermediate code generator.
 *
 * Walks the abstract syntax tree produced by the parser and emits
 * three-address code.  Semantic checks (declaration before use,
 * argument counts, lvalues, ...) are performed while generating.
 * ============================================================ */
#include "ast.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <set>
#include <sys/stat.h>

namespace {

/* ------------------------------------------------------------
 * Core data structures
 * ------------------------------------------------------------ */
struct FuncSig;

struct Value {
    string s;
    Type  *ty = nullptr;
    bool   isConst = false;
    bool   isFloatC = false;
    long long iv = 0;
    double dv = 0;
    FuncSig *lam = nullptr;               /* set when the value is a lambda    */
    vector<string> capIr;                 /* hidden capture slots of a lambda  */
    bool   isRefVal = false;              /* holds the address of a 'ty' object */
    bool   isCast = false;                /* produced by an explicit cast      */
};

struct VarInfo {
    Type  *ty = nullptr;
    string ir;
    bool   isRef = false;
    bool   isConst = false;
    FuncSig *lam = nullptr;               /* variable holds a lambda           */
    vector<string> capIr;
};

enum LKind { LV_VAR, LV_INDEX, LV_DEREF, LV_FIELD };

struct LV {
    LKind  k = LV_VAR;
    string base;          /* variable / pointer operand / struct base  */
    Value  off;           /* byte offset (LV_INDEX)                    */
    string path;          /* member path (LV_FIELD)                    */
    bool   arrow = false;
    Type  *ty = nullptr;
};

struct FuncSig {
    string name, ir;
    Type  *ret = nullptr;
    bool   retRef = false;                /* returns a reference (an address)  */
    vector<Param> params;
    bool   variadic = false;
    Node  *body = nullptr;
    Node  *node = nullptr;
    int    line = 0;
    Type  *cls = nullptr;                 /* owning class (methods)            */
    int    access = 0;
    bool   isCtor = false, isDtor = false, hasBInit = false;
};

struct LibFn {
    Type *ret;
    bool  variadic;
    int   nargs;
    Type *argTy;          /* conversion applied to each argument, or null */
};

struct LoopCtx { string brk, cont; };

/* ------------------------------------------------------------
 * Generator state
 * ------------------------------------------------------------ */
vector<string> cur;                       /* body of the current function */
vector<string> dataLines;
vector<string> textOut;
int tc = 0, lc = 0, dryDepth = 0;
int instrCount = 0;
FuncSig *curFunc = nullptr;
vector<map<string, VarInfo> > scopes;     /* scopes[0] holds globals       */
map<string, int> nameCount, globalNames;
set<string> definedGlobals;                 /* globals that already have a definition */
map<string, vector<FuncSig *> > funcs;
map<string, LibFn> lib;
vector<LoopCtx> loops;
set<string> labelsDefined;
vector<std::pair<string, int> > labelsUsed;
int staticCounter = 0;
vector<FuncSig *> lambdaQ;                /* lambdas waiting to be generated   */
int lamCount = 0;
vector<vector<std::pair<string, Type *> > > dtorScopes;   /* parallel to scopes */

/* forward declarations */
Value genExpr(Node *n);
bool  genLV(Node *n, LV &out);
void  genStmt(Node *n);
void  genCond(Node *e, const string &target, bool whenTrue);
Value binaryOp(const string &op, Value a, Value b, int line);
Value convertTo(Value v, Type *to);
Type *typeOf(Node *n);
Type *typeOfNoDecay(Node *n);
Value genCall(Node *n);
void  emitDtors(size_t scopeIdx);
void  checkAccess(Type *owner, int access, int line, const string &what);
bool  implicitMember(const string &name, int line, LV &out);
Type *funcTypeOf(FuncSig *f, size_t skip = 0);
Value emitUserCall(FuncSig *f, vector<Node *> &argNodes, size_t off, const vector<string> &prefix, int line);
Value genMethodCall(Node *n);
Value genICall(Node *n);
Value genLambda(Node *n);

void err(int line, const string &msg)
{
    if (dryDepth) return;
    report_error("SEMANTIC", line, msg);
}

/* ------------------------------------------------------------
 * Small helpers
 * ------------------------------------------------------------ */
void emit(const string &s)      { cur.push_back(s); }
void emitLabel(const string &l) { cur.push_back(l + ":"); }
string newTemp()                { return "t" + std::to_string(++tc); }
string newLabel()               { return "L" + std::to_string(++lc); }

string fmtDouble(double d)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.15g", d);
    string s = buf;
    if (s.find_first_of(".eEni") == string::npos) s += ".0";
    return s;
}

string quoteStr(const string &raw)
{
    string o = "\"";
    for (size_t i = 0; i < raw.size(); i++) {
        unsigned char c = (unsigned char)raw[i];
        switch (c) {
        case '\n': o += "\\n"; break;
        case '\t': o += "\\t"; break;
        case '\r': o += "\\r"; break;
        case '\\': o += "\\\\"; break;
        case '"':  o += "\\\""; break;
        case 0:    o += "\\0"; break;
        default:
            if (c < 32 || c > 126) {
                char b[8];
                snprintf(b, sizeof(b), "\\x%02x", c);
                o += b;
            } else {
                o += (char)c;
            }
        }
    }
    return o + "\"";
}

string charRepr(long long v)
{
    if (v == 10) return "'\\n'";
    if (v == 9)  return "'\\t'";
    if (v == 13) return "'\\r'";
    if (v == 0)  return "'\\0'";
    if (v == 39) return "'\\''";
    if (v == 92) return "'\\\\'";
    if (v >= 32 && v < 127) return string("'") + (char)v + "'";
    return std::to_string(v);
}

Value cInt(long long v, Type *ty = nullptr)
{
    Value r;
    r.s = std::to_string(v);
    r.ty = ty ? ty : tInt();
    r.isConst = true;
    r.iv = v;
    return r;
}

Value cFlt(double d, Type *ty = nullptr)
{
    Value r;
    r.s = fmtDouble(d);
    r.ty = ty ? ty : tDouble();
    r.isConst = true;
    r.isFloatC = true;
    r.dv = d;
    return r;
}

Value cChar(long long v)
{
    Value r = cInt(v, tChar());
    r.s = charRepr(v);
    return r;
}

Value mkv(const string &s, Type *t)
{
    Value v;
    v.s = s;
    v.ty = t;
    return v;
}

bool constTruth(const Value &v) { return v.isFloatC ? v.dv != 0 : v.iv != 0; }

struct DrySnap { size_t n; int t, l; };
DrySnap dryBegin()
{
    DrySnap s;
    s.n = cur.size(); s.t = tc; s.l = lc;
    dryDepth++;
    return s;
}
void dryEnd(const DrySnap &s)
{
    cur.resize(s.n);
    tc = s.t; lc = s.l;
    dryDepth--;
}

/* ------------------------------------------------------------
 * Scopes and variables
 * ------------------------------------------------------------ */
void pushScope()
{
    scopes.push_back(map<string, VarInfo>());
    dtorScopes.push_back(vector<std::pair<string, Type *> >());
}
void popScope()
{
    scopes.pop_back();
    dtorScopes.pop_back();
}

VarInfo *lookupVar(const string &n)
{
    for (int i = (int)scopes.size() - 1; i >= 0; i--) {
        map<string, VarInfo>::iterator it = scopes[i].find(n);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

VarInfo *lookupVarLocal(const string &n)      /* ignores the global scope */
{
    for (int i = (int)scopes.size() - 1; i >= 1; i--) {
        map<string, VarInfo>::iterator it = scopes[i].find(n);
        if (it != scopes[i].end()) return &it->second;
    }
    return nullptr;
}

/* User variables must never collide with generated temporaries (t1, t2, ...). */
bool tempLike(const string &s)
{
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); i++)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

string uniqueName(const string &n)
{
    int &c = nameCount[n];
    string r = (c == 0) ? n : n + "_" + std::to_string(c);
    if (tempLike(r)) r += "_v";
    c++;
    return r;
}

string declareVar(int line, const string &name, Type *ty, bool isConst, bool isRef)
{
    if (scopes.back().count(name)) {
        err(line, "redeclaration of '" + name + "' in the same scope");
    }
    VarInfo v;
    v.ty = ty;
    v.ir = uniqueName(name);
    v.isRef = isRef;
    v.isConst = isConst;
    scopes.back()[name] = v;
    return v.ir;
}

/* ------------------------------------------------------------
 * Type conversions
 * ------------------------------------------------------------ */
Type *promote(Type *t)
{
    if (isInteger(t) && sizeOf(t) < 4) return tInt();
    return t;
}

Type *commonType(Type *a, Type *b)
{
    a = promote(a); b = promote(b);
    if (a->kind == T_DOUBLE || b->kind == T_DOUBLE) return tDouble();
    if (a->kind == T_FLOAT  || b->kind == T_FLOAT)  return tFloat();
    if (a->kind == T_LONG   || b->kind == T_LONG)   return tLong();
    return tInt();
}

long long narrowConst(long long v, Type *to)
{
    switch (to->kind) {
    case T_CHAR:  return to->is_unsigned ? (long long)(uint8_t)v  : (long long)(int8_t)v;
    case T_SHORT: return to->is_unsigned ? (long long)(uint16_t)v : (long long)(int16_t)v;
    case T_INT:   return to->is_unsigned ? (long long)(uint32_t)v : (long long)(int32_t)v;
    default:      return v;
    }
}

/* Converts a value to the target type, emitting a cast instruction wherever the representation changes. */
Value convertTo(Value v, Type *to)
{
    Type *from = v.ty;
    if (!to || !from) return v;
    if (to->kind == T_STRUCT || to->kind == T_ARRAY || to->kind == T_VOID) return v;
    if (isArith(from) || isPointer(from)) {
        if (to->kind == T_BOOL && from->kind != T_BOOL) {
            if (v.isConst) return cInt(constTruth(v) ? 1 : 0, to);
            string t = newTemp();
            emit(t + " = " + v.s + " != 0");
            return mkv(t, to);
        }
    }
    if (isArith(to) && isArith(from)) {
        bool tf = isFloat(to), ff = isFloat(from);
        if (tf != ff) {
            if (v.isConst) {
                if (tf) return cFlt(v.isFloatC ? v.dv : (double)v.iv, to);
                return cInt(narrowConst((long long)(v.isFloatC ? v.dv : (double)v.iv), to), to);
            }
            string t = newTemp();
            emit(t + " = (" + typeStr(to) + ") " + v.s);
            return mkv(t, to);
        }
        if (!tf && sizeOf(to) < sizeOf(from)) {
            if (v.isConst) return cInt(narrowConst(v.iv, to), to);
            string t = newTemp();
            emit(t + " = (" + typeStr(to) + ") " + v.s);
            return mkv(t, to);
        }
        if (tf && to->kind == T_FLOAT && from->kind == T_DOUBLE && v.isConst) {
            Value r = v; r.ty = to; return r;
        }
    }
    if (!v.isConst && !v.s.empty()) {
        bool needCast = false;
        if (isPointer(to) && isPointer(from)) needCast = typeStr(to) != typeStr(from);
        else if (isArith(to) && isArith(from)) {
            if (to->kind == T_LONG && from->kind != T_LONG) needCast = true;
            else if (to->kind == T_DOUBLE && from->kind == T_FLOAT) needCast = true;
            else if ((to->kind == T_INT || to->kind == T_LONG) && (from->kind == T_INT || from->kind == T_LONG) &&
                     to->is_unsigned != from->is_unsigned) needCast = true;
        } else if ((isPointer(to) && isArith(from)) || (isArith(to) && isPointer(from))) needCast = true;
        if (needCast) {
            string t = newTemp();
            emit(t + " = (" + typeStr(to) + ") " + v.s);
            return mkv(t, to);
        }
    }
    v.ty = to;
    return v;
}

/* Rejects implicit pointer<->integer and pointer<->pointer conversions that
 * C++ forbids: a null pointer constant and explicit casts are the exceptions. */
bool ptrConvError(Type *to, Value r, int line)
{
    Type *from = r.ty;
    bool toPtr = isPointer(to), fromPtr = isPtrLike(from);
    if (fromPtr && toPtr) {
        Type *tb = to->kind == T_ARRAY ? to->base : to->base;
        Type *fb = from->kind == T_ARRAY ? from->base : from->base;
        if (!tb || !fb || tb->kind == T_VOID || fb->kind == T_VOID) return false;
        if (from->kind == T_FUNC || to->kind == T_FUNC) return false;
        if (!sameType(tb, fb)) {
            if (r.isCast) return false;
            if (tb->kind == T_STRUCT && fb->kind == T_STRUCT && derivedFrom(fb, tb)) return false;
            err(line, "incompatible pointer types ('" + typeStr(from) + "' to '" + typeStr(to) + "')");
            return true;
        }
        return false;
    }
    if (toPtr && !fromPtr) {
        if (r.isConst && !r.isFloatC && r.iv == 0) return false;
        if (r.isCast) return false;
        err(line, "cannot convert '" + typeStr(from) + "' to pointer type '" + typeStr(to) + "'");
        return true;
    }
    if (!toPtr && fromPtr) {
        if (r.isCast) return false;
        err(line, "cannot convert pointer type '" + typeStr(from) + "' to '" + typeStr(to) + "'");
        return true;
    }
    return false;
}

/* ------------------------------------------------------------
 * Lvalues
 * ------------------------------------------------------------ */
string lvText(const LV &lv)
{
    switch (lv.k) {
    case LV_VAR:   return lv.base;
    case LV_INDEX: return lv.base + "[" + lv.off.s + "]";
    case LV_DEREF: return "*" + lv.base;
    case LV_FIELD: return lv.base + (lv.arrow ? "->" : ".") + lv.path;
    }
    return lv.base;
}

LV lvOfVar(const VarInfo &v)
{
    LV lv;
    lv.ty = v.ty;
    if (v.isRef) { lv.k = LV_DEREF; lv.base = v.ir; }
    else         { lv.k = LV_VAR;   lv.base = v.ir; }
    return lv;
}

Value addrOf(const LV &lv)
{
    Type *pt = mkPtr(lv.ty);
    if (lv.k == LV_DEREF) return mkv(lv.base, pt);
    string t = newTemp();
    emit(t + " = &" + lvText(lv));
    return mkv(t, pt);
}

Value loadLV(const LV &lv)
{
    if (lv.ty->kind == T_ARRAY) {                    /* array decays to pointer */
        Type *pt = mkPtr(lv.ty->base);
        if (lv.k == LV_DEREF) return mkv(lv.base, pt);
        string t = newTemp();
        emit(t + " = &" + lvText(lv));
        return mkv(t, pt);
    }
    if (lv.k == LV_VAR) return mkv(lv.base, lv.ty);
    string t = newTemp();
    emit(t + " = " + lvText(lv));
    return mkv(t, lv.ty);
}

void storeLV(const LV &lv, const Value &v)
{
    emit(lvText(lv) + " = " + v.s);
}

bool foldBin(const string &op, const Value &a, const Value &b, Type *rt, Value &out);

Value scaleIdx(Value idx, long long sz)
{
    if (sz == 1) return idx;
    if (idx.isConst && !idx.isFloatC) return cInt(idx.iv * sz, tLong());
    string t = newTemp();
    emit(t + " = " + idx.s + " * " + std::to_string(sz));
    return mkv(t, tLong());
}

Value addOffsets(const Value &a, const Value &b)
{
    if (a.isConst && b.isConst) return cInt(a.iv + b.iv, tLong());
    if (a.isConst && a.iv == 0) return b;
    if (b.isConst && b.iv == 0) return a;
    string t = newTemp();
    emit(t + " = " + a.s + " + " + b.s);
    return mkv(t, tLong());
}

LV indexOf(const LV &base, Value idx)
{
    Type *elem = base.ty->base;
    Value off = scaleIdx(idx, sizeOf(elem));
    LV r;
    r.k = LV_INDEX;
    r.ty = elem;
    if (base.k == LV_INDEX) {
        r.base = base.base;
        r.off = addOffsets(base.off, off);
    } else {
        r.base = (base.k == LV_VAR) ? base.base : addrOf(base).s;
        r.off = off;
    }
    return r;
}

LV memberOf(const LV &base, const Field &f)
{
    LV r;
    r.k = LV_FIELD;
    r.ty = f.ty;
    if (base.k == LV_VAR) {
        r.base = base.base; r.path = f.name; r.arrow = false;
    } else if (base.k == LV_FIELD) {
        r.base = base.base; r.path = base.path + "." + f.name; r.arrow = base.arrow;
    } else {
        r.base = addrOf(base).s; r.path = f.name; r.arrow = true;
    }
    return r;
}

Type *typeOfNoDecay(Node *n)
{
    DrySnap s = dryBegin();
    Type *r = nullptr;
    if (n->kind == N_ID) {
        VarInfo *v = lookupVar(n->s);
        if (v) r = v->ty;
        else   r = genExpr(n).ty;
    } else if (n->kind == N_INDEX || n->kind == N_MEMBER || (n->kind == N_UN && n->s == "*")) {
        LV lv;
        if (genLV(n, lv)) r = lv.ty;
    } else {
        r = genExpr(n).ty;
    }
    dryEnd(s);
    return r ? r : tInt();
}

Type *typeOf(Node *n)
{
    DrySnap s = dryBegin();
    Type *r = genExpr(n).ty;
    dryEnd(s);
    return r ? r : tInt();
}

static bool g_lv_quiet = false;

bool tryGenLV(Node *n, LV &out)
{
    g_lv_quiet = true;
    bool ok = genLV(n, out);
    g_lv_quiet = false;
    return ok;
}

/* Computes the l-value form (variable, element, field, dereference) of an expression node. */
bool genLV(Node *n, LV &out)
{
    switch (n->kind) {
    case N_ID: {
        VarInfo *v = lookupVar(n->s);
        if (!v) {
            if (implicitMember(n->s, n->line, out)) return true;
            if (g_enum_consts.count(n->s)) err(n->line, "enumeration constant '" + n->s + "' is not assignable");
            else err(n->line, "use of undeclared identifier '" + n->s + "'");
            return false;
        }
        out = lvOfVar(*v);
        return true;
    }
    case N_UN: {
        if (n->s != "*") break;
        Value p = genExpr(n->kids[0]);
        if (!isPtrLike(p.ty)) { err(n->line, "indirection requires a pointer operand"); return false; }
        if (p.ty->base->kind == T_VOID) { err(n->line, "cannot dereference a 'void*'"); return false; }
        out = LV();
        out.k = LV_DEREF; out.base = p.s; out.ty = p.ty->base;
        return true;
    }
    case N_INDEX: {
        Node *b = n->kids[0];
        Type *bt = typeOfNoDecay(b);
        if (bt->kind != T_ARRAY && bt->kind != T_PTR) {
            /* C allows i[a] as well as a[i]; swap the operands. */
            Type *it = typeOfNoDecay(n->kids[1]);
            if (it->kind == T_ARRAY || it->kind == T_PTR) {
                Node *tmp = n->kids[0]; n->kids[0] = n->kids[1]; n->kids[1] = tmp;
                b = n->kids[0];
                bt = it;
            }
        }
        if (bt->kind == T_ARRAY) {
            LV base;
            if (!genLV(b, base)) return false;
            Value idx = genExpr(n->kids[1]);
            if (!isInteger(idx.ty)) { err(n->line, "array subscript is not an integer"); return false; }
            out = indexOf(base, idx);
            return true;
        }
        if (bt->kind == T_PTR) {
            Value p = genExpr(b);
            Value idx = genExpr(n->kids[1]);
            if (!isInteger(idx.ty)) { err(n->line, "array subscript is not an integer"); return false; }
            if (p.ty->base->kind == T_VOID) { err(n->line, "cannot subscript a 'void*'"); return false; }
            out = LV();
            out.k = LV_INDEX; out.base = p.s; out.ty = p.ty->base;
            out.off = scaleIdx(idx, sizeOf(p.ty->base));
            return true;
        }
        err(n->line, "subscripted value is neither an array nor a pointer");
        return false;
    }
    case N_MEMBER: {
        if (n->flag) {                                   /* p->member */
            Value p = genExpr(n->kids[0]);
            if (!isPointer(p.ty) || p.ty->base->kind != T_STRUCT) {
                err(n->line, "member reference with '->' requires a pointer to struct or union");
                return false;
            }
            const Field *f = findField(p.ty->base, n->s);
            if (!f) { err(n->line, "no member named '" + n->s + "' in '" + typeStr(p.ty->base) + "'"); return false; }
            checkAccess(f->owner, f->access, n->line, n->s);
            out = LV();
            out.k = LV_FIELD; out.base = p.s; out.path = f->name; out.arrow = true; out.ty = f->ty;
            return true;
        }
        LV base;
        if (tryGenLV(n->kids[0], base) && base.ty->kind == T_STRUCT) {
            const Field *f = findField(base.ty, n->s);
            if (!f) { err(n->line, "no member named '" + n->s + "' in '" + typeStr(base.ty) + "'"); return false; }
            checkAccess(f->owner, f->access, n->line, n->s);
            out = memberOf(base, *f);
            return true;
        }
        {                                              /* member of a struct rvalue, e.g. mkPoint(1,2).x */
            Value v = genExpr(n->kids[0]);
            if (v.ty->kind != T_STRUCT) {
                err(n->line, "member reference with '.' requires a struct or union");
                return false;
            }
            const Field *f = findField(v.ty, n->s);
            if (!f) { err(n->line, "no member named '" + n->s + "' in '" + typeStr(v.ty) + "'"); return false; }
            checkAccess(f->owner, f->access, n->line, n->s);
            out = LV();
            out.k = LV_FIELD;
            out.base = v.s;
            out.path = f->name;
            out.arrow = false;
            out.ty = f->ty;
            return true;
        }
    }
    case N_COND: {                                     /* (c ? lv1 : lv2) is an lvalue when both branches are */
        string Lf = newLabel(), Le = newLabel(), t = newTemp();
        genCond(n->kids[0], Lf, false);
        LV l1;
        if (!genLV(n->kids[1], l1)) return false;
        emit(t + " = " + addrOf(l1).s);
        emit("goto " + Le);
        emitLabel(Lf);
        LV l2;
        if (!genLV(n->kids[2], l2)) return false;
        emit(t + " = " + addrOf(l2).s);
        emitLabel(Le);
        out = LV();
        out.k = LV_DEREF;
        out.base = t;
        out.ty = n->ty;
        return true;
    }
    case N_CALL:
    case N_MCALL:
    case N_ICALL: {
        Value v = (n->kind == N_CALL) ? genCall(n)
                : (n->kind == N_MCALL) ? genMethodCall(n) : genICall(n);
        if (!v.isRefVal) {
            if (!g_lv_quiet) err(n->line, "expression is not assignable");
            return false;
        }
        out = LV();
        out.k = LV_DEREF;
        out.base = v.s;
        out.ty = v.ty;
        return true;
    }
    default:
        break;
    }
    if (!g_lv_quiet) err(n->line, "expression is not assignable");
    return false;
}

/* ------------------------------------------------------------
 * Constant folding and binary operators
 * ------------------------------------------------------------ */
bool isRel(const string &op)
{
    return op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=";
}

string invRel(const string &op)
{
    if (op == "<")  return ">=";
    if (op == ">")  return "<=";
    if (op == "<=") return ">";
    if (op == ">=") return "<";
    if (op == "==") return "!=";
    return "==";
}

/* Folds a binary operation on two constants; returns false when it cannot be folded. */
bool foldBin(const string &op, const Value &a, const Value &b, Type *rt, Value &out)
{
    if (!a.isConst || !b.isConst) return false;
    bool fl = isFloat(rt) || a.isFloatC || b.isFloatC;
    if (fl) {
        double x = a.isFloatC ? a.dv : (double)a.iv;
        double y = b.isFloatC ? b.dv : (double)b.iv;
        if (isRel(op)) {
            bool r = op == "<" ? x < y : op == ">" ? x > y : op == "<=" ? x <= y :
                     op == ">=" ? x >= y : op == "==" ? x == y : x != y;
            out = cInt(r ? 1 : 0);
            return true;
        }
        if (op == "+") { out = cFlt(x + y, rt); return true; }
        if (op == "-") { out = cFlt(x - y, rt); return true; }
        if (op == "*") { out = cFlt(x * y, rt); return true; }
        if (op == "/" && y != 0) { out = cFlt(x / y, rt); return true; }
        return false;
    }
    long long x = a.iv, y = b.iv;
    if (isRel(op)) {
        bool r = op == "<" ? x < y : op == ">" ? x > y : op == "<=" ? x <= y :
                 op == ">=" ? x >= y : op == "==" ? x == y : x != y;
        out = cInt(r ? 1 : 0);
        return true;
    }
    long long r;
    if (op == "+") r = x + y;
    else if (op == "-") r = x - y;
    else if (op == "*") r = x * y;
    else if (op == "/") { if (y == 0) return false; r = x / y; }
    else if (op == "%") { if (y == 0) return false; r = x % y; }
    else if (op == "&") r = x & y;
    else if (op == "|") r = x | y;
    else if (op == "^") r = x ^ y;
    else if (op == "<<") { if (y < 0 || y > 62) return false; r = x << y; }
    else if (op == ">>") { if (y < 0 || y > 62) return false; r = x >> y; }
    else return false;
    out = cInt(rt->kind == T_INT ? narrowConst(r, rt) : r, rt);
    return true;
}

/* Generates a binary operation: promotion, constant folding, pointer arithmetic and checks. */
Value binaryOp(const string &op, Value a, Value b, int line)
{
    bool pa = isPtrLike(a.ty), pb = isPtrLike(b.ty);

    if (isRel(op)) {
        if (isArith(a.ty) && isArith(b.ty)) {
            Type *rt = commonType(a.ty, b.ty);
            a = convertTo(a, rt); b = convertTo(b, rt);
            Value f;
            if (foldBin(op, a, b, rt, f)) return f;
        } else if (!((pa || isInteger(a.ty)) && (pb || isInteger(b.ty)))) {
            err(line, "invalid operands to comparison");
        }
        string t = newTemp();
        emit(t + " = " + a.s + " " + op + " " + b.s);
        return mkv(t, tInt());
    }

    if ((op == "+" || op == "-") && (pa || pb)) {
        if (op == "-" && pa && pb) {
            long long sz = sizeOf(a.ty->base);
            if (sz < 1) sz = 1;
            string t1 = newTemp();
            emit(t1 + " = " + a.s + " - " + b.s);
            if (sz == 1) return mkv(t1, tLong());
            string t2 = newTemp();
            emit(t2 + " = " + t1 + " / " + std::to_string(sz));
            return mkv(t2, tLong());
        }
        if (op == "+" && pa && pb) { err(line, "cannot add two pointers"); return a; }
        Value p = pa ? a : b;
        Value i = pa ? b : a;
        if (op == "-" && pb) { err(line, "cannot subtract a pointer from an integer"); return a; }
        if (p.ty->kind == T_PTR && p.ty->base && p.ty->base->kind == T_VOID) {
            err(line, "arithmetic on a pointer to 'void'");
            return a;
        }
        if (!isInteger(i.ty)) { err(line, "pointer arithmetic requires an integer operand"); return p; }
        long long sz = sizeOf(p.ty->base);
        if (sz < 1) sz = 1;
        Value off = scaleIdx(i, sz);
        string t = newTemp();
        emit(t + " = " + p.s + " " + op + " " + off.s);
        Type *rt = p.ty->kind == T_ARRAY ? mkPtr(p.ty->base) : p.ty;
        return mkv(t, rt);
    }

    if (!isArith(a.ty) || !isArith(b.ty)) {
        err(line, "invalid operands to binary operator '" + op + "'");
        return cInt(0);
    }
    bool intOnly = (op == "%" || op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>");
    if (intOnly && (isFloat(a.ty) || isFloat(b.ty))) {
        err(line, "invalid operands: '" + op + "' requires integer operands");
        return cInt(0);
    }
    Type *rt;
    if (op == "<<" || op == ">>") {
        rt = promote(a.ty);
        a = convertTo(a, rt);
    } else {
        rt = commonType(a.ty, b.ty);
        a = convertTo(a, rt);
        b = convertTo(b, rt);
    }
    Value f;
    if (foldBin(op, a, b, rt, f)) return f;
    if ((op == "/" || op == "%") && b.isConst && !b.isFloatC && b.iv == 0)
        err(line, "division by zero");
    string t = newTemp();
    emit(t + " = " + a.s + " " + op + " " + b.s);
    return mkv(t, rt);
}

/* ------------------------------------------------------------
 * Functions: signatures, overload resolution, library table
 * ------------------------------------------------------------ */
bool sameSig(const vector<Param> &a, const vector<Param> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (!sameType(a[i].ty, b[i].ty)) return false;
    return true;
}

int argCost(Type *arg, Type *param)
{
    if (sameType(arg, param)) return 0;
    if (isArith(arg) && isArith(param)) return arg->kind == param->kind ? 0 : 1;
    if (arg->kind == T_STRUCT && param->kind == T_STRUCT && derivedFrom(arg, param)) return 10;
    if (isPointer(param) && isPtrLike(arg)) {
        Type *ab = arg->base;
        if (param->base->kind == T_VOID || ab->kind == T_VOID) return 1;
        if (sameType(ab, param->base)) return 0;
        if (ab->kind == T_STRUCT && param->base->kind == T_STRUCT && derivedFrom(ab, param->base)) return 5;
        return 50;
    }
    if (isPointer(param) && isInteger(arg)) return 60;
    return 1000;
}

FuncSig *resolve(vector<FuncSig *> &cands, const vector<Type *> &args, size_t skip = 0)
{
    FuncSig *best = nullptr;
    int bestCost = 1000000;
    for (size_t k = 0; k < cands.size(); k++) {
        FuncSig *f = cands[k];
        if (f->params.size() < skip) continue;
        size_t np = f->params.size() - skip;
        if (f->variadic ? args.size() < np : args.size() != np) continue;
        int cost = 0;
        for (size_t i = 0; i < np; i++) cost += argCost(args[i], f->params[i + skip].ty);
        if (cost < 1000 && cost < bestCost) { best = f; bestCost = cost; }
    }
    return best;
}

string sigTag(const vector<Param> &ps, size_t skip = 0)
{
    if (ps.size() <= skip) return "void";
    string r;
    for (size_t i = skip; i < ps.size(); i++) {
        string t = typeStr(ps[i].ty);
        for (size_t j = 0; j < t.size(); j++) {
            if (t[j] == '*') t[j] = 'p';
            else if (t[j] == ' ') t[j] = '_';
            else if (t[j] == '[' || t[j] == ']') t[j] = '_';
        }
        if (ps[i].ref) t += "_ref";
        r += (i > skip ? "_" : "") + t;
    }
    return r;
}

/* ------------------------------------------------------------
 * Classes: access control, implicit 'this', method lookup
 * ------------------------------------------------------------ */
Type *currentClass() { return curFunc ? curFunc->cls : nullptr; }

void checkAccess(Type *owner, int access, int line, const string &what)
{
    if (access == 0 || !owner) return;
    Type *cc = currentClass();
    bool ok = cc && (access == 2 ? cc == owner : derivedFrom(cc, owner));
    if (!ok)
        err(line, string(access == 2 ? "private" : "protected") + " member '" + what +
                  "' of '" + typeStr(owner) + "' is not accessible from this context");
}

/* Unqualified name inside a method that names a data member: this->name */
bool implicitMember(const string &name, int line, LV &out)
{
    Type *cc = currentClass();
    if (!cc) return false;
    const Field *f = findField(cc, name);
    if (!f) return false;
    VarInfo *self = lookupVar("this");
    if (!self) return false;
    checkAccess(f->owner, f->access, line, name);
    out = LV();
    out.k = LV_FIELD;
    out.base = self->ir;
    out.path = f->name;
    out.arrow = true;
    out.ty = f->ty;
    return true;
}

/* Methods called "name" visible in cls (derived classes hide base ones). */
vector<FuncSig *> *findMethods(Type *cls, const string &name)
{
    for (Type *t = cls; t; t = t->parent) {
        map<string, vector<FuncSig *> >::iterator it = funcs.find(t->tag + "::" + name);
        if (it != funcs.end() && !it->second.empty()) return &it->second;
    }
    return nullptr;
}

Type *funcTypeOf(FuncSig *f, size_t skip)
{
    vector<Type *> pt;
    for (size_t i = skip; i < f->params.size(); i++) pt.push_back(f->params[i].ty);
    return mkFunc(f->ret, pt, f->variadic);
}

/* Registers the library functions known to the compiler with their return and argument types. */
void initLib()
{
    Type *I = tInt(), *D = tDouble(), *V = tVoid();
    Type *S = mkPtr(tChar()), *P = mkPtr(tVoid());
    Type *L = tLong(), *UL = mkBase(T_LONG, true);
    Type *F = mkPtr(g_typedefs["FILE"]);
    lib["printf"]  = LibFn{I, true, 1, nullptr};
    lib["scanf"]   = LibFn{I, true, 1, nullptr};
    lib["sprintf"] = LibFn{I, true, 2, nullptr};
    lib["sscanf"]  = LibFn{I, true, 2, nullptr};
    lib["fprintf"] = LibFn{I, true, 2, nullptr};
    lib["fscanf"]  = LibFn{I, true, 2, nullptr};
    lib["puts"]    = LibFn{I, false, 1, nullptr};
    lib["putchar"] = LibFn{I, false, 1, nullptr};
    lib["getchar"] = LibFn{I, false, 0, nullptr};
    lib["gets"]    = LibFn{S, false, 1, nullptr};
    lib["fgets"]   = LibFn{S, false, 3, nullptr};
    lib["fputs"]   = LibFn{I, false, 2, nullptr};
    lib["fputc"]   = LibFn{I, false, 2, nullptr};
    lib["fgetc"]   = LibFn{I, false, 1, nullptr};
    lib["fopen"]   = LibFn{F, false, 2, nullptr};
    lib["fclose"]  = LibFn{I, false, 1, nullptr};
    lib["fread"]   = LibFn{UL, false, 4, nullptr};
    lib["fwrite"]  = LibFn{UL, false, 4, nullptr};
    lib["fseek"]   = LibFn{I, false, 3, nullptr};
    lib["ftell"]   = LibFn{L, false, 1, nullptr};
    lib["rewind"]  = LibFn{V, false, 1, nullptr};
    lib["feof"]    = LibFn{I, false, 1, nullptr};
    lib["fflush"]  = LibFn{I, false, 1, nullptr};
    lib["remove"]  = LibFn{I, false, 1, nullptr};
    lib["rename"]  = LibFn{I, false, 2, nullptr};
    lib["memset"]  = LibFn{P, false, 3, nullptr};
    lib["memcpy"]  = LibFn{P, false, 3, nullptr};
    lib["memmove"] = LibFn{P, false, 3, nullptr};
    lib["realloc"] = LibFn{P, false, 2, nullptr};
    lib["strlen"]  = LibFn{UL, false, 1, nullptr};
    lib["strcpy"]  = LibFn{S, false, 2, nullptr};
    lib["strncpy"] = LibFn{S, false, 3, nullptr};
    lib["strcat"]  = LibFn{S, false, 2, nullptr};
    lib["strcmp"]  = LibFn{I, false, 2, nullptr};
    lib["strncmp"] = LibFn{I, false, 3, nullptr};
    lib["strchr"]  = LibFn{S, false, 2, nullptr};
    lib["strstr"]  = LibFn{S, false, 2, nullptr};
    lib["atoi"]    = LibFn{I, false, 1, nullptr};
    lib["atol"]    = LibFn{L, false, 1, nullptr};
    lib["atof"]    = LibFn{D, false, 1, nullptr};
    lib["abs"]     = LibFn{I, false, 1, I};
    lib["labs"]    = LibFn{L, false, 1, L};
    const char *math1[] = {"sqrt", "sin", "cos", "tan", "fabs", "floor", "ceil", "log", "exp"};
    for (size_t i = 0; i < sizeof(math1) / sizeof(math1[0]); i++)
        lib[math1[i]] = LibFn{D, false, 1, D};
    lib["pow"]     = LibFn{D, false, 2, D};
    lib["qsort"]   = LibFn{V, false, 4, nullptr};
    lib["strdup"]  = LibFn{S, false, 1, nullptr};
    lib["strrchr"] = LibFn{S, false, 2, nullptr};
    lib["strncat"] = LibFn{S, false, 3, nullptr};
    lib["strtol"]  = LibFn{L, false, 3, nullptr};
    lib["ungetc"]  = LibFn{I, false, 2, nullptr};
    lib["perror"]  = LibFn{V, false, 1, nullptr};
    lib["fmod"]    = LibFn{D, false, 2, D};
    lib["atan"]    = LibFn{D, false, 1, D};
    lib["log10"]   = LibFn{D, false, 1, D};
    lib["round"]   = LibFn{D, false, 1, D};
    lib["isxdigit"] = LibFn{I, false, 1, nullptr};
    lib["ispunct"] = LibFn{I, false, 1, nullptr};
    lib["clock"]   = LibFn{L, false, 0, nullptr};
    lib["rand"]    = LibFn{I, false, 0, nullptr};
    lib["srand"]   = LibFn{V, false, 1, nullptr};
    lib["exit"]    = LibFn{V, false, 1, nullptr};
    lib["time"]    = LibFn{L, false, 1, nullptr};
    const char *ctype[] = {"toupper", "tolower", "isdigit", "isalpha", "isspace",
                           "isalnum", "isupper", "islower"};
    for (size_t i = 0; i < sizeof(ctype) / sizeof(ctype[0]); i++)
        lib[ctype[i]] = LibFn{I, false, 1, nullptr};
}

Value finishCall(const string &ir, size_t argc, Type *ret)
{
    if (ret->kind == T_VOID) {
        emit("call " + ir + ", " + std::to_string(argc));
        return mkv("", ret);
    }
    string t = newTemp();
    emit(t + " = call " + ir + ", " + std::to_string(argc));
    return mkv(t, ret);
}

/* Like finishCall, but the result holds the *address* of the returned object. */
Value finishCallRef(const string &ir, size_t argc, Type *ret)
{
    string t = newTemp();
    emit(t + " = call " + ir + ", " + std::to_string(argc));
    Value v;
    v.s = t;
    v.ty = ret;
    v.isRefVal = true;
    return v;
}

Value derefRefVal(const Value &v)
{
    string t = newTemp();
    emit(t + " = *" + v.s);
    return mkv(t, v.ty);
}

/* Emits the 'param' lines and the call itself.  'prefix' holds the hidden
 * arguments already evaluated (this pointer, lambda captures); 'off' says how
 * many hidden parameters the callee has in front of the declared ones. */
Value emitUserCall(FuncSig *f, vector<Node *> &argNodes, size_t off, const vector<string> &prefix, int line)
{
    vector<string> args = prefix;
    for (size_t i = 0; i < argNodes.size(); i++) {
        if (i + off < f->params.size()) {
            const Param &p = f->params[i + off];
            if (p.ref) {
                LV lv;
                if (!genLV(argNodes[i], lv)) { args.push_back("0"); continue; }
                args.push_back(addrOf(lv).s);
            } else {
                Value v = genExpr(argNodes[i]);
                if (v.ty->kind == T_VOID) { err(line, "void value passed as argument"); v = cInt(0); }
                v = convertTo(v, p.ty);
                args.push_back(v.s);
            }
        } else {
            Value v = genExpr(argNodes[i]);
            args.push_back(v.s);
        }
    }
    for (size_t i = 0; i < args.size(); i++) emit("param " + args[i]);
    return f->retRef ? finishCallRef(f->ir, args.size(), f->ret)
                     : finishCall(f->ir, args.size(), f->ret);
}

/* Call through a pointer to function. */
Value emitIndirectCall(const Value &fp, Type *ft, vector<Node *> &argNodes, int line)
{
    if ((!ft->fvariadic && argNodes.size() != ft->ptys.size()) || argNodes.size() < ft->ptys.size()) {
        err(line, "wrong number of arguments in call through function pointer");
        return cInt(0);
    }
    vector<string> args;
    for (size_t i = 0; i < argNodes.size(); i++) {
        Value v = genExpr(argNodes[i]);
        if (v.ty->kind == T_VOID) { err(line, "void value passed as argument"); v = cInt(0); }
        if (i < ft->ptys.size()) v = convertTo(v, ft->ptys[i]);
        args.push_back(v.s);
    }
    for (size_t i = 0; i < args.size(); i++) emit("param " + args[i]);
    return finishCall("*" + fp.s, args.size(), ft->base);
}

/* Generates a member function call through an object or a pointer, passing this as first argument. */
Value genMethodCall(Node *n)
{
    Node *objN = n->kids[0];
    const string &name = n->s;
    Value self;
    Type *cls = nullptr;
    LV olv;
    if (n->flag) {                                            /* p->method() */
        self = genExpr(objN);
        if (!isPointer(self.ty) || self.ty->base->kind != T_STRUCT) {
            err(n->line, "member reference with '->' requires a pointer to a class");
            return cInt(0);
        }
        cls = self.ty->base;
    } else {                                                  /* obj.method() */
        if (!genLV(objN, olv)) return cInt(0);
        if (olv.ty->kind != T_STRUCT) { err(n->line, "member reference with '.' requires a class or struct"); return cInt(0); }
        cls = olv.ty;
    }
    vector<FuncSig *> *cands = findMethods(cls, name);
    if (!cands) {
        /* a data member holding a function pointer? */
        const Field *fld = findField(cls, name);
        if (fld && isPointer(fld->ty) && fld->ty->base->kind == T_FUNC) {
            checkAccess(fld->owner, fld->access, n->line, name);
            LV fl;
            if (n->flag) {
                fl.k = LV_FIELD; fl.base = self.s; fl.path = fld->name; fl.arrow = true; fl.ty = fld->ty;
            } else {
                fl = memberOf(olv, *fld);
            }
            Value fp = loadLV(fl);
            return emitIndirectCall(fp, fld->ty->base, n->extra, n->line);
        }
        err(n->line, "no member function named '" + name + "' in '" + typeStr(cls) + "'");
        return cInt(0);
    }
    vector<Type *> at;
    for (size_t i = 0; i < n->extra.size(); i++) at.push_back(typeOf(n->extra[i]));
    FuncSig *f = resolve(*cands, at, 1);
    if (!f) {
        err(n->line, "no matching member function for call to '" + name + "' with " +
                     std::to_string(n->extra.size()) + " argument(s)");
        return cInt(0);
    }
    checkAccess(f->cls, f->access, n->line, name + "()");
    if (!n->flag) self = addrOf(olv);
    vector<string> pre;
    pre.push_back(self.s);
    return emitUserCall(f, n->extra, 1, pre, n->line);
}

/* Call through an arbitrary expression: fp(x), (*fp)(x), arr[i](x), f()(x), obj.m(x). */
Value genICall(Node *n)
{
    Node *callee = n->kids[0];
    vector<Node *> args(n->kids.begin() + 1, n->kids.end());
    if (callee->kind == N_MEMBER) {                      /* obj.m(args) / p->m(args) */
        Node tmp = *callee;
        tmp.kind = N_MCALL;
        tmp.kids.assign(1, callee->kids[0]);
        tmp.extra = args;
        return genMethodCall(&tmp);
    }
    Value fp = genExpr(callee);
    if (fp.lam) {                                        /* lambda obtained indirectly */
        size_t ncap = fp.capIr.size();
        vector<Type *> at;
        for (size_t i = 0; i < args.size(); i++) at.push_back(typeOf(args[i]));
        vector<FuncSig *> one(1, fp.lam);
        if (!resolve(one, at, ncap)) {
            err(n->line, "no matching call to lambda with " + std::to_string(args.size()) + " argument(s)");
            return cInt(0);
        }
        return emitUserCall(fp.lam, args, ncap, fp.capIr, n->line);
    }
    if (isPointer(fp.ty) && fp.ty->base->kind == T_FUNC)
        return emitIndirectCall(fp, fp.ty->base, args, n->line);
    if (fp.ty->kind == T_FUNC)                           /* (*fp)(...) - dereferenced function pointer */
        return emitIndirectCall(fp, fp.ty, args, n->line);
    err(n->line, "called object is not a function or function pointer");
    return cInt(0);
}

/* Names used anywhere inside a statement tree (for lambda captures). */
void collectIds(Node *n, set<string> &out)
{
    if (!n) return;
    if (n->kind == N_ID) out.insert(n->s);
    for (size_t i = 0; i < n->kids.size(); i++) collectIds(n->kids[i], out);
    for (size_t i = 0; i < n->extra.size(); i++) collectIds(n->extra[i], out);
}

Node *firstReturn(Node *n)
{
    if (!n) return nullptr;
    if (n->kind == N_RETURN) return n;
    if (n->kind == N_LAMBDA) return nullptr;
    for (size_t i = 0; i < n->kids.size(); i++) {
        Node *r = firstReturn(n->kids[i]);
        if (r) return r;
    }
    return nullptr;
}

/* Lowers a lambda expression to a function whose captured variables become extra parameters. */
Value genLambda(Node *n)
{
    struct Cap { string name; bool byRef; Type *ty; bool isRefVar; };
    vector<Cap> caps;
    char defMode = 0;
    map<string, bool> explicitCaps;
    for (size_t i = 0; i < n->extra.size(); i++) {
        Node *c = n->extra[i];
        if (c->s == "=") defMode = '=';
        else if (c->s == "&") defMode = '&';
        else explicitCaps[c->s] = c->flag;
    }
    set<string> pnames;
    for (size_t i = 0; i < n->params.size(); i++) pnames.insert(n->params[i].name);
    set<string> used;
    collectIds(n->kids[0], used);
    for (map<string, bool>::iterator it = explicitCaps.begin(); it != explicitCaps.end(); ++it) used.insert(it->first);
    for (set<string>::iterator it = used.begin(); it != used.end(); ++it) {
        if (pnames.count(*it)) continue;
        VarInfo *vi = lookupVarLocal(*it);
        if (!vi) continue;
        bool byRef;
        map<string, bool>::iterator ex = explicitCaps.find(*it);
        if (ex != explicitCaps.end()) byRef = ex->second;
        else if (defMode) byRef = (defMode == '&');
        else continue;
        Cap c; c.name = *it; c.byRef = byRef; c.ty = vi->ty; c.isRefVar = vi->isRef;
        caps.push_back(c);
    }

    FuncSig *f = new FuncSig();
    f->name = "__lambda" + std::to_string(lamCount + 1);
    f->ir = f->name;
    f->body = n->kids[0];
    f->node = n;
    f->line = n->line;
    for (size_t i = 0; i < caps.size(); i++) {
        Param p; p.ty = caps[i].ty; p.name = caps[i].name; p.ref = caps[i].byRef;
        f->params.push_back(p);
    }
    for (size_t i = 0; i < n->params.size(); i++) f->params.push_back(n->params[i]);

    /* return type: explicit, or deduced from the first 'return expr' */
    if (n->ty) {
        f->ret = n->ty;
    } else {
        Node *r = firstReturn(n->kids[0]);
        if (r && r->kids[0]) {
            pushScope();
            for (size_t i = 0; i < f->params.size(); i++) {
                VarInfo v; v.ty = f->params[i].ty; v.ir = f->params[i].name; v.isRef = f->params[i].ref;
                scopes.back()[f->params[i].name] = v;
            }
            f->ret = typeOf(r->kids[0]);
            popScope();
        } else {
            f->ret = tVoid();
        }
    }
    size_t ncap = caps.size();
    Value v = mkv(f->ir, mkPtr(funcTypeOf(f, ncap)));
    v.lam = f;
    if (dryDepth) return v;

    lamCount++;
    lambdaQ.push_back(f);
    for (size_t i = 0; i < caps.size(); i++) {
        string slot = "__cap" + std::to_string(lamCount) + "_" + caps[i].name;
        VarInfo *vi = lookupVarLocal(caps[i].name);
        if (caps[i].byRef) {
            emit("local " + declStr(mkPtr(caps[i].ty), slot));
            emit(slot + " = " + addrOf(lvOfVar(*vi)).s);
        } else {
            emit("local " + declStr(caps[i].ty, slot));
            emit(slot + " = " + loadLV(lvOfVar(*vi)).s);
        }
        v.capIr.push_back(slot);
    }
    return v;
}

/* Generates calls: user functions with overload resolution, constructors, library and indirect calls. */
Value genCall(Node *n)
{
    const string &name = n->s;
    size_t argc = n->kids.size();

    /* variables (lambdas, function pointers) hide functions */
    VarInfo *cv = lookupVar(name);
    if (cv) {
        if (cv->lam) {
            FuncSig *f = cv->lam;
            size_t ncap = cv->capIr.size();
            vector<Type *> at;
            for (size_t i = 0; i < argc; i++) at.push_back(typeOf(n->kids[i]));
            vector<FuncSig *> one(1, f);
            if (!resolve(one, at, ncap)) {
                err(n->line, "no matching call to lambda '" + name + "' with " + std::to_string(argc) + " argument(s)");
                return cInt(0);
            }
            return emitUserCall(f, n->kids, ncap, cv->capIr, n->line);
        }
        if (isPointer(cv->ty) && cv->ty->base->kind == T_FUNC) {
            Value fp = loadLV(lvOfVar(*cv));
            return emitIndirectCall(fp, cv->ty->base, n->kids, n->line);
        }
        err(n->line, "'" + name + "' is not a function");
        return cInt(0);
    }

    /* methods of the current class (implicit this) */
    if (currentClass()) {
        vector<FuncSig *> *mc = findMethods(currentClass(), name);
        if (mc) {
            vector<Type *> at;
            for (size_t i = 0; i < argc; i++) at.push_back(typeOf(n->kids[i]));
            FuncSig *f = resolve(*mc, at, 1);
            if (!f) {
                err(n->line, "no matching member function for call to '" + name + "' with " +
                             std::to_string(argc) + " argument(s)");
                return cInt(0);
            }
            checkAccess(f->cls, f->access, n->line, name + "()");
            vector<string> pre;
            pre.push_back(lookupVar("this")->ir);
            return emitUserCall(f, n->kids, 1, pre, n->line);
        }
    }

    /* user-defined functions take priority */
    map<string, vector<FuncSig *> >::iterator it = funcs.find(name);
    if (it != funcs.end()) {
        /* a constructor / base-constructor call "Class::Class" carries 'this' as its first argument */
        bool isCtorCall = name.find("::") != string::npos;
        vector<Type *> at;
        for (size_t i = isCtorCall ? 1 : 0; i < argc; i++) at.push_back(typeOf(n->kids[i]));
        FuncSig *f = resolve(it->second, at, isCtorCall ? 1 : 0);
        if (!f) {
            err(n->line, "no matching function for call to '" + name + "' with " +
                         std::to_string(isCtorCall ? argc - 1 : argc) + " argument(s)");
            return cInt(0);
        }
        if (isCtorCall) {
            vector<string> pre;
            pre.push_back(genExpr(n->kids[0]).s);
            vector<Node *> rest(n->kids.begin() + 1, n->kids.end());
            return emitUserCall(f, rest, 1, pre, n->line);
        }
        return emitUserCall(f, n->kids, 0, vector<string>(), n->line);
    }

    /* variadic argument helpers */
    if (name == "va_start") {
        if (argc != 2 || n->kids[1]->kind != N_ID) { err(n->line, "invalid use of 'va_start'"); return cInt(0); }
        if (!curFunc || !curFunc->variadic) { err(n->line, "'va_start' used in a function that is not variadic"); return cInt(0); }
        LV ap;
        VarInfo *last = lookupVar(n->kids[1]->s);
        if (!genLV(n->kids[0], ap)) return cInt(0);
        if (!last) { err(n->line, "use of undeclared identifier '" + n->kids[1]->s + "'"); return cInt(0); }
        emit(lvText(ap) + " = va_start " + last->ir);
        return mkv("", tVoid());
    }
    if (name == "va_end") {
        if (argc != 1) { err(n->line, "wrong number of arguments to 'va_end'"); return cInt(0); }
        Value ap = genExpr(n->kids[0]);
        emit("va_end " + ap.s);
        return mkv("", tVoid());
    }

    /* memory management */
    if (name == "malloc" || name == "calloc") {
        size_t need = (name == "malloc") ? 1 : 2;
        if (argc != need) { err(n->line, "wrong number of arguments to '" + name + "'"); return cInt(0); }
        Value sz = genExpr(n->kids[0]);
        if (need == 2) sz = binaryOp("*", sz, genExpr(n->kids[1]), n->line);
        string t = newTemp();
        emit(t + " = alloc " + sz.s);
        return mkv(t, mkPtr(tVoid()));
    }
    if (name == "free") {
        if (argc != 1) { err(n->line, "wrong number of arguments to 'free'"); return cInt(0); }
        Value p = genExpr(n->kids[0]);
        emit("free " + p.s);
        return mkv("", tVoid());
    }

    /* standard library */
    map<string, LibFn>::iterator li = lib.find(name);
    if (li != lib.end()) {
        const LibFn &lf = li->second;
        if (lf.variadic ? (int)argc < lf.nargs : (int)argc != lf.nargs) {
            err(n->line, "wrong number of arguments to '" + name + "'");
            return cInt(0);
        }
        vector<string> args;
        for (size_t i = 0; i < argc; i++) {
            Value v = genExpr(n->kids[i]);
            if (lf.argTy) v = convertTo(v, lf.argTy);
            args.push_back(v.s);
        }
        for (size_t i = 0; i < args.size(); i++) emit("param " + args[i]);
        return finishCall(name, argc, lf.ret);
    }

    err(n->line, "call to undeclared function '" + name + "'");
    return cInt(0);
}

/* ------------------------------------------------------------
 * Stream input and output (cout / cin)
 * ------------------------------------------------------------ */
/* Generates one cout << expr or cin >> lvalue step by dispatching to the typed runtime helper. */
Value genStream(Node *n, const Value &s, bool out)
{
    Node *r = n->kids[1];
    if (out) {
        if (r->kind == N_ENDL) {
            emit("call __print_endl, 0");
            return s;
        }
        Value v = genExpr(r);
        Type *t = v.ty;
        string fn = "__print_int";
        if (isPointer(t) && t->base->kind == T_CHAR) fn = "__print_str";
        else if (isPointer(t)) fn = "__print_ptr";
        else if (t->kind == T_CHAR) fn = "__print_char";
        else if (t->kind == T_BOOL) fn = "__print_bool";
        else if (isFloat(t)) fn = "__print_double";
        else if (t->kind == T_LONG) fn = "__print_long";
        else if (!isInteger(t)) { err(n->line, "value of type '" + typeStr(t) + "' cannot be written to cout"); return s; }
        emit("param " + v.s);
        emit("call " + fn + ", 1");
        return s;
    }
    LV lv;
    if (!genLV(r, lv)) return s;
    Type *t = lv.ty;
    string fn;
    string arg;
    if (t->kind == T_ARRAY && t->base->kind == T_CHAR) { fn = "__read_str"; arg = loadLV(lv).s; }
    else if (isPointer(t) && t->base->kind == T_CHAR) { fn = "__read_str"; arg = loadLV(lv).s; }
    else if (t->kind == T_CHAR)  { fn = "__read_char";   arg = addrOf(lv).s; }
    else if (isFloat(t))         { fn = "__read_double"; arg = addrOf(lv).s; }
    else if (t->kind == T_LONG)  { fn = "__read_long";   arg = addrOf(lv).s; }
    else if (isInteger(t))       { fn = "__read_int";    arg = addrOf(lv).s; }
    else { err(n->line, "value of type '" + typeStr(t) + "' cannot be read from cin"); return s; }
    emit("param " + arg);
    emit("call " + fn + ", 1");
    return s;
}

/* ------------------------------------------------------------
 * Expressions
 * ------------------------------------------------------------ */
/* Generates unary operators (-, +, ~, !, *, &, prefix/postfix ++ and --) with constant folding. */
Value genUnary(Node *n)
{
    const string &op = n->s;
    Node *k = n->kids[0];

    if (op == "-" || op == "+" || op == "~" || op == "!") {
        Value v = genExpr(k);
        if (op == "!") {
            if (v.isConst) return cInt(constTruth(v) ? 0 : 1);
            if (!isArith(v.ty) && !isPointer(v.ty)) { err(n->line, "invalid operand to '!'"); return cInt(0); }
            string t = newTemp();
            emit(t + " = !" + v.s);
            return mkv(t, tInt());
        }
        if (!isArith(v.ty)) { err(n->line, "invalid operand to unary '" + op + "'"); return cInt(0); }
        if (op == "~" && isFloat(v.ty)) { err(n->line, "invalid operand to '~'"); return cInt(0); }
        v = convertTo(v, promote(v.ty));
        if (op == "+") return v;
        if (v.isConst) {
            if (op == "-") return v.isFloatC ? cFlt(-v.dv, v.ty) : cInt(narrowConst(-v.iv, v.ty), v.ty);
            return cInt(narrowConst(~v.iv, v.ty), v.ty);
        }
        string t = newTemp();
        emit(t + " = " + op + v.s);
        return mkv(t, v.ty);
    }

    if (op == "*") {
        LV lv;
        if (!genLV(n, lv)) return cInt(0);
        return loadLV(lv);
    }

    if (op == "&") {
        if (k->kind == N_ID && !lookupVar(k->s) && funcs.count(k->s) && !funcs[k->s].empty()) {
            FuncSig *f = funcs[k->s][0];
            return mkv(f->ir, mkPtr(funcTypeOf(f)));
        }
        LV lv;
        if (!genLV(k, lv)) return mkv("0", mkPtr(tVoid()));
        return addrOf(lv);
    }

    /* ++ -- (prefix) and p++ p-- (postfix) */
    bool post = (op == "p++" || op == "p--");
    bool inc  = (op == "++" || op == "p++");
    LV lv;
    if (!genLV(k, lv)) return cInt(0);
    if (!isArith(lv.ty) && !isPointer(lv.ty)) { err(n->line, "invalid operand to increment/decrement"); return cInt(0); }
    if (k->kind == N_ID) {
        VarInfo *vi = lookupVar(k->s);
        if (vi && vi->isConst) err(n->line, "cannot modify read-only variable '" + k->s + "'");
    }
    Value old = loadLV(lv);
    Value snap = old;
    if (post && lv.k == LV_VAR) {
        string t = newTemp();
        emit(t + " = " + old.s);
        snap = mkv(t, old.ty);
    }
    Value r = binaryOp(inc ? "+" : "-", old, cInt(1), n->line);
    r = convertTo(r, lv.ty);
    storeLV(lv, r);
    return post ? snap : r;
}

/* Generates plain and compound assignments, including struct copies. */
Value genAssign(Node *n)
{
    string op = n->s;
    LV lv;
    if (!genLV(n->kids[0], lv)) { genExpr(n->kids[1]); return cInt(0); }
    if (lv.ty->kind == T_ARRAY) { err(n->line, "array type is not assignable"); return cInt(0); }
    if (n->kids[0]->kind == N_ID) {
        VarInfo *vi = lookupVar(n->kids[0]->s);
        if (vi && vi->isConst) err(n->line, "cannot assign to read-only variable '" + n->kids[0]->s + "'");
    }
    if (op == "=") {
        Value r = genExpr(n->kids[1]);
        if (r.ty->kind == T_VOID) { err(n->line, "cannot assign a void value"); return cInt(0); }
        if (r.lam && n->kids[0]->kind == N_ID) {
            VarInfo *vi = lookupVar(n->kids[0]->s);
            if (vi) { vi->lam = r.lam; vi->capIr = r.capIr; }
        }
        if (lv.ty->kind == T_STRUCT && !sameType(lv.ty, r.ty)) {
            err(n->line, "incompatible types in struct assignment");
            return r;
        }
        if (ptrConvError(lv.ty, r, n->line)) return cInt(0);
        r = convertTo(r, lv.ty);
        storeLV(lv, r);
        return r;
    }
    string bop = op.substr(0, op.size() - 1);
    Value curv = loadLV(lv);
    Value r = genExpr(n->kids[1]);
    Value res = binaryOp(bop, curv, r, n->line);
    res = convertTo(res, lv.ty);
    storeLV(lv, res);
    return res;
}

Value genCondExpr(Node *n)
{
    Type *ta = typeOf(n->kids[1]);
    Type *tb = typeOf(n->kids[2]);
    Type *rt = (isArith(ta) && isArith(tb)) ? commonType(ta, tb) : ta;
    CVal cc = constEval(n->kids[0]);
    if (cc.ok) {                                   /* constant condition: only the taken branch runs */
        int taken = (cc.isf ? cc.d : (double)cc.i) != 0 ? 1 : 2;
        return convertTo(genExpr(n->kids[taken]), rt);
    }
    string Lf = newLabel(), Le = newLabel(), t = newTemp();
    genCond(n->kids[0], Lf, false);
    Value a = convertTo(genExpr(n->kids[1]), rt);
    emit(t + " = " + a.s);
    emit("goto " + Le);
    emitLabel(Lf);
    Value b = convertTo(genExpr(n->kids[2]), rt);
    emit(t + " = " + b.s);
    emitLabel(Le);
    return mkv(t, rt);
}

/* Generates binary expressions, including short-circuit &&/||, stream shifts and arithmetic. */
Value genBinary(Node *n)
{
    const string &op = n->s;
    if (op == "&&" || op == "||") {
        CVal c = constEval(n->kids[0]);
        if (c.ok) {                                /* constant left side: short-circuit folds */
            bool truth = (c.isf ? c.d : (double)c.i) != 0;
            if (op == "&&") return truth ? convertTo(genExpr(n->kids[1]), tBool()) : cInt(0);
            return truth ? cInt(1) : convertTo(genExpr(n->kids[1]), tBool());
        }
        string Lf = newLabel(), Le = newLabel(), t = newTemp();
        genCond(n, Lf, false);
        emit(t + " = 1");
        emit("goto " + Le);
        emitLabel(Lf);
        emit(t + " = 0");
        emitLabel(Le);
        return mkv(t, tInt());
    }
    Value a = genExpr(n->kids[0]);
    if (a.ty && a.ty->kind == T_STREAM) {
        if (op == "<<") return genStream(n, a, true);
        if (op == ">>") return genStream(n, a, false);
    }
    Value b = genExpr(n->kids[1]);
    if (a.ty->kind == T_VOID || b.ty->kind == T_VOID) {
        err(n->line, "void value used in an expression");
        return cInt(0);
    }
    return binaryOp(op, a, b, n->line);
}

/* Dispatches on the expression node kind and generates its code. */
Value genExpr(Node *n)
{
    switch (n->kind) {
    case N_INT:
        return cInt(n->iv, (n->s == "L" || n->iv > 2147483647LL || n->iv < -2147483648LL) ? tLong() : tInt());
    case N_FLOAT: return cFlt(n->dv, n->s == "f" ? tFloat() : tDouble());
    case N_CHAR:  return cChar(n->iv);
    case N_BOOL:  return cInt(n->iv, tBool());
    case N_NULL:  return cInt(0, mkPtr(tVoid()));
    case N_STR:   return mkv(quoteStr(n->s), mkPtr(tChar()));
    case N_ID: {
        VarInfo *v = lookupVar(n->s);
        if (!v) {
            LV im;
            if (implicitMember(n->s, n->line, im)) return loadLV(im);
            map<string, long long>::iterator e = g_enum_consts.find(n->s);
            if (e != g_enum_consts.end()) return cInt(e->second);
            map<string, vector<FuncSig *> >::iterator fi = funcs.find(n->s);
            if (fi != funcs.end() && !fi->second.empty()) {        /* function name decays to a pointer */
                FuncSig *f = fi->second[0];
                return mkv(f->ir, mkPtr(funcTypeOf(f)));
            }
            err(n->line, "use of undeclared identifier '" + n->s + "'");
            return cInt(0);
        }
        Value lv = loadLV(lvOfVar(*v));
        lv.lam = v->lam;
        lv.capIr = v->capIr;
        return lv;
    }
    case N_BIN:    return genBinary(n);
    case N_UN:     return genUnary(n);
    case N_ASSIGN: return genAssign(n);
    case N_COND:   return genCondExpr(n);
    case N_COMMA:
        genExpr(n->kids[0]);
        return genExpr(n->kids[1]);
    case N_CALL: {
        Value v = genCall(n);
        return v.isRefVal ? derefRefVal(v) : v;
    }
    case N_MCALL: {
        Value v = genMethodCall(n);
        return v.isRefVal ? derefRefVal(v) : v;
    }
    case N_ICALL: {
        Value v = genICall(n);
        return v.isRefVal ? derefRefVal(v) : v;
    }
    case N_INDEX:
    case N_MEMBER: {
        LV lv;
        if (!genLV(n, lv)) return cInt(0);
        return loadLV(lv);
    }
    case N_CAST: {
        Value v = genExpr(n->kids[0]);
        if (v.ty->kind == T_VOID) { err(n->line, "cannot cast a void value"); return cInt(0); }
        if (n->ty->kind == T_STRUCT || n->ty->kind == T_ARRAY) {
            err(n->line, "invalid cast to '" + typeStr(n->ty) + "'");
            return v;
        }
        v.isCast = true;
        return convertTo(v, n->ty);
    }
    case N_SIZEOF_T:
        return cInt(sizeOf(n->ty), mkBase(T_LONG, true));
    case N_SIZEOF_E:
        return cInt(sizeOf(typeOfNoDecay(n->kids[0])), mkBase(T_LONG, true));
    case N_NEW: {
        Type *t = n->ty;
        long long sz = sizeOf(t);
        Value bytes = cInt(sz, tLong());
        if (n->flag) {
            Value cnt = genExpr(n->kids[0]);
            if (!isInteger(cnt.ty)) { err(n->line, "array size in 'new' must be an integer"); return cInt(0); }
            bytes = binaryOp("*", cnt, cInt(sz, tLong()), n->line);
        }
        string tmp = newTemp();
        emit(tmp + " = alloc " + bytes.s);
        Value p = mkv(tmp, mkPtr(t));
        if (t->kind == T_STRUCT && !n->flag) {                   /* run the constructor */
            map<string, vector<FuncSig *> >::iterator ci = funcs.find(t->tag + "::" + t->tag);
            if (ci != funcs.end()) {
                vector<Type *> at;
                for (size_t i = 0; i < n->extra.size(); i++) at.push_back(typeOf(n->extra[i]));
                FuncSig *f = resolve(ci->second, at, 1);
                if (!f) {
                    err(n->line, "no matching constructor for '" + typeStr(t) + "' with " +
                                 std::to_string(n->extra.size()) + " argument(s)");
                    return p;
                }
                vector<string> pre;
                pre.push_back(tmp);
                emitUserCall(f, n->extra, 1, pre, n->line);
                return p;
            }
            if (!n->extra.empty()) { err(n->line, "'" + typeStr(t) + "' has no constructor taking arguments"); return p; }
        }
        if (n->kids[1]) {
            Value iv = convertTo(genExpr(n->kids[1]), t);
            emit("*" + tmp + " = " + iv.s);
        }
        return p;
    }
    case N_LAMBDA:  return genLambda(n);
    case N_VAARG: {
        Value ap = genExpr(n->kids[0]);
        string t = newTemp();
        emit(t + " = va_arg " + ap.s + ", " + typeStr(n->ty));
        return mkv(t, n->ty);
    }
    case N_COUT: return mkv("cout", tStream());
    case N_CIN:  return mkv("cin", tStream());
    case N_ENDL:
        err(n->line, "'endl' may only appear on the right of '<<'");
        return cInt(0);
    default:
        err(n->line, "unsupported expression");
        return cInt(0);
    }
}

/* ------------------------------------------------------------
 * Conditions: jumps with short-circuit evaluation
 * ------------------------------------------------------------ */
/* Emits conditional branches for boolean expressions using short-circuit evaluation. */
void genCond(Node *e, const string &target, bool whenTrue)
{
    if (e->kind == N_UN && e->s == "!") {
        genCond(e->kids[0], target, !whenTrue);
        return;
    }
    if (e->kind == N_BIN) {
        const string &op = e->s;
        if (op == "&&") {
            if (!whenTrue) {
                genCond(e->kids[0], target, false);
                genCond(e->kids[1], target, false);
            } else {
                string skip = newLabel();
                genCond(e->kids[0], skip, false);
                genCond(e->kids[1], target, true);
                emitLabel(skip);
            }
            return;
        }
        if (op == "||") {
            if (whenTrue) {
                genCond(e->kids[0], target, true);
                genCond(e->kids[1], target, true);
            } else {
                string skip = newLabel();
                genCond(e->kids[0], skip, true);
                genCond(e->kids[1], target, false);
                emitLabel(skip);
            }
            return;
        }
        if (isRel(op)) {
            Value a = genExpr(e->kids[0]);
            Value b = genExpr(e->kids[1]);
            if (a.ty->kind == T_VOID || b.ty->kind == T_VOID) {
                err(e->line, "void value used in a condition");
                return;
            }
            Type *rt = tInt();
            if (isArith(a.ty) && isArith(b.ty)) {
                rt = commonType(a.ty, b.ty);
                a = convertTo(a, rt);
                b = convertTo(b, rt);
            } else if (!((isPtrLike(a.ty) || isInteger(a.ty)) && (isPtrLike(b.ty) || isInteger(b.ty)))) {
                err(e->line, "invalid operands to comparison");
                return;
            }
            Value f;
            if (foldBin(op, a, b, rt, f)) {
                if ((f.iv != 0) == whenTrue) emit("goto " + target);
                return;
            }
            emit("if " + a.s + " " + (whenTrue ? op : invRel(op)) + " " + b.s + " goto " + target);
            return;
        }
    }
    Value v = genExpr(e);
    if (v.ty->kind == T_VOID) { err(e->line, "void value used in a condition"); return; }
    if (v.isConst) {
        if (constTruth(v) == whenTrue) emit("goto " + target);
        return;
    }
    emit(string(whenTrue ? "if " : "ifFalse ") + v.s + " goto " + target);
}

/* ------------------------------------------------------------
 * Initialisers
 * ------------------------------------------------------------ */
long long scalarCount(Type *t)
{
    if (t->kind == T_ARRAY) return (t->len < 0 ? 0 : t->len) * scalarCount(t->base);
    if (t->kind == T_STRUCT) {
        long long c = 0;
        for (size_t i = 0; i < t->fields.size(); i++) {
            long long f = scalarCount(t->fields[i].ty);
            c = t->is_union ? std::max(c, f) : c + f;
        }
        return c;
    }
    return 1;
}

LV indexConst(const LV &arr, long long i)
{
    return indexOf(arr, cInt(i));
}

/* Element of a (possibly nested) array at a flattened scalar position. */
LV flatIndex(const LV &lv, Type *ty, long long pos)
{
    if (ty->kind == T_ARRAY && ty->base->kind == T_ARRAY) {
        long long sub = scalarCount(ty->base);
        long long i = sub > 0 ? pos / sub : 0;
        return flatIndex(indexConst(lv, i), ty->base, pos - i * sub);
    }
    return indexConst(lv, pos);
}

void zeroInit(const LV &lv)
{
    Type *t = lv.ty;
    if (t->kind == T_ARRAY) {
        for (long long i = 0; i < t->len; i++) zeroInit(indexConst(lv, i));
    } else if (t->kind == T_STRUCT) {
        for (size_t i = 0; i < t->fields.size(); i++) {
            zeroInit(memberOf(lv, t->fields[i]));
            if (t->is_union) break;
        }
    } else {
        storeLV(lv, isFloat(t) ? cFlt(0.0, t) : cInt(0, t));
    }
}

void bulkZero(const LV &lv)
{
    Value p = addrOf(lv);
    emit("param " + p.s);
    emit("param 0");
    emit("param " + std::to_string(sizeOf(lv.ty)));
    emit("call memset, 3");
}

/* Initialises an array or struct from a brace-enclosed initialiser list. */
void initAgg(const LV &lv, Type *ty, Node *init, int line)
{
    if (init->kind != N_INITLIST) {
        if (ty->kind == T_ARRAY && ty->base->kind == T_CHAR && init->kind == N_STR) {
            const string &s = init->s;
            if ((long long)s.size() > ty->len) err(line, "initializer string is too long for the array");
            for (long long i = 0; i < ty->len; i++) {
                char c = i < (long long)s.size() ? s[i] : 0;
                storeLV(indexConst(lv, i), cChar((signed char)c));
            }
            return;
        }
        if (ty->kind == T_ARRAY) { err(line, "array must be initialised with a brace-enclosed list"); return; }
        Value v = genExpr(init);
        if (v.ty->kind == T_VOID) { err(line, "cannot initialise from a void value"); return; }
        if (ty->kind == T_STRUCT && !sameType(ty, v.ty)) { err(line, "incompatible types in struct initialisation"); return; }
        if (ptrConvError(ty, v, line)) return;
        v = convertTo(v, ty);
        storeLV(lv, v);
        return;
    }
    if (ty->kind == T_ARRAY) {
        if (ty->base->kind == T_ARRAY) {
            bool flat = false;                          /* flat init of a multi-dim array */
            for (size_t i = 0; i < init->kids.size(); i++)
                if (init->kids[i]->kind != N_INITLIST) { flat = true; break; }
            if (flat) {
                long long total = scalarCount(ty);
                Type *st = ty;
                while (st->kind == T_ARRAY) st = st->base;
                long long n = (long long)init->kids.size();
                if (n > total) { err(line, "too many initializers for the array"); n = total; }
                for (long long i = 0; i < n; i++)
                    initAgg(flatIndex(lv, ty, i), st, init->kids[i], line);
                for (long long i = n; i < total; i++) zeroInit(flatIndex(lv, ty, i));
                return;
            }
        }
        bool bulk = scalarCount(ty) > 32 && (long long)init->kids.size() < ty->len;
        if (bulk) bulkZero(lv);
        for (size_t i = 0; i < init->kids.size(); i++) {
            if ((long long)i >= ty->len) { err(line, "too many initializers for the array"); break; }
            LV e = indexConst(lv, (long long)i);
            initAgg(e, ty->base, init->kids[i], line);
        }
        if (!bulk)
            for (long long i = (long long)init->kids.size(); i < ty->len; i++) zeroInit(indexConst(lv, i));
    } else if (ty->kind == T_STRUCT) {
        size_t nf = ty->is_union ? std::min<size_t>(1, ty->fields.size()) : ty->fields.size();
        if (init->kids.size() > nf) err(line, "too many initializers for '" + typeStr(ty) + "'");
        for (size_t i = 0; i < nf; i++) {
            LV f = memberOf(lv, ty->fields[i]);
            if (i < init->kids.size()) initAgg(f, ty->fields[i].ty, init->kids[i], line);
            else zeroInit(f);
        }
    } else {
        if (init->kids.size() > 1) err(line, "too many initializers for a scalar");
        if (init->kids.empty()) zeroInit(lv);
        else initAgg(lv, ty, init->kids[0], line);
    }
}

/* Constant initialiser text for globals and static variables. */
bool constInit(Node *init, Type *ty, string &out)
{
    if (init->kind == N_INITLIST) {
        string r = "{";
        for (size_t i = 0; i < init->kids.size(); i++) {
            Type *et = ty;
            if (ty->kind == T_ARRAY) et = ty->base;
            else if (ty->kind == T_STRUCT && i < ty->fields.size()) et = ty->fields[i].ty;
            string part;
            if (!constInit(init->kids[i], et, part)) return false;
            r += (i ? ", " : "") + part;
        }
        out = r + "}";
        return true;
    }
    if (init->kind == N_STR) { out = quoteStr(init->s); return true; }
    CVal c = constEval(init);
    if (!c.ok) return false;
    if (isFloat(ty)) out = fmtDouble(c.isf ? c.d : (double)c.i);
    else out = std::to_string(c.isf ? (long long)c.d : c.i);
    return true;
}

Type *fixArrayLen(Type *ty, Node *init, int line)
{
    if (ty->kind != T_ARRAY || ty->len >= 0) return ty;
    long long n = -1;
    if (init && init->kind == N_STR && ty->base->kind == T_CHAR) n = (long long)init->s.size() + 1;
    else if (init && init->kind == N_INITLIST) n = (long long)init->kids.size();
    if (n < 0) {
        err(line, "array size is missing and cannot be deduced from an initializer");
        n = 1;
    }
    return mkArray(ty->base, n);
}

/* ------------------------------------------------------------
 * Declarations
 * ------------------------------------------------------------ */
void genStaticLocal(Node *d, Type *ty, Node *init)
{
    string ir = "static_" + (curFunc ? curFunc->name : string("global")) + "_" + d->s +
                (staticCounter ? "_" + std::to_string(staticCounter) : "");
    staticCounter++;
    if (scopes.back().count(d->s)) err(d->line, "redeclaration of '" + d->s + "' in the same scope");
    VarInfo v;
    v.ty = ty; v.ir = ir; v.isConst = d->is_const;
    scopes.back()[d->s] = v;
    string line = "global " + declStr(ty, ir);
    if (init) {
        string c;
        if (!constInit(init, ty, c)) err(d->line, "initializer of static variable '" + d->s + "' is not a constant expression");
        else line += " = " + c;
    }
    dataLines.push_back(line + "    ; static, local to " + (curFunc ? curFunc->name : string("file")));
}

FuncSig *findDtor(Type *t)
{
    for (; t && t->kind == T_STRUCT; t = t->parent) {
        map<string, vector<FuncSig *> >::iterator it = funcs.find(t->tag + "::~" + t->tag);
        if (it != funcs.end() && !it->second.empty()) return it->second[0];
    }
    return nullptr;
}

void emitDtors(size_t idx)
{
    if (idx >= dtorScopes.size()) return;
    vector<std::pair<string, Type *> > &v = dtorScopes[idx];
    for (int i = (int)v.size() - 1; i >= 0; i--) {
        FuncSig *d = findDtor(v[i].second);
        if (!d) continue;
        string t = newTemp();
        emit(t + " = &" + v[i].first);
        emit("param " + t);
        emit("call " + d->ir + ", 1");
    }
}

/* Generates a local declaration: storage, static locals, initialisers and constructor calls. */
void genDecl(Node *d)
{
    if (d->kind != N_DECL) return;
    Node *init = d->kids.empty() ? nullptr : d->kids[0];
    Type *ty = d->ty;

    if (d->is_auto) {                                       /* type deduced from the initialiser */
        if (!init) { err(d->line, "declaration of 'auto " + d->s + "' requires an initializer"); return; }
        if (init->kind == N_INITLIST) { err(d->line, "cannot deduce 'auto' from a brace list"); return; }
        Type *deduced = (init->kind == N_LAMBDA) ? typeOf(init) : typeOfNoDecay(init);
        if (deduced->kind == T_ARRAY) deduced = mkPtr(deduced->base);
        ty = deduced;
    }
    if (ty->kind == T_VOID) { err(d->line, "variable '" + d->s + "' declared void"); return; }
    ty = fixArrayLen(ty, init, d->line);
    if (ty->kind == T_STRUCT && !ty->complete) { err(d->line, "variable '" + d->s + "' has incomplete type '" + typeStr(ty) + "'"); return; }
    if (d->is_static) { genStaticLocal(d, ty, init); return; }
    if (d->flag) return;                                   /* extern declaration */
    if (d->is_ref && !init) { err(d->line, "reference '" + d->s + "' must be initialised"); return; }

    /* int x(5);  -> plain initialisation of a scalar */
    if (d->ctor_init && ty->kind != T_STRUCT) {
        if (d->extra.size() != 1) { err(d->line, "initializer of '" + d->s + "' needs exactly one value"); return; }
        init = d->extra[0];
    }

    string ir = declareVar(d->line, d->s, ty, d->is_const, d->is_ref);
    emit("local " + declStr(ty, ir, d->is_ref));

    /* class object: run the constructor */
    if (ty->kind == T_STRUCT && !d->is_ref) {
        map<string, vector<FuncSig *> >::iterator ci = funcs.find(ty->tag + "::" + ty->tag);
        if (ci != funcs.end() && !init) {
            vector<Type *> at;
            for (size_t i = 0; i < d->extra.size(); i++) at.push_back(typeOf(d->extra[i]));
            FuncSig *f = resolve(ci->second, at, 1);
            if (!f) {
                err(d->line, "no matching constructor for '" + typeStr(ty) + "' with " +
                             std::to_string(d->extra.size()) + " argument(s)");
            } else {
                checkAccess(f->cls, f->access, d->line, ty->tag + "()");
                vector<string> pre;
                string self = newTemp();
                emit(self + " = &" + ir);
                pre.push_back(self);
                emitUserCall(f, d->extra, 1, pre, d->line);
            }
        } else if (ci == funcs.end() && d->ctor_init) {
            err(d->line, "'" + typeStr(ty) + "' has no constructor taking arguments");
        }
        if (findDtor(ty)) dtorScopes.back().push_back(std::make_pair(ir, ty));
    }
    if (!init) return;
    if (d->is_ref) {
        LV src;
        if (genLV(init, src)) {
            if (!sameType(src.ty, ty) && !(isArith(src.ty) && isArith(ty)))
                err(d->line, "reference type does not match the initialiser");
            emit(ir + " = " + addrOf(src).s);
        }
        return;
    }
    LV lv;
    lv.k = LV_VAR; lv.base = ir; lv.ty = ty;
    if (init->kind == N_LAMBDA) {                          /* remember the closure for later calls */
        Value lamv = genExpr(init);
        if (ty->kind != T_PTR) { err(d->line, "a lambda can only initialise 'auto' or a function pointer"); return; }
        storeLV(lv, lamv);
        VarInfo &vi = scopes.back()[d->s];
        vi.lam = lamv.lam;
        vi.capIr = lamv.capIr;
        return;
    }
    initAgg(lv, ty, init, d->line);
}

/* Generates a global declaration and its constant initialiser into the data section. */
void genGlobalDecl(Node *d)
{
    Node *init = d->kids.empty() ? nullptr : d->kids[0];
    Type *ty = d->ty;
    if (d->is_auto) { err(d->line, "'auto' is only supported for local variables"); return; }
    if (d->ctor_init) {
        if (ty->kind == T_STRUCT) { err(d->line, "global objects with constructor arguments are not supported"); return; }
        if (d->extra.size() == 1) init = d->extra[0];
    }
    if (ty->kind == T_VOID) { err(d->line, "variable '" + d->s + "' declared void"); return; }
    ty = fixArrayLen(ty, init, d->line);
    if (ty->kind == T_STRUCT && !ty->complete) { err(d->line, "variable '" + d->s + "' has incomplete type"); return; }
    map<string, VarInfo>::iterator ex = scopes[0].find(d->s);
    if (ex != scopes[0].end()) {
        if (d->flag || !init) return;                      /* repeated declaration */
        if (definedGlobals.count(d->s)) err(d->line, "redefinition of global '" + d->s + "'");
        if (!ex->second.ir.empty() && !sameType(ex->second.ty, ty)) err(d->line, "conflicting types for '" + d->s + "'");
    }
    if (d->flag && !init) {                                /* extern */
        VarInfo v; v.ty = ty; v.ir = d->s; v.isConst = d->is_const;
        scopes[0][d->s] = v;
        globalNames[d->s] = 1;
        return;
    }
    VarInfo v;
    v.ty = ty; v.ir = d->s; v.isConst = d->is_const; v.isRef = false;
    scopes[0][d->s] = v;
    globalNames[d->s] = 1;
    if (init) definedGlobals.insert(d->s);
    if (d->is_ref) { err(d->line, "global references are not supported"); return; }
    string line = "global " + declStr(ty, d->s);
    if (init) {
        string c;
        if (!constInit(init, ty, c)) {
            err(d->line, "initializer of global '" + d->s + "' is not a constant expression");
            return;
        }
        line += " = " + c;
    }
    if (d->is_static) line += "    ; static (internal linkage)";
    dataLines.push_back(line);
}

/* ------------------------------------------------------------
 * Statements
 * ------------------------------------------------------------ */
void collectCases(Node *s, vector<std::pair<Node *, string> > &cases, string &defLabel)
{
    if (!s) return;
    switch (s->kind) {
    case N_BLOCK:
        for (size_t i = 0; i < s->kids.size(); i++) collectCases(s->kids[i], cases, defLabel);
        break;
    case N_CASE:
        s->s = newLabel();
        cases.push_back(std::make_pair(s->kids[0], s->s));
        collectCases(s->kids[1], cases, defLabel);
        break;
    case N_DEFAULT:
        s->s = newLabel();
        defLabel = s->s;
        collectCases(s->kids[0], cases, defLabel);
        break;
    case N_LABEL:
        collectCases(s->kids[0], cases, defLabel);
        break;
    default:
        break;
    }
}

void genExprStmt(Node *e)
{
    if (e->kind == N_UN && (e->s == "p++" || e->s == "p--")) {
        Node tmp = *e;
        tmp.s = (e->s == "p++") ? "++" : "--";
        genExpr(&tmp);
        return;
    }
    genExpr(e);
}

/* Generates statements: blocks, selection, loops, jumps, return, delete and scope-exit destructors. */
void genStmt(Node *n)
{
    if (!n) return;
    switch (n->kind) {
    case N_EMPTY: break;
    case N_FUNC:  break;
    case N_BLOCK:
        pushScope();
        for (size_t i = 0; i < n->kids.size(); i++) genStmt(n->kids[i]);
        if (!(cur.size() && cur.back().compare(0, 6, "return") == 0)) emitDtors(scopes.size() - 1);
        popScope();
        break;
    case N_DECLLIST:
        for (size_t i = 0; i < n->kids.size(); i++) {
            if (n->kids[i]->kind == N_FUNC) continue;
            genDecl(n->kids[i]);
        }
        break;
    case N_EXPRSTMT:
        genExprStmt(n->kids[0]);
        break;
    case N_IF: {
        string Lelse = newLabel();
        genCond(n->kids[0], Lelse, false);
        genStmt(n->kids[1]);
        if (n->kids[2]) {
            string Lend = newLabel();
            emit("goto " + Lend);
            emitLabel(Lelse);
            genStmt(n->kids[2]);
            emitLabel(Lend);
        } else {
            emitLabel(Lelse);
        }
        break;
    }
    case N_WHILE:
    case N_UNTIL: {
        string Ls = newLabel(), Le = newLabel();
        emitLabel(Ls);
        genCond(n->kids[0], Le, n->kind == N_UNTIL);
        LoopCtx c; c.brk = Le; c.cont = Ls;
        loops.push_back(c);
        genStmt(n->kids[1]);
        loops.pop_back();
        emit("goto " + Ls);
        emitLabel(Le);
        break;
    }
    case N_DOWHILE:
    case N_DOUNTIL: {
        string Ls = newLabel(), Lc = newLabel(), Le = newLabel();
        emitLabel(Ls);
        LoopCtx c; c.brk = Le; c.cont = Lc;
        loops.push_back(c);
        genStmt(n->kids[0]);
        loops.pop_back();
        emitLabel(Lc);
        genCond(n->kids[1], Ls, n->kind == N_DOWHILE);
        emitLabel(Le);
        break;
    }
    case N_FOR: {
        pushScope();
        genStmt(n->kids[0]);
        string Lcond = newLabel(), Lstep = newLabel(), Le = newLabel();
        emitLabel(Lcond);
        if (n->kids[1]) genCond(n->kids[1], Le, false);
        LoopCtx c; c.brk = Le; c.cont = Lstep;
        loops.push_back(c);
        genStmt(n->kids[3]);
        loops.pop_back();
        emitLabel(Lstep);
        if (n->kids[2]) genExprStmt(n->kids[2]);
        emit("goto " + Lcond);
        emitLabel(Le);
        emitDtors(scopes.size() - 1);
        popScope();
        break;
    }
    case N_SWITCH: {
        Value v = genExpr(n->kids[0]);
        if (!isInteger(v.ty)) { err(n->line, "switch quantity is not an integer"); v = cInt(0); }
        string Le = newLabel();
        vector<std::pair<Node *, string> > cases;
        string defLabel;
        collectCases(n->kids[1], cases, defLabel);
        set<long long> seen;
        for (size_t i = 0; i < cases.size(); i++) {
            CVal cv = constEval(cases[i].first);
            if (!cv.ok || cv.isf) { err(cases[i].first->line, "case label is not an integer constant"); continue; }
            if (!seen.insert(cv.i).second) err(cases[i].first->line, "duplicate case value " + std::to_string(cv.i));
            emit("if " + v.s + " == " + std::to_string(cv.i) + " goto " + cases[i].second);
        }
        emit("goto " + (defLabel.empty() ? Le : defLabel));
        LoopCtx c;
        c.brk = Le;
        c.cont = loops.empty() ? string() : loops.back().cont;
        loops.push_back(c);
        genStmt(n->kids[1]);
        loops.pop_back();
        emitLabel(Le);
        break;
    }
    case N_CASE:
        if (n->s.empty()) err(n->line, "'case' label not within a switch statement");
        else emitLabel(n->s);
        genStmt(n->kids[1]);
        break;
    case N_DEFAULT:
        if (n->s.empty()) err(n->line, "'default' label not within a switch statement");
        else emitLabel(n->s);
        genStmt(n->kids[0]);
        break;
    case N_BREAK:
        if (loops.empty()) err(n->line, "'break' statement not within a loop or switch");
        else emit("goto " + loops.back().brk);
        break;
    case N_CONTINUE:
        if (loops.empty() || loops.back().cont.empty()) err(n->line, "'continue' statement not within a loop");
        else emit("goto " + loops.back().cont);
        break;
    case N_RETURN: {
        Type *rt = curFunc ? curFunc->ret : tVoid();
        if (n->kids[0]) {
            if (curFunc && curFunc->retRef) {                /* reference return: hand back an address */
                if (rt->kind == T_VOID) {
                    err(n->line, "void function '" + (curFunc ? curFunc->name : string("")) + "' should not return a value");
                    emit("return 0");
                    break;
                }
                LV lv;
                if (!genLV(n->kids[0], lv)) {
                    err(n->line, "cannot return a reference to a non-lvalue");
                    emit("return 0");
                    break;
                }
                for (size_t k = scopes.size() - 1; k >= 1; k--) emitDtors(k);
                emit("return " + addrOf(lv).s);
                break;
            }
            Value v = genExpr(n->kids[0]);
            if (rt->kind == T_VOID) {
                err(n->line, "void function '" + (curFunc ? curFunc->name : string("")) + "' should not return a value");
                emit("return");
            } else {
                if (v.ty->kind == T_VOID) { err(n->line, "cannot return a void value"); v = cInt(0); }
                v = convertTo(v, rt);
                for (size_t k = scopes.size() - 1; k >= 1; k--) emitDtors(k);
                emit("return " + v.s);
            }
        } else {
            if (rt->kind != T_VOID)
                err(n->line, "non-void function '" + (curFunc ? curFunc->name : string("")) + "' should return a value");
            for (size_t k = scopes.size() - 1; k >= 1; k--) emitDtors(k);
            emit("return");
        }
        break;
    }
    case N_GOTO:
        labelsUsed.push_back(std::make_pair(n->s, n->line));
        emit("goto L_" + n->s);
        break;
    case N_LABEL:
        if (!labelsDefined.insert(n->s).second) err(n->line, "duplicate label '" + n->s + "'");
        emitLabel("L_" + n->s);
        genStmt(n->kids[0]);
        break;
    case N_DELETE: {
        Value p = genExpr(n->kids[0]);
        if (!isPointer(p.ty)) { err(n->line, "'delete' requires a pointer operand"); break; }
        FuncSig *dt = n->flag ? nullptr : findDtor(p.ty->base);
        if (dt) {
            emit("param " + p.s);
            emit("call " + dt->ir + ", 1");
        }
        emit("free " + p.s);
        break;
    }
    default:
        err(n->line, "unsupported statement");
    }
}

/* ------------------------------------------------------------
 * Post-processing of generated code
 * ------------------------------------------------------------ */
bool isIdentCh(char c) { return isalnum((unsigned char)c) || c == '_'; }

void tokenCounts(const vector<string> &v, map<string, int> &cnt)
{
    for (size_t k = 0; k < v.size(); k++) {
        const string &l = v[k];
        size_t i = 0;
        while (i < l.size()) {
            if (isalpha((unsigned char)l[i]) || l[i] == '_') {
                size_t j = i;
                while (j < l.size() && isIdentCh(l[j])) j++;
                cnt[l.substr(i, j - i)]++;
                i = j;
            } else {
                i++;
            }
        }
    }
}

bool isTempName(const string &s)
{
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); i++) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

bool isSimpleName(const string &s)
{
    if (s.empty() || isdigit((unsigned char)s[0])) return false;
    for (size_t i = 0; i < s.size(); i++) if (!isIdentCh(s[i])) return false;
    return true;
}

bool isGenLabel(const string &l)
{
    if (l.size() < 3 || l[0] != 'L' || l[l.size() - 1] != ':') return false;
    for (size_t i = 1; i + 1 < l.size(); i++) if (!isdigit((unsigned char)l[i])) return false;
    return true;
}

/* Cleans the generated lines of one function (jump-to-next removal and unused labels). */
void optimise(vector<string> &v)
{
    bool changed = true;
    while (changed) {
        changed = false;

        /* copy propagation: "t = expr" immediately followed by "x = t" */
        map<string, int> counts;
        tokenCounts(v, counts);
        for (size_t i = 0; i + 1 < v.size(); i++) {
            size_t sp = v[i].find(" = ");
            if (sp == string::npos) continue;
            string lhs = v[i].substr(0, sp);
            if (!isTempName(lhs)) continue;
            size_t sp2 = v[i + 1].find(" = ");
            if (sp2 == string::npos) continue;
            string dst = v[i + 1].substr(0, sp2);
            string rhs2 = v[i + 1].substr(sp2 + 3);
            if (rhs2 != lhs || !isSimpleName(dst) || counts[lhs] != 2) continue;
            v[i] = dst + " = " + v[i].substr(sp + 3);
            v.erase(v.begin() + i + 1);
            changed = true;
            break;
        }
        if (changed) continue;

        /* jump to the immediately following label */
        for (size_t i = 0; i + 1 < v.size(); i++) {
            if (v[i].compare(0, 5, "goto ") == 0 && v[i + 1] == v[i].substr(5) + ":") {
                v.erase(v.begin() + i);
                changed = true;
                break;
            }
        }
        if (changed) continue;

        /* unreferenced generated labels */
        set<string> targets;
        for (size_t i = 0; i < v.size(); i++) {
            size_t g = v[i].rfind("goto ");
            if (g != string::npos) targets.insert(v[i].substr(g + 5));
        }
        for (size_t i = 0; i < v.size(); i++) {
            if (isGenLabel(v[i]) && !targets.count(v[i].substr(0, v[i].size() - 1))) {
                v.erase(v.begin() + i);
                changed = true;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------
 * Functions
 * ------------------------------------------------------------ */
/* Emits the complete IR for one function: parameters, base ctor/dtor hooks, body, labels and optimisation. */
void genFunction(FuncSig *f)
{
    cur.clear();
    tc = 0;
    nameCount = globalNames;
    loops.clear();
    labelsDefined.clear();
    labelsUsed.clear();
    curFunc = f;

    pushScope();
    vector<string> ptext;
    for (size_t i = 0; i < f->params.size(); i++) {
        const Param &p = f->params[i];
        string nm = p.name.empty() ? "_arg" + std::to_string(i) : p.name;
        string ir = declareVar(f->line, nm, p.ty, false, p.ref);
        ptext.push_back(declStr(p.ty, ir, p.ref));
    }
    string header = "func " + f->ir + "(";
    for (size_t i = 0; i < ptext.size(); i++) header += (i ? ", " : "") + ptext[i];
    if (f->variadic) header += ptext.empty() ? "..." : ", ...";
    header += ") : " + typeStr(f->ret);

    /* a derived-class constructor runs the base default constructor first,
       unless the initialiser list names the base explicitly */
    if (f->isCtor && f->cls && f->cls->parent && !f->hasBInit) {
        Type *par = f->cls->parent;
        map<string, vector<FuncSig *> >::iterator bi = funcs.find(par->tag + "::" + par->tag);
        if (bi != funcs.end()) {
            vector<Type *> none;
            FuncSig *bc = resolve(bi->second, none, 1);
            if (bc) {
                vector<string> pre;
                pre.push_back(lookupVar("this")->ir);
                vector<Node *> noargs;
                emitUserCall(bc, noargs, 1, pre, f->line);
            } else {
                err(f->line, "base class '" + par->tag + "' has no default constructor; initialise it explicitly");
            }
        }
    }

    genStmt(f->body);

    if (f->cls && f->cls->parent && f->name == f->cls->tag + "::~" + f->cls->tag) {
        FuncSig *pd = findDtor(f->cls->parent);
        if (pd) {
            emit("param " + lookupVar("this")->ir);
            emit("call " + pd->ir + ", 1");
        }
    }

    bool endsWithReturn = !cur.empty() && cur.back().compare(0, 6, "return") == 0;
    if (!endsWithReturn)
        emit((f->name == "main" && f->ret->kind != T_VOID) || f->retRef ? "return 0" : "return");
    popScope();

    for (size_t i = 0; i < labelsUsed.size(); i++)
        if (!labelsDefined.count(labelsUsed[i].first))
            err(labelsUsed[i].second, "use of undeclared label '" + labelsUsed[i].first + "'");

    optimise(cur);

    textOut.push_back(header);
    for (size_t i = 0; i < cur.size(); i++) {
        const string &l = cur[i];
        if (!l.empty() && l[l.size() - 1] == ':') {
            textOut.push_back(l);
        } else {
            textOut.push_back("    " + l);
            if (l.compare(0, 6, "local ") != 0) instrCount++;
        }
    }
    textOut.push_back("end " + f->ir);
    textOut.push_back("");
    curFunc = nullptr;
}

/* First pass over the program: records every function signature before bodies are generated. */
void collectFunctions()
{
    for (size_t i = 0; i < g_program.size(); i++) {
        Node *n = g_program[i];
        if (n->kind != N_FUNC) continue;
        vector<FuncSig *> &v = funcs[n->s];
        Node *body = n->kids.empty() ? nullptr : n->kids[0];
        FuncSig *match = nullptr;
        for (size_t k = 0; k < v.size(); k++)
            if (sameSig(v[k]->params, n->params)) { match = v[k]; break; }
        if (match) {
            if (match->body && body) err(n->line, "redefinition of function '" + n->s + "'");
            if (!sameType(match->ret, n->ty)) err(n->line, "conflicting return type for '" + n->s + "'");
            if (match->retRef != n->is_ref) err(n->line, "conflicting reference return type for '" + n->s + "'");
            if (body) {
                match->body = body; match->node = n; match->line = n->line;
                match->hasBInit = n->has_binit;
                match->retRef = n->is_ref;
            }
        } else {
            FuncSig *f = new FuncSig();
            f->name = n->s; f->ret = n->ty; f->params = n->params;
            f->variadic = n->variadic; f->body = body; f->node = n; f->line = n->line;
            f->cls = n->cls; f->access = n->access;
            f->isCtor = n->is_ctor; f->isDtor = n->is_dtor; f->hasBInit = n->has_binit;
            f->retRef = n->is_ref;
            v.push_back(f);
        }
    }
    for (map<string, vector<FuncSig *> >::iterator it = funcs.begin(); it != funcs.end(); ++it) {
        bool over = it->second.size() > 1 && it->first != "main";
        for (size_t k = 0; k < it->second.size(); k++) {
            FuncSig *f = it->second[k];
            f->ir = over ? it->first + "_" + sigTag(f->params, f->cls ? 1 : 0) : it->first;
        }
    }
}

string typeSection()
{
    string out;
    for (size_t i = 0; i < g_struct_list.size(); i++) {
        Type *t = g_struct_list[i];
        if (!t->complete || t->tag == "FILE") continue;
        out += "    " + string(t->is_union ? "union " : (t->is_class ? "class " : "struct ")) + t->tag + " : size " +
               std::to_string(t->size) + (t->parent ? " extends " + t->parent->tag : string()) + " { ";
        for (size_t k = 0; k < t->fields.size(); k++) {
            out += declStr(t->fields[k].ty, t->fields[k].name) + " @" + std::to_string(t->fields[k].offset);
            out += (k + 1 < t->fields.size()) ? "; " : ";";
        }
        out += " }\n";
    }
    return out;
}

} /* namespace */

/* ------------------------------------------------------------
 * Entry point
 * ------------------------------------------------------------ */
string g_ir_text;

/* Drives code generation for the whole program and returns the number of semantic errors. */
int generate_ir(const string &infile, const string &outfile, bool printIR)
{
    int errorsBefore = g_sem_errors;
    initLib();
    pushScope();                                           /* global scope */
    {
        Type *fp = mkPtr(g_typedefs["FILE"]);
        const char *std_files[] = {"stdin", "stdout", "stderr"};
        for (int i = 0; i < 3; i++) {
            VarInfo v; v.ty = fp; v.ir = std_files[i];
            scopes[0][std_files[i]] = v;
            globalNames[std_files[i]] = 1;
        }
    }

    collectFunctions();
    for (size_t i = 0; i < g_program.size(); i++)
        if (g_program[i]->kind == N_DECL) genGlobalDecl(g_program[i]);

    for (size_t i = 0; i < g_program.size(); i++) {
        Node *n = g_program[i];
        if (n->kind != N_FUNC || n->kids.empty() || !n->kids[0]) continue;
        vector<FuncSig *> &v = funcs[n->s];
        for (size_t k = 0; k < v.size(); k++)
            if (v[k]->node == n && v[k]->body == n->kids[0]) genFunction(v[k]);
    }

    for (size_t q = 0; q < lambdaQ.size(); q++) genFunction(lambdaQ[q]);   /* lambdas may queue more */

    if (g_sem_errors > errorsBefore) return g_sem_errors - errorsBefore;

    string out;
    out += "; Intermediate code (three-address form)\n";
    out += "; Source file: " + infile + "\n\n";
    string types = typeSection();
    if (!types.empty()) out += ".types\n" + types + "\n";
    if (!dataLines.empty()) {
        out += ".data\n";
        for (size_t i = 0; i < dataLines.size(); i++) out += "    " + dataLines[i] + "\n";
        out += "\n";
    }
    out += ".text\n";
    for (size_t i = 0; i < textOut.size(); i++) out += textOut[i] + "\n";

    g_ir_text = out;
    if (!outfile.empty()) {
        FILE *f = fopen(outfile.c_str(), "w");
        if (!f) {
            fprintf(stderr, "Error: cannot write '%s'\n", outfile.c_str());
            return 1;
        }
        fputs(out.c_str(), f);
        fclose(f);
        printf(">> Intermediate code written to %s (%d instruction(s), %d function(s)).\n",
               outfile.c_str(), instrCount, (int)(std::count(textOut.begin(), textOut.end(), string(""))));
    }
    if (printIR) fputs(out.c_str(), stdout);
    return 0;
}
