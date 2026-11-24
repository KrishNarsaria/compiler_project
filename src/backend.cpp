/* ============================================================
 * backend.cpp - RISC-V (RV64IMFD) code generator for the IR
 * Target simulator: RARS 1.6 (java -jar rars.jar rv64 prog.s)
 * ============================================================ */
#include "backend.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace {

/* ---------------- Types ---------------- */

enum K { VOID, BOOL, CHAR, SHORT, INT, LONG, FLOAT, DOUBLE, PTR, ARRAY, STRUCT, FUNC };

struct Ty {
    K k = INT;
    bool uns = false;
    Ty *base = nullptr;
    long long len = 0;
    string tag;
    vector<Ty *> params;
    bool variadic = false;
};

struct Fld { string name; Ty *ty; long long off; };
struct SInfo { long long size = 0; bool isUnion = false; vector<Fld> fields; };

map<string, SInfo> g_structs;

Ty *mk(K k, Ty *base = nullptr)
{
    Ty *t = new Ty();
    t->k = k;
    t->base = base;
    return t;
}

Ty *T_INT()    { static Ty *t = mk(INT);    return t; }
Ty *T_LONG()   { static Ty *t = mk(LONG);   return t; }
Ty *T_CHAR()   { static Ty *t = mk(CHAR);   return t; }
Ty *T_DOUBLE() { static Ty *t = mk(DOUBLE); return t; }
Ty *T_VOID()   { static Ty *t = mk(VOID);   return t; }

bool isFlt(Ty *t) { return t && (t->k == FLOAT || t->k == DOUBLE); }
bool isPtrLike(Ty *t) { return t && (t->k == PTR || t->k == ARRAY || t->k == FUNC); }

string trim(const string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool startsWith(const string &s, const string &p) { return s.compare(0, p.size(), p) == 0; }

long long sizeOf(Ty *t);

long long alignOf(Ty *t)
{
    if (!t) return 4;
    if (t->k == ARRAY) return alignOf(t->base);
    if (t->k == STRUCT) {
        long long a = 1;
        map<string, SInfo>::iterator it = g_structs.find(t->tag);
        if (it == g_structs.end()) return 8;
        for (size_t i = 0; i < it->second.fields.size(); i++) {
            long long fa = alignOf(it->second.fields[i].ty);
            if (fa > a) a = fa;
        }
        return a;
    }
    long long s = sizeOf(t);
    return s < 1 ? 1 : s;
}

long long sizeOf(Ty *t)
{
    if (!t) return 4;
    switch (t->k) {
    case VOID: case BOOL: case CHAR: return 1;
    case SHORT: return 2;
    case INT: case FLOAT: return 4;
    case LONG: case DOUBLE: case PTR: return 8;
    case ARRAY: return t->len < 0 ? 0 : t->len * sizeOf(t->base);
    case STRUCT: {
        map<string, SInfo>::iterator it = g_structs.find(t->tag);
        return it == g_structs.end() ? 8 : it->second.size;
    }
    case FUNC: return 1;
    }
    return 4;
}

/* Scans from the right to find the '(' that matches the final ')'. */
size_t matchOpen(const string &s)
{
    int depth = 0;
    for (size_t i = s.size(); i-- > 0;) {
        if (s[i] == ')') depth++;
        else if (s[i] == '(') { if (--depth == 0) return i; }
    }
    return string::npos;
}

vector<string> splitTopLevel(const string &s, char sep)
{
    vector<string> out;
    int depth = 0;
    bool inStr = false, inChr = false;
    string cur;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (inStr) { cur += c; if (c == '\\' && i + 1 < s.size()) cur += s[++i]; else if (c == '"') inStr = false; continue; }
        if (inChr) { cur += c; if (c == '\\' && i + 1 < s.size()) cur += s[++i]; else if (c == '\'') inChr = false; continue; }
        if (c == '"') { inStr = true; cur += c; continue; }
        if (c == '\'') { inChr = true; cur += c; continue; }
        if (c == '(' || c == '{' || c == '[') depth++;
        if (c == ')' || c == '}' || c == ']') depth--;
        if (c == sep && depth == 0) { out.push_back(trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    if (!trim(cur).empty() || !out.empty()) out.push_back(trim(cur));
    return out;
}

/* Parses an IR type string (pointers, arrays, function types, struct tags) into a Ty. */
Ty *parseType(const string &str)
{
    string s = trim(str);
    if (s.empty()) return T_INT();
    if (s[s.size() - 1] == '*') return mk(PTR, parseType(s.substr(0, s.size() - 1)));
    if (s[s.size() - 1] == '&') return mk(PTR, parseType(s.substr(0, s.size() - 1)));
    if (s[s.size() - 1] == ')') {
        size_t o = matchOpen(s);
        if (o != string::npos) {
            Ty *f = mk(FUNC, parseType(s.substr(0, o)));
            string inner = s.substr(o + 1, s.size() - o - 2);
            vector<string> ps = splitTopLevel(inner, ',');
            for (size_t i = 0; i < ps.size(); i++) {
                if (ps[i] == "...") f->variadic = true;
                else if (!ps[i].empty()) f->params.push_back(parseType(ps[i]));
            }
            return f;
        }
    }
    if (s[s.size() - 1] == ']') {
        size_t o = s.rfind('[');
        string n = s.substr(o + 1, s.size() - o - 2);
        Ty *a = mk(ARRAY, parseType(s.substr(0, o)));
        a->len = n.empty() ? -1 : atoll(n.c_str());
        return a;
    }
    bool uns = false;
    if (startsWith(s, "unsigned ")) { uns = true; s = trim(s.substr(9)); }
    if (s == "unsigned") { uns = true; s = "int"; }
    Ty *t = nullptr;
    if (s == "void") t = mk(VOID);
    else if (s == "bool") t = mk(BOOL);
    else if (s == "char") t = mk(CHAR);
    else if (s == "short") t = mk(SHORT);
    else if (s == "int") t = mk(INT);
    else if (s == "long") t = mk(LONG);
    else if (s == "float") t = mk(FLOAT);
    else if (s == "double") t = mk(DOUBLE);
    else if (startsWith(s, "struct ") || startsWith(s, "class ") || startsWith(s, "union ")) {
        t = mk(STRUCT);
        t->tag = trim(s.substr(s.find(' ') + 1));
    } else t = mk(INT);
    t->uns = uns;
    return t;
}

/* Splits "type name[dims]" into a type and a name; sets isRef for "type& name". */
Ty *parseDecl(const string &decl, string &name, bool &isRef)
{
    string s = trim(decl);
    vector<long long> dims;
    while (!s.empty() && s[s.size() - 1] == ']') {
        size_t o = s.rfind('[');
        string n = s.substr(o + 1, s.size() - o - 2);
        dims.insert(dims.begin(), n.empty() ? -1 : atoll(n.c_str()));
        s = s.substr(0, o);
    }
    size_t sp = s.rfind(' ');
    string typ = sp == string::npos ? s : trim(s.substr(0, sp));
    name = sp == string::npos ? "" : trim(s.substr(sp + 1));
    isRef = false;
    if (!typ.empty() && typ[typ.size() - 1] == '&') { isRef = true; typ = trim(typ.substr(0, typ.size() - 1)); }
    Ty *t = parseType(typ);
    for (size_t i = dims.size(); i-- > 0;) {
        Ty *a = mk(ARRAY, t);
        a->len = dims[i];
        t = a;
    }
    return t;
}

Ty *innermost(Ty *t)
{
    while (t && t->k == ARRAY) t = t->base;
    return t;
}

/* ---------------- Literals ---------------- */

bool isIntLit(const string &s)
{
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    if (s.size() > i + 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        for (size_t j = i + 2; j < s.size(); j++) if (!isxdigit((unsigned char)s[j])) return false;
        return true;
    }
    for (size_t j = i; j < s.size(); j++) if (!isdigit((unsigned char)s[j])) return false;
    return true;
}

bool isFloatLit(const string &s)
{
    if (s.empty()) return false;
    char c = s[0];
    if (!(isdigit((unsigned char)c) || c == '-' || c == '.' || c == '+')) return false;
    if (isIntLit(s)) return false;
    char *end = nullptr;
    strtod(s.c_str(), &end);
    return end && *end == 0 && end != s.c_str();
}

bool isIdent(const string &s)
{
    if (s.empty() || !(isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (size_t i = 0; i < s.size(); i++)
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == ':' || s[i] == '~')) return false;
    return true;
}

/* Decodes the escapes of a quoted literal body (without the quotes). */
string decodeEscapes(const string &b)
{
    string out;
    for (size_t i = 0; i < b.size(); i++) {
        if (b[i] != '\\' || i + 1 >= b.size()) { out += b[i]; continue; }
        char c = b[++i];
        switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'a': out += '\a'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'v': out += '\v'; break;
        case '\\': out += '\\'; break;
        case '\'': out += '\''; break;
        case '"': out += '"'; break;
        case 'x': {
            int v = 0;
            while (i + 1 < b.size() && isxdigit((unsigned char)b[i + 1])) {
                char h = b[++i];
                v = v * 16 + (isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10));
            }
            out += (char)v;
            break;
        }
        default:
            if (c >= '0' && c <= '7') {
                int v = c - '0', n = 1;
                while (n < 3 && i + 1 < b.size() && b[i + 1] >= '0' && b[i + 1] <= '7') { v = v * 8 + (b[++i] - '0'); n++; }
                out += (char)v;
            } else out += c;
        }
    }
    return out;
}

/* ---------------- Function table ---------------- */

struct FuncInfo {
    string name;
    vector<string> pnames;
    vector<Ty *> ptypes;
    vector<bool> prefs;
    Ty *ret = nullptr;
    bool variadic = false;
};

string mangle(const string &n)
{
    string o;
    for (size_t i = 0; i < n.size(); i++) {
        char c = n[i];
        if (c == ':') { if (i + 1 < n.size() && n[i + 1] == ':') i++; o += "__"; }
        else if (c == '~') o += "D_";
        else if (isalnum((unsigned char)c) || c == '_') o += c;
        else o += '_';
    }
    return o;
}

struct Var {
    Ty *ty = nullptr;
    long long off = 0;
    bool global = false;
    string label;
};

struct LibSig { char ret; bool variadic; };

map<string, LibSig> &libTable()
{
    static map<string, LibSig> m;
    if (!m.empty()) return m;
    const char *ints[] = {"printf", "scanf", "sprintf", "sscanf", "fprintf", "fscanf", "puts", "putchar", "getchar",
                          "fputs", "fputc", "fgetc", "fclose", "fseek", "feof", "fflush", "remove", "rename", "strcmp",
                          "strncmp", "atoi", "abs", "ungetc", "isxdigit", "ispunct", "rand", "toupper", "tolower",
                          "isdigit", "isalpha", "isspace", "isalnum", "isupper", "islower"};
    const char *longs[] = {"fread", "fwrite", "ftell", "strlen", "atol", "labs", "strtol", "clock", "time"};
    const char *dbls[] = {"atof", "sqrt", "sin", "cos", "tan", "fabs", "floor", "ceil", "log", "exp", "pow", "fmod",
                          "atan", "log10", "round"};
    const char *ptrs[] = {"gets", "fgets", "memset", "memcpy", "memmove", "realloc", "strcpy", "strncpy", "strcat",
                          "strchr", "strstr", "strdup", "strrchr", "strncat", "malloc", "calloc"};
    const char *voids[] = {"rewind", "qsort", "perror", "srand", "exit", "free"};
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) m[ints[i]] = LibSig{'i', false};
    for (size_t i = 0; i < sizeof(longs) / sizeof(longs[0]); i++) m[longs[i]] = LibSig{'l', false};
    for (size_t i = 0; i < sizeof(dbls) / sizeof(dbls[0]); i++) m[dbls[i]] = LibSig{'d', false};
    for (size_t i = 0; i < sizeof(ptrs) / sizeof(ptrs[0]); i++) m[ptrs[i]] = LibSig{'p', false};
    for (size_t i = 0; i < sizeof(voids) / sizeof(voids[0]); i++) m[voids[i]] = LibSig{'v', false};
    m["fopen"] = LibSig{'f', false};
    const char *va[] = {"printf", "scanf", "sprintf", "sscanf", "fprintf", "fscanf"};
    for (size_t i = 0; i < sizeof(va) / sizeof(va[0]); i++) m[va[i]].variadic = true;
    const char *stream[] = {"__print_int", "__print_long", "__print_char", "__print_str", "__print_bool", "__print_ptr",
                            "__print_double", "__print_endl", "__read_int", "__read_long", "__read_char", "__read_str",
                            "__read_double"};
    for (size_t i = 0; i < sizeof(stream) / sizeof(stream[0]); i++) m[stream[i]] = LibSig{'v', false};
    return m;
}

Ty *libRet(char c)
{
    switch (c) {
    case 'i': return T_INT();
    case 'l': return T_LONG();
    case 'd': return T_DOUBLE();
    case 'p': return mk(PTR, T_CHAR());
    case 'f': { Ty *f = mk(STRUCT); f->tag = "FILE"; return mk(PTR, f); }
    default:  return T_VOID();
    }
}

} /* namespace */

namespace {

/* ---------------- Code generator ---------------- */

class Backend {
public:
    bool runtime = false;
    std::ostringstream body, text, data;
    map<string, FuncInfo> funcs;
    map<string, Ty *> globals;
    map<string, string> strLabels;
    map<string, string> fconsts;
    vector<string> strOrder;
    int strCount = 0, fcCount = 0, skipCount = 0, copyCount = 0;

    FuncInfo *cur = nullptr;
    string fnLabel, fnMangled;
    map<string, Var> vars, temps;
    set<string> addrHolders;
    long long frameTop = 0;
    vector<string> pend;

    /* ---------- emit helpers ---------- */
    void I(const string &s) { body << "    " << s << "\n"; }
    void LBL(const string &l) { body << l << ":\n"; }
    static string num(long long v) { return std::to_string(v); }
    string strPrefix() const { return runtime ? "RS_" : "S_"; }
    string fconstPrefix() const { return runtime ? "RF_" : "F_"; }

    string funcLabel(const string &n) const
    {
        if (funcs.count(n)) return "fn_" + mangle(n);
        if (libTable().count(n) || startsWith(n, "__")) {
            if (n == "__ecall" || n == "__fsqrt") return "fn_" + mangle(n);
            return "fn___rt_" + mangle(n);
        }
        return "fn_" + mangle(n);
    }

    string strLabel(const string &decoded)
    {
        map<string, string>::iterator it = strLabels.find(decoded);
        if (it != strLabels.end()) return it->second;
        string l = strPrefix() + num(++strCount);
        strLabels[decoded] = l;
        strOrder.push_back(decoded);
        return l;
    }

    string fconst(const string &lit)
    {
        map<string, string>::iterator it = fconsts.find(lit);
        if (it != fconsts.end()) return it->second;
        string l = fconstPrefix() + num(++fcCount);
        fconsts[lit] = l;
        return l;
    }

    void loadImm(const string &reg, long long v)
    {
        I("li " + reg + ", " + num(v));
    }

    /* Adds a (possibly large) constant to a register. */
    void addImm(const string &dst, const string &src, long long v)
    {
        if (v >= -2000 && v <= 2000) { if (v != 0 || dst != src) I("addi " + dst + ", " + src + ", " + num(v)); return; }
        loadImm("t5", v);
        I("add " + dst + ", " + src + ", t5");
    }

    /* ---------- variables ---------- */
    Var *findVar(const string &n)
    {
        map<string, Var>::iterator it = vars.find(n);
        if (it != vars.end()) return &it->second;
        it = temps.find(n);
        if (it != temps.end()) return &it->second;
        map<string, Ty *>::iterator g = globals.find(n);
        if (g != globals.end()) {
            static Var gv;
            gv = Var();
            gv.ty = g->second;
            gv.global = true;
            gv.label = "G_" + mangle(n);
            return &gv;
        }
        return nullptr;
    }

    long long allocSlot(long long size)
    {
        if (size < 8) size = 8;
        frameTop = (frameTop + size + 7) / 8 * 8;
        return -(16 + frameTop);
    }

    static long long slotSize(Ty *t)
    {
        if (t && t->k == STRUCT) { long long s = (sizeOf(t) + 7) / 8 * 8; return s < 8 ? 8 : s; }
        return 8;
    }

    string memVar(const Var &v, const string &scratch)
    {
        if (v.global) { I("la " + scratch + ", " + v.label); return "0(" + scratch + ")"; }
        if (v.off >= -2000 && v.off <= 2000) return num(v.off) + "(s0)";
        loadImm(scratch, v.off);
        I("add " + scratch + ", s0, " + scratch);
        return "0(" + scratch + ")";
    }

    void addrVar(const Var &v, const string &reg)
    {
        if (v.global) { I("la " + reg + ", " + v.label); return; }
        addImm(reg, "s0", v.off);
    }

    /* ---------- loads, stores, conversions ---------- */
    void ldOp(Ty *t, const string &reg, const string &mem)
    {
        switch (t->k) {
        case BOOL: I("lbu " + reg + ", " + mem); break;
        case CHAR: I(string(t->uns ? "lbu " : "lb ") + reg + ", " + mem); break;
        case SHORT: I(string(t->uns ? "lhu " : "lh ") + reg + ", " + mem); break;
        case INT: I(string(t->uns ? "lwu " : "lw ") + reg + ", " + mem); break;
        case FLOAT: I("flw " + reg + ", " + mem); break;
        case DOUBLE: I("fld " + reg + ", " + mem); break;
        default: I("ld " + reg + ", " + mem); break;
        }
    }

    void stOp(Ty *t, const string &reg, const string &mem)
    {
        switch (t->k) {
        case BOOL: case CHAR: I("sb " + reg + ", " + mem); break;
        case SHORT: I("sh " + reg + ", " + mem); break;
        case INT: I("sw " + reg + ", " + mem); break;
        case FLOAT: I("fsw " + reg + ", " + mem); break;
        case DOUBLE: I("fsd " + reg + ", " + mem); break;
        default: I("sd " + reg + ", " + mem); break;
        }
    }

    void narrow(Ty *to, const string &r)
    {
        switch (to->k) {
        case BOOL: I("snez " + r + ", " + r); break;
        case CHAR:
            if (to->uns) I("andi " + r + ", " + r + ", 255");
            else { I("slli " + r + ", " + r + ", 56"); I("srai " + r + ", " + r + ", 56"); }
            break;
        case SHORT:
            I("slli " + r + ", " + r + ", 48");
            I(string(to->uns ? "srli " : "srai ") + r + ", " + r + ", 48");
            break;
        case INT:
            if (to->uns) { I("slli " + r + ", " + r + ", 32"); I("srli " + r + ", " + r + ", 32"); }
            else I("addiw " + r + ", " + r + ", 0");
            break;
        default: break;
        }
    }

    /* Converts the value held in ir/fr from type 'from' to type 'to' in place. */
    void conv(Ty *from, Ty *to, const string &ir, const string &fr)
    {
        if (!from || !to || to->k == VOID || to->k == STRUCT || from->k == STRUCT) return;
        bool ff = isFlt(from), tf = isFlt(to);
        if (ff && tf) {
            if (from->k == DOUBLE && to->k == FLOAT) I("fcvt.s.d " + fr + ", " + fr);
            else if (from->k == FLOAT && to->k == DOUBLE) I("fcvt.d.s " + fr + ", " + fr);
            return;
        }
        if (!ff && tf) {
            string sfx = to->k == DOUBLE ? "d" : "s";
            bool wide = from->k == LONG || isPtrLike(from);
            string src = wide ? (from->uns ? "lu" : "l") : (from->uns && from->k == INT ? "wu" : "w");
            I("fcvt." + sfx + "." + src + " " + fr + ", " + ir);
            return;
        }
        if (ff && !tf) {
            string sfx = from->k == DOUBLE ? "d" : "s";
            if (to->k == BOOL) {
                I("fcvt." + sfx + ".w ft7, zero");
                I("feq." + sfx + " " + ir + ", " + fr + ", ft7");
                I("xori " + ir + ", " + ir + ", 1");
                return;
            }
            if (to->k == LONG || isPtrLike(to)) I("fcvt.l." + sfx + " " + ir + ", " + fr + ", rtz");
            else I("fcvt.w." + sfx + " " + ir + ", " + fr + ", rtz");
            narrow(to, ir);
            return;
        }
        if (to->k == BOOL || to->k == CHAR || to->k == SHORT || to->k == INT) narrow(to, ir);
    }

    /* ---------- operands ---------- */
    Ty *opType(const string &s)
    {
        if (isIntLit(s)) { long long v = strtoll(s.c_str(), nullptr, 0); return (v >= -2147483648LL && v <= 2147483647LL) ? T_INT() : T_LONG(); }
        if (isFloatLit(s)) return T_DOUBLE();
        if (!s.empty() && s[0] == '"') return mk(PTR, T_CHAR());
        if (!s.empty() && s[0] == '\'') return T_CHAR();
        Var *v = findVar(s);
        if (v) {
            if (v->ty->k == ARRAY) return mk(PTR, v->ty->base);
            return v->ty;
        }
        return T_INT();
    }

    /* Loads an operand into ir (integer/pointer) or fr (float); returns its type.
       Struct operands are represented by their address. */
    Ty *loadOp(const string &s, const string &ir, const string &fr)
    {
        if (isIntLit(s)) {
            long long v = strtoll(s.c_str(), nullptr, 0);
            loadImm(ir, v);
            return (v >= -2147483648LL && v <= 2147483647LL) ? T_INT() : T_LONG();
        }
        if (isFloatLit(s)) {
            string l = fconst(s);
            I("la t5, " + l);
            I("fld " + fr + ", 0(t5)");
            return T_DOUBLE();
        }
        if (!s.empty() && s[0] == '\'') {
            string d = decodeEscapes(s.substr(1, s.size() - 2));
            loadImm(ir, d.empty() ? 0 : (long long)(signed char)d[0]);
            return T_CHAR();
        }
        if (!s.empty() && s[0] == '"') {
            string d = decodeEscapes(s.substr(1, s.size() - 2));
            I("la " + ir + ", " + strLabel(d));
            return mk(PTR, T_CHAR());
        }
        if (s == "null" || s == "nullptr" || s == "NULL" || s == "false") { loadImm(ir, 0); return T_INT(); }
        if (s == "true") { loadImm(ir, 1); return T_INT(); }
        Var *v = findVar(s);
        if (v) {
            Ty *t = v->ty;
            bool holder = !v->global && addrHolders.count(s);
            if (t->k == ARRAY) { addrVar(*v, ir); return mk(PTR, t->base); }
            if (t->k == STRUCT && !holder) { addrVar(*v, ir); return t; }
            string m = memVar(*v, "t5");
            if (t->k == STRUCT) { I("ld " + ir + ", " + m); return t; }
            ldOp(t, isFlt(t) ? fr : ir, m);
            return t;
        }
        if (funcs.count(s) || libTable().count(s) || startsWith(s, "__")) {
            I("la " + ir + ", " + funcLabel(s));
            Ty *f = mk(FUNC, T_INT());
            if (funcs.count(s)) { f->base = funcs[s].ret; f->params = funcs[s].ptypes; }
            return mk(PTR, f);
        }
        loadImm(ir, 0);
        return T_INT();
    }

    /* ---------- lvalues ---------- */
    bool isLvalueText(const string &s)
    {
        if (s.empty() || isIntLit(s) || isFloatLit(s) || s[0] == '"' || s[0] == '\'') return false;
        if (s[0] == '*') return true;
        if (s.find("->") != string::npos) return true;
        if (s[s.size() - 1] == ']' && s.find('[') != string::npos) return true;
        size_t d = s.find('.');
        return d != string::npos && isIdent(s.substr(0, d));
    }

    Ty *elementOf(Ty *t)
    {
        Ty *e = t && (t->k == PTR || t->k == ARRAY) ? t->base : T_CHAR();
        e = innermost(e);
        if (!e || e->k == VOID || e->k == FUNC) return T_CHAR();
        return e;
    }

    const Fld *findField(Ty *st, const string &name)
    {
        map<string, SInfo>::iterator it = g_structs.find(st->tag);
        if (it == g_structs.end()) return nullptr;
        for (size_t i = it->second.fields.size(); i-- > 0;)
            if (it->second.fields[i].name == name) return &it->second.fields[i];
        return nullptr;
    }

    /* Computes the address of an lvalue expression into areg; returns the lvalue type. */
    Ty *lvAddr(const string &s, const string &areg)
    {
        if (s[0] == '*') {
            Ty *pt = loadOp(s.substr(1), areg, "ft3");
            Ty *e = (pt->k == PTR || pt->k == ARRAY) ? pt->base : T_INT();
            e = innermost(e);
            if (!e || e->k == VOID) e = T_CHAR();
            return e;
        }
        size_t arrow = s.find("->");
        size_t dot = s.find('.');
        if (arrow != string::npos || (dot != string::npos && s[s.size() - 1] != ']')) {
            string base, path;
            Ty *st = nullptr;
            if (arrow != string::npos) {
                base = s.substr(0, arrow);
                path = s.substr(arrow + 2);
                Ty *pt = loadOp(base, areg, "ft3");
                st = pt->k == PTR ? pt->base : pt;
            } else {
                base = s.substr(0, dot);
                path = s.substr(dot + 1);
                Var *v = findVar(base);
                if (v && v->ty->k == STRUCT) {
                    st = v->ty;
                    if (!v->global && addrHolders.count(base)) { string m = memVar(*v, "t5"); I("ld " + areg + ", " + m); }
                    else addrVar(*v, areg);
                } else { Ty *pt = loadOp(base, areg, "ft3"); st = pt->k == PTR ? pt->base : pt; }
            }
            long long total = 0;
            Ty *cur = st;
            vector<string> parts;
            size_t p = 0;
            while (true) {
                size_t q = path.find('.', p);
                parts.push_back(path.substr(p, q == string::npos ? string::npos : q - p));
                if (q == string::npos) break;
                p = q + 1;
            }
            for (size_t i = 0; i < parts.size(); i++) {
                const Fld *f = cur ? findField(cur, parts[i]) : nullptr;
                if (!f) { fprintf(stderr, "backend: unknown field '%s'\n", parts[i].c_str()); return T_INT(); }
                total += f->off;
                cur = f->ty;
            }
            if (total) addImm(areg, areg, total);
            return cur;
        }
        size_t br = s.find('[');
        string base = s.substr(0, br);
        string off = s.substr(br + 1, s.size() - br - 2);
        Var *v = findVar(base);
        Ty *elem;
        if (v && v->ty->k == ARRAY) { addrVar(*v, areg); elem = elementOf(v->ty); }
        else { Ty *pt = loadOp(base, areg, "ft3"); elem = elementOf(pt); }
        if (isIntLit(off)) {
            long long o = strtoll(off.c_str(), nullptr, 0);
            if (o) addImm(areg, areg, o);
        } else {
            loadOp(off, "t4", "ft3");
            I("add " + areg + ", " + areg + ", t4");
        }
        return elem;
    }

    /* Copies 'size' bytes from the address in src to the address in dst. */
    void copyMem(const string &src, const string &dst, long long size, long long align)
    {
        long long chunk = align >= 8 ? 8 : (align >= 4 ? 4 : (align >= 2 ? 2 : 1));
        string ld = chunk == 8 ? "ld" : chunk == 4 ? "lw" : chunk == 2 ? "lh" : "lb";
        string sd = chunk == 8 ? "sd" : chunk == 4 ? "sw" : chunk == 2 ? "sh" : "sb";
        if (size <= chunk * 8) {
            for (long long o = 0; o + chunk <= size; o += chunk) {
                I(ld + " t5, " + num(o) + "(" + src + ")");
                I(sd + " t5, " + num(o) + "(" + dst + ")");
            }
            return;
        }
        string l = string(runtime ? "RCP_" : "CP_") + num(++copyCount);
        loadImm("t6", size / chunk);
        LBL(l);
        I(ld + " t5, 0(" + src + ")");
        I(sd + " t5, 0(" + dst + ")");
        I("addi " + src + ", " + src + ", " + num(chunk));
        I("addi " + dst + ", " + dst + ", " + num(chunk));
        I("addi t6, t6, -1");
        I("bnez t6, " + l);
    }

    /* ---------- expressions ---------- */
    Ty *commonType(Ty *a, Ty *b)
    {
        if (a->k == DOUBLE || b->k == DOUBLE) return T_DOUBLE();
        if (a->k == FLOAT || b->k == FLOAT) return mk(FLOAT);
        if (isPtrLike(a) || isPtrLike(b)) { Ty *t = mk(LONG); t->uns = true; return t; }
        if (a->k == LONG || b->k == LONG) {
            Ty *t = mk(LONG);
            t->uns = (a->k == LONG && a->uns) || (b->k == LONG && b->uns);
            return t;
        }
        Ty *t = mk(INT);
        t->uns = (a->k == INT && a->uns) || (b->k == INT && b->uns);
        return t;
    }

    bool isCmp(const string &op)
    {
        return op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
    }

    /* Evaluates "a op b" leaving the result in t0 / ft0 and returning its type. */
    Ty *evalBinary(const string &a, const string &op, const string &b)
    {
        Ty *ta = loadOp(a, "t0", "ft0");
        Ty *tb = loadOp(b, "t1", "ft1");
        bool pa = isPtrLike(ta), pb = isPtrLike(tb);
        if ((op == "+" || op == "-") && (pa || pb)) {
            if (pa && pb) { I("sub t0, t0, t1"); return T_LONG(); }
            I(string(op == "+" ? "add" : "sub") + " t0, t0, t1");
            return pa ? ta : tb;
        }
        Ty *ct;
        if (op == "<<" || op == ">>") {
            ct = (ta->k == LONG) ? ta : mk(INT);
            if (ct->k == INT) ct->uns = ta->k == INT && ta->uns;
            conv(ta, ct, "t0", "ft0");
        } else {
            ct = commonType(ta, tb);
            conv(ta, ct, "t0", "ft0");
            conv(tb, ct, "t1", "ft1");
        }
        if (isFlt(ct)) {
            string s = ct->k == DOUBLE ? "d" : "s";
            if (op == "+") I("fadd." + s + " ft0, ft0, ft1");
            else if (op == "-") I("fsub." + s + " ft0, ft0, ft1");
            else if (op == "*") I("fmul." + s + " ft0, ft0, ft1");
            else if (op == "/") I("fdiv." + s + " ft0, ft0, ft1");
            else if (op == "==") I("feq." + s + " t0, ft0, ft1");
            else if (op == "!=") { I("feq." + s + " t0, ft0, ft1"); I("xori t0, t0, 1"); }
            else if (op == "<") I("flt." + s + " t0, ft0, ft1");
            else if (op == "<=") I("fle." + s + " t0, ft0, ft1");
            else if (op == ">") I("flt." + s + " t0, ft1, ft0");
            else if (op == ">=") I("fle." + s + " t0, ft1, ft0");
            else { fprintf(stderr, "backend: operator '%s' not valid for floating point\n", op.c_str()); }
            return isCmp(op) ? T_INT() : ct;
        }
        bool wide = ct->k == LONG;
        bool u = ct->uns;
        string w = wide ? "" : "w";
        if (op == "+") I("add" + w + " t0, t0, t1");
        else if (op == "-") I("sub" + w + " t0, t0, t1");
        else if (op == "*") I("mul" + w + " t0, t0, t1");
        else if (op == "/") I(string(u ? "divu" : "div") + w + " t0, t0, t1");
        else if (op == "%") I(string(u ? "remu" : "rem") + w + " t0, t0, t1");
        else if (op == "&") I("and t0, t0, t1");
        else if (op == "|") I("or t0, t0, t1");
        else if (op == "^") I("xor t0, t0, t1");
        else if (op == "<<") I("sll" + w + " t0, t0, t1");
        else if (op == ">>") I(string(u ? "srl" : "sra") + w + " t0, t0, t1");
        else if (op == "==") { I("sub t0, t0, t1"); I("seqz t0, t0"); }
        else if (op == "!=") { I("sub t0, t0, t1"); I("snez t0, t0"); }
        else if (op == "<") I(string(u ? "sltu" : "slt") + " t0, t0, t1");
        else if (op == ">") I(string(u ? "sltu" : "slt") + " t0, t1, t0");
        else if (op == "<=") { I(string(u ? "sltu" : "slt") + " t0, t1, t0"); I("xori t0, t0, 1"); }
        else if (op == ">=") { I(string(u ? "sltu" : "slt") + " t0, t0, t1"); I("xori t0, t0, 1"); }
        else fprintf(stderr, "backend: unknown operator '%s'\n", op.c_str());
        return isCmp(op) ? T_INT() : ct;
    }

    vector<string> tokens(const string &s)
    {
        vector<string> out;
        string cur;
        bool inStr = false, inChr = false;
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (inStr) { cur += c; if (c == '\\' && i + 1 < s.size()) cur += s[++i]; else if (c == '"') inStr = false; continue; }
            if (inChr) { cur += c; if (c == '\\' && i + 1 < s.size()) cur += s[++i]; else if (c == '\'') inChr = false; continue; }
            if (c == '"') { inStr = true; cur += c; continue; }
            if (c == '\'') { inChr = true; cur += c; continue; }
            if (c == ' ') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } continue; }
            cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    /* Evaluates an IR right-hand side into t0 / ft0 and returns its type. */
    Ty *evalExpr(const string &rhs)
    {
        string r = trim(rhs);
        if (startsWith(r, "alloc ")) {
            loadOp(trim(r.substr(6)), "t0", "ft0");
            I("addi sp, sp, -16");
            I("sd t0, 0(sp)");
            I("jal ra, fn___rt_alloc");
            I("addi sp, sp, 16");
            I("mv t0, a0");
            return mk(PTR, T_VOID());
        }
        if (startsWith(r, "va_start ")) {
            string n = trim(r.substr(9));
            Var *v = findVar(n);
            long long off = v ? v->off + slotSize(v->ty) : 0;
            addImm("t0", "s0", off);
            return mk(PTR, T_CHAR());
        }
        if (startsWith(r, "va_arg ")) return evalVaArg(trim(r.substr(7)));
        if (r[0] == '(') {
            size_t close = r.find(')');
            int depth = 0;
            for (size_t i = 0; i < r.size(); i++) {
                if (r[i] == '(') depth++;
                else if (r[i] == ')' && --depth == 0) { close = i; break; }
            }
            Ty *to = parseType(r.substr(1, close - 1));
            Ty *from = loadOp(trim(r.substr(close + 1)), "t0", "ft0");
            conv(from, to, "t0", "ft0");
            return to;
        }
        vector<string> tk = tokens(r);
        if (tk.size() == 3) return evalBinary(tk[0], tk[1], tk[2]);
        if (r[0] == '&' && r.size() > 1) return evalAddrOf(trim(r.substr(1)));
        if (r.size() > 1 && (r[0] == '-' || r[0] == '!' || r[0] == '~') && !isIntLit(r) && !isFloatLit(r)) {
            Ty *t = loadOp(trim(r.substr(1)), "t0", "ft0");
            if (r[0] == '-') {
                if (t->k == DOUBLE) I("fneg.d ft0, ft0");
                else if (t->k == FLOAT) I("fneg.s ft0, ft0");
                else I(string(t->k == LONG ? "sub" : "subw") + " t0, zero, t0");
                return t;
            }
            if (r[0] == '~') { I("xori t0, t0, -1"); return t; }
            if (isFlt(t)) { conv(t, mk(BOOL), "t0", "ft0"); I("xori t0, t0, 1"); }
            else I("seqz t0, t0");
            return T_INT();
        }
        if (isLvalueText(r)) {
            Ty *lt = lvAddr(r, "t3");
            if (lt->k == ARRAY) { I("mv t0, t3"); return mk(PTR, lt->base); }
            if (lt->k == STRUCT) { I("mv t0, t3"); return lt; }
            if (lt->k == FUNC) { I("mv t0, t3"); return mk(PTR, lt); }
            ldOp(lt, isFlt(lt) ? "ft0" : "t0", "0(t3)");
            return lt;
        }
        return loadOp(r, "t0", "ft0");
    }

    Ty *evalAddrOf(const string &x)
    {
        if (isLvalueText(x)) {
            Ty *lt = lvAddr(x, "t0");
            return mk(PTR, lt);
        }
        Var *v = findVar(x);
        if (v) {
            addrVar(*v, "t0");
            if (!v->global && addrHolders.count(x)) { string m = memVar(*v, "t5"); I("ld t0, " + m); }
            return mk(PTR, v->ty->k == ARRAY ? v->ty->base : v->ty);
        }
        if (funcs.count(x) || libTable().count(x) || startsWith(x, "__")) {
            I("la t0, " + funcLabel(x));
            return mk(PTR, mk(FUNC, T_INT()));
        }
        loadImm("t0", 0);
        return mk(PTR, T_INT());
    }

    Ty *evalVaArg(const string &spec)
    {
        size_t c = spec.find(',');
        string ap = trim(spec.substr(0, c));
        Ty *t = parseType(trim(spec.substr(c + 1)));
        Var *v = findVar(ap);
        string m = memVar(*v, "t5");
        I("ld t3, " + m);
        if (t->k == STRUCT) I("mv t0, t3");
        else ldOp(t, isFlt(t) ? "ft0" : "t0", "0(t3)");
        addImm("t3", "t3", slotSize(t));
        m = memVar(*v, "t5");
        I("sd t3, " + m);
        return t;
    }

    /* ---------- assignment ---------- */
    /* Stores the value currently in t0 / ft0 (with type vt) into lhs (an lvalue, variable or temp). */
    void assignTo(const string &lhs, Ty *vt)
    {
        if (isLvalueText(lhs)) {
            Ty *lt = lvAddr(lhs, "t3");
            if (lt->k == STRUCT) { copyMem("t0", "t3", sizeOf(lt), alignOf(lt)); return; }
            conv(vt, lt, "t0", "ft0");
            stOp(lt, isFlt(lt) ? "ft0" : "t0", "0(t3)");
            return;
        }
        Var *v = findVar(lhs);
        if (!v) {
            Var nv;
            Ty *ty = vt;
            if (ty->k == STRUCT) addrHolders.insert(lhs);
            if (ty->k == ARRAY) ty = mk(PTR, ty->base);
            nv.ty = ty;
            nv.off = allocSlot(8);
            temps[lhs] = nv;
            v = &temps[lhs];
        }
        Ty *dt = v->ty;
        bool holder = !v->global && addrHolders.count(lhs);
        if (dt->k == STRUCT && !holder) {
            addrVar(*v, "t3");
            copyMem("t0", "t3", sizeOf(dt), alignOf(dt));
            return;
        }
        if (dt->k == STRUCT) { string m = memVar(*v, "t5"); I("sd t0, " + m); return; }
        if (dt->k == ARRAY) return;
        conv(vt, dt, "t0", "ft0");
        string m = memVar(*v, "t5");
        stOp(dt, isFlt(dt) ? "ft0" : "t0", m);
    }

    /* ---------- calls ---------- */
    /* Performs "[dest =] call target, argc": arguments were queued by 'param'
       instructions; each occupies an 8-byte-aligned slot on the stack. */
    void doCall(const string &dest, const string &callee, int argc)
    {
        vector<string> args;
        if (argc > (int)pend.size()) argc = (int)pend.size();
        args.assign(pend.end() - argc, pend.end());
        pend.resize(pend.size() - argc);
        string target = trim(callee);
        bool indirect = !target.empty() && target[0] == '*';
        FuncInfo *fi = nullptr;
        Ty *ret = T_INT();
        vector<Ty *> ptypes;
        bool variadicFn = false;
        if (indirect) {
            Var *fv = findVar(target.substr(1));
            Ty *ft = fv ? fv->ty : nullptr;
            while (ft && ft->k == PTR) ft = ft->base;
            if (ft && ft->k == FUNC) { ret = ft->base; ptypes = ft->params; variadicFn = ft->variadic; }
        } else if (funcs.count(target)) {
            fi = &funcs[target];
            ret = fi->ret;
            ptypes = fi->ptypes;
            variadicFn = fi->variadic;
        } else if (libTable().count(target) || startsWith(target, "__")) {
            map<string, LibSig>::iterator it = libTable().find(target);
            if (it != libTable().end()) { ret = libRet(it->second.ret); variadicFn = it->second.variadic; }
            if (target == "__ecall") ret = T_LONG();
            if (target == "__fsqrt") ret = T_DOUBLE();
        }
        if (target == "__ecall") { doEcall(dest, args); return; }
        if (target == "__fsqrt") {
            loadOp(args[0], "t0", "ft0");
            I("fsqrt.d ft0, ft0");
            if (!dest.empty()) assignTo(dest, T_DOUBLE());
            return;
        }
        vector<Ty *> atypes;
        long long total = 0;
        for (size_t i = 0; i < args.size(); i++) {
            Ty *t = opType(args[i]);
            atypes.push_back(t);
            total += slotSize(t);
        }
        total = (total + 15) / 16 * 16;
        if (total) { if (total <= 2000) I("addi sp, sp, " + num(-total)); else { loadImm("t5", total); I("sub sp, sp, t5"); } }
        long long off = 0;
        for (size_t i = 0; i < args.size(); i++) {
            Ty *ta = loadOp(args[i], "t0", "ft0");
            Ty *want = i < ptypes.size() ? ptypes[i] : nullptr;
            if (want && want->k != STRUCT && want->k != ARRAY) { conv(ta, want, "t0", "ft0"); ta = want; }
            else if (!want && ta->k == FLOAT) { conv(ta, T_DOUBLE(), "t0", "ft0"); ta = T_DOUBLE(); }
            if (ta->k == STRUCT) { addImm("t3", "sp", off); copyMem("t0", "t3", sizeOf(ta), alignOf(ta)); }
            else if (isFlt(ta)) I(string(ta->k == DOUBLE ? "fsd" : "fsw") + " ft0, " + num(off) + "(sp)");
            else I("sd t0, " + num(off) + "(sp)");
            off += slotSize(atypes[i]);
        }
        (void)variadicFn;
        if (indirect) {
            loadOp(target.substr(1), "t6", "ft2");
            I("jalr ra, t6, 0");
        } else {
            I("jal ra, " + funcLabel(target));
        }
        if (total) { if (total <= 2000) I("addi sp, sp, " + num(total)); else { loadImm("t5", total); I("add sp, sp, t5"); } }
        if (dest.empty() || ret->k == VOID) return;
        if (ret->k == DOUBLE) I("fmv.d ft0, fa0");
        else if (ret->k == FLOAT) I("fmv.s ft0, fa0");
        else I("mv t0, a0");
        assignTo(dest, ret);
    }

    void doEcall(const string &dest, const vector<string> &args)
    {
        const char *regs[4] = {"a7", "a0", "a1", "a2"};
        for (size_t i = 0; i < args.size() && i < 4; i++) {
            Ty *t = loadOp(args[i], "t0", "ft0");
            conv(t, T_LONG(), "t0", "ft0");
            I(string("mv ") + regs[i] + ", t0");
        }
        I("ecall");
        if (!dest.empty()) { I("mv t0, a0"); assignTo(dest, T_LONG()); }
    }

    /* ---------- statements ---------- */
    void doReturn(const string &rest)
    {
        if (!rest.empty()) {
            Ty *t = loadOp(rest, "t0", "ft0");
            Ty *rt = cur->ret;
            if (rt->k != VOID && rt->k != STRUCT) conv(t, rt, "t0", "ft0");
            if (isFlt(rt)) I(string(rt->k == DOUBLE ? "fmv.d" : "fmv.s") + " fa0, ft0");
            else I("mv a0, t0");
        }
        I("j E_" + fnMangled);
    }

    string irLabel(const string &l) { return "L_" + fnMangled + "_" + l; }

    void doIf(const string &line)
    {
        vector<string> tk = tokens(line);
        string skip = string(runtime ? "RK_" : "K_") + num(++skipCount);
        if (tk.size() == 4) {
            bool jumpIfFalse = tk[0] == "ifFalse";
            Ty *t = loadOp(tk[1], "t0", "ft0");
            if (isFlt(t)) conv(t, mk(BOOL), "t0", "ft0");
            I(string(jumpIfFalse ? "bnez" : "beqz") + " t0, " + skip);
            I("j " + irLabel(tk[3]));
            LBL(skip);
            return;
        }
        if (tk.size() < 6) return;
        evalBinary(tk[1], tk[2], tk[3]);
        I("beqz t0, " + skip);
        I("j " + irLabel(tk[5]));
        LBL(skip);
    }

    void doLocal(const string &decl)
    {
        string name;
        bool isRef;
        Ty *t = parseDecl(decl, name, isRef);
        if (isRef) t = mk(PTR, t);
        Var v;
        v.ty = t;
        v.off = allocSlot(sizeOf(t));
        long long al = alignOf(t);
        if (al > 8) al = 8;
        vars[name] = v;
        temps.erase(name);
    }

    /* Translates one IR line (declaration, label, jump, call, return or assignment) into assembly. */
    void stmt(const string &raw)
    {
        string line = trim(raw);
        if (line.empty() || line[0] == ';') return;
        if (startsWith(line, "local ")) { doLocal(line.substr(6)); return; }
        if (line[line.size() - 1] == ':' && line.find(' ') == string::npos) { LBL(irLabel(line.substr(0, line.size() - 1))); return; }
        if (startsWith(line, "goto ")) { I("j " + irLabel(trim(line.substr(5)))); return; }
        if (startsWith(line, "if ") || startsWith(line, "ifFalse ")) { doIf(line); return; }
        if (startsWith(line, "param ")) { pend.push_back(trim(line.substr(6))); return; }
        if (startsWith(line, "call ")) {
            string rest = trim(line.substr(5));
            size_t c = rest.rfind(',');
            doCall("", trim(rest.substr(0, c)), atoi(rest.c_str() + c + 1));
            return;
        }
        if (line == "return") { doReturn(""); return; }
        if (startsWith(line, "return ")) { doReturn(trim(line.substr(7))); return; }
        if (startsWith(line, "free ")) {
            loadOp(trim(line.substr(5)), "t0", "ft0");
            I("addi sp, sp, -16");
            I("sd t0, 0(sp)");
            I("jal ra, fn___rt_free");
            I("addi sp, sp, 16");
            return;
        }
        if (startsWith(line, "va_end")) return;
        size_t eq = string::npos;
        bool inStr = false;
        for (size_t i = 0; i + 2 < line.size(); i++) {
            if (line[i] == '"') inStr = !inStr;
            if (!inStr && line[i] == ' ' && line[i + 1] == '=' && line[i + 2] == ' ') { eq = i; break; }
        }
        if (eq == string::npos) { fprintf(stderr, "backend: cannot translate '%s'\n", line.c_str()); return; }
        string lhs = trim(line.substr(0, eq));
        string rhs = trim(line.substr(eq + 3));
        if (startsWith(rhs, "call ")) {
            string rest = trim(rhs.substr(5));
            size_t c = rest.rfind(',');
            doCall(lhs, trim(rest.substr(0, c)), atoi(rest.c_str() + c + 1));
            return;
        }
        Ty *vt = evalExpr(rhs);
        assignTo(lhs, vt);
    }

    /* ---------- functions ---------- */
    /* Parses an IR function header "func name(params) : ret" into a FuncInfo record. */
    FuncInfo parseHeader(const string &line)
    {
        FuncInfo f;
        size_t open = line.find('(');
        f.name = trim(line.substr(5, open - 5));
        size_t close = string::npos;
        int depth = 0;
        for (size_t i = open; i < line.size(); i++) {
            if (line[i] == '(') depth++;
            else if (line[i] == ')' && --depth == 0) { close = i; break; }
        }
        string params = line.substr(open + 1, close - open - 1);
        vector<string> ps = splitTopLevel(params, ',');
        for (size_t i = 0; i < ps.size(); i++) {
            if (ps[i].empty()) continue;
            if (ps[i] == "...") { f.variadic = true; continue; }
            string nm;
            bool ref;
            Ty *t = parseDecl(ps[i], nm, ref);
            if (ref) t = mk(PTR, t);
            f.pnames.push_back(nm);
            f.ptypes.push_back(t);
            f.prefs.push_back(ref);
        }
        string rest = trim(line.substr(close + 1));
        if (startsWith(rest, ":")) rest = trim(rest.substr(1));
        f.ret = parseType(rest);
        return f;
    }

    /* Generates one function: the body is built first so that the final frame
       size is known when the prologue is written. */
    void genFunction(const vector<string> &lines, size_t &i)
    {
        FuncInfo &f = funcs[parseHeader(lines[i]).name];
        cur = &f;
        fnMangled = mangle(f.name);
        fnLabel = "fn_" + fnMangled;
        vars.clear();
        temps.clear();
        addrHolders.clear();
        pend.clear();
        frameTop = 0;
        body.str("");
        body.clear();
        long long off = 0;
        for (size_t p = 0; p < f.pnames.size(); p++) {
            Var v;
            v.ty = f.ptypes[p];
            v.off = off;
            off += slotSize(v.ty);
            vars[f.pnames[p]] = v;
        }
        for (i++; i < lines.size(); i++) {
            string l = trim(lines[i]);
            if (startsWith(l, "end ") && trim(l.substr(4)) == f.name) break;
            stmt(l);
        }
        if (f.name == "main" && f.ret->k != VOID) I("li a0, 0");
        long long fs = (16 + frameTop + 15) / 16 * 16;
        text << "\n" << fnLabel << ":\n";
        text << "    mv t6, s0\n";
        if (fs <= 2000) {
            text << "    addi sp, sp, " << -fs << "\n";
            text << "    addi s0, sp, " << fs << "\n";
        } else {
            text << "    li t5, " << fs << "\n";
            text << "    sub sp, sp, t5\n";
            text << "    add s0, sp, t5\n";
        }
        text << "    sd ra, -8(s0)\n";
        text << "    sd t6, -16(s0)\n";
        text << body.str();
        text << "E_" << fnMangled << ":\n";
        text << "    ld ra, -8(s0)\n";
        text << "    ld t6, -16(s0)\n";
        text << "    mv sp, s0\n";
        text << "    mv s0, t6\n";
        text << "    ret\n";
    }

    /* ---------- data section ---------- */
    void parseStruct(const string &raw)
    {
        string s = trim(raw);
        size_t sp = s.find(' ');
        string kind = s.substr(0, sp);
        size_t colon = s.find(" : size ");
        string tag = trim(s.substr(sp + 1, colon - sp - 1));
        size_t brace = s.find('{');
        SInfo si;
        si.isUnion = kind == "union";
        si.size = atoll(s.c_str() + colon + 8);
        string inner = s.substr(brace + 1, s.rfind('}') - brace - 1);
        vector<string> fs = splitTopLevel(inner, ';');
        for (size_t i = 0; i < fs.size(); i++) {
            if (fs[i].empty()) continue;
            size_t at = fs[i].rfind('@');
            string decl = trim(fs[i].substr(0, at));
            Fld f;
            bool ref;
            f.ty = parseDecl(decl, f.name, ref);
            if (ref) f.ty = mk(PTR, f.ty);
            f.off = atoll(fs[i].c_str() + at + 1);
            si.fields.push_back(f);
        }
        g_structs[tag] = si;
    }

    /* Writes a global initialiser (scalar, string or brace list) into the byte image of the variable. */
    void fillInit(Ty *t, const string &init, vector<uint8_t> &buf, long long off, map<long long, string> &rel)
    {
        string s = trim(init);
        if (s.empty() || off >= (long long)buf.size()) return;
        if (s[0] == '{') {
            vector<string> parts = splitTopLevel(s.substr(1, s.size() - 2), ',');
            if (t->k == ARRAY) {
                long long es = sizeOf(t->base);
                for (size_t i = 0; i < parts.size(); i++) fillInit(t->base, parts[i], buf, off + (long long)i * es, rel);
            } else if (t->k == STRUCT) {
                SInfo &si = g_structs[t->tag];
                for (size_t i = 0; i < parts.size() && i < si.fields.size(); i++) {
                    fillInit(si.fields[i].ty, parts[i], buf, off + si.fields[i].off, rel);
                    if (si.isUnion) break;
                }
            } else if (!parts.empty()) fillInit(t, parts[0], buf, off, rel);
            return;
        }
        if (s[0] == '"') {
            string d = decodeEscapes(s.substr(1, s.size() - 2));
            if (t->k == ARRAY) {
                for (size_t i = 0; i < d.size() && off + (long long)i < (long long)buf.size(); i++) buf[off + i] = (uint8_t)d[i];
            } else rel[off] = strLabel(d);
            return;
        }
        if (isFlt(t)) {
            double dv = strtod(s.c_str(), nullptr);
            if (t->k == DOUBLE) memcpy(&buf[off], &dv, 8);
            else { float fv = (float)dv; memcpy(&buf[off], &fv, 4); }
            return;
        }
        long long v = isFloatLit(s) ? (long long)strtod(s.c_str(), nullptr) : strtoll(s.c_str(), nullptr, 0);
        long long sz = sizeOf(t);
        for (long long b = 0; b < sz && off + b < (long long)buf.size(); b++) buf[off + b] = (uint8_t)((v >> (8 * b)) & 0xff);
    }

    string gdata;

    /* Parses one global line of the IR and emits its storage in the data section. */
    void parseGlobal(const string &raw)
    {
        string s = trim(raw);
        bool inStr = false;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '"') inStr = !inStr;
            if (!inStr && s[i] == ';') { s = trim(s.substr(0, i)); break; }
        }
        s = trim(s.substr(7));
        string decl = s, init;
        inStr = false;
        for (size_t i = 0; i + 2 < s.size(); i++) {
            if (s[i] == '"') inStr = !inStr;
            if (!inStr && s[i] == ' ' && s[i + 1] == '=' && s[i + 2] == ' ') { decl = s.substr(0, i); init = trim(s.substr(i + 3)); break; }
        }
        string name;
        bool ref;
        Ty *t = parseDecl(decl, name, ref);
        globals[name] = t;
        long long size = sizeOf(t);
        if (size < 1) size = 1;
        vector<uint8_t> buf((size_t)size, 0);
        map<long long, string> rel;
        if (!init.empty()) fillInit(t, init, buf, 0, rel);
        std::ostringstream o;
        o << "    .align 3\nG_" << mangle(name) << ":\n";
        long long pos = 0;
        while (pos < size) {
            if (rel.count(pos)) { o << "    .dword " << rel[pos] << "\n"; pos += 8; continue; }
            o << "    .byte ";
            int n = 0;
            while (pos < size && n < 16 && !rel.count(pos)) { o << (n ? ", " : "") << (int)buf[pos]; pos++; n++; }
            o << "\n";
        }
        gdata += o.str();
    }

    string escapeForAsm(const string &d, bool &ok)
    {
        string o;
        ok = true;
        for (size_t i = 0; i < d.size(); i++) {
            unsigned char c = (unsigned char)d[i];
            if (c == '\n') o += "\\n";
            else if (c == '\t') o += "\\t";
            else if (c == '"') o += "\\\"";
            else if (c == '\\') o += "\\\\";
            else if (c >= 32 && c < 127) o += (char)c;
            else ok = false;
        }
        return o;
    }

    string emitPools()
    {
        std::ostringstream o;
        for (size_t i = 0; i < strOrder.size(); i++) {
            const string &d = strOrder[i];
            o << strLabels[d] << ":\n";
            bool ok;
            string e = escapeForAsm(d, ok);
            if (ok) o << "    .asciz \"" << e << "\"\n";
            else {
                o << "    .byte ";
                for (size_t k = 0; k < d.size(); k++) o << (int)(unsigned char)d[k] << ", ";
                o << "0\n";
            }
        }
        o << "    .align 3\n";
        for (map<string, string>::iterator it = fconsts.begin(); it != fconsts.end(); ++it) {
            char b[64];
            snprintf(b, sizeof b, "%.17g", strtod(it->first.c_str(), nullptr));
            o << it->second << ":\n    .double " << b << "\n";
        }
        return o.str();
    }

    /* Emits the program entry: copies argc/argv into 8-byte slots, calls main and exits with its result. */
    string entryStub()
    {
        string pn = strLabel("a.out");
        std::ostringstream o;
        o << "_start:\n"
             "    mv s1, a0\n"
             "    mv s2, a1\n"
             "    addi a0, s1, 2\n"
             "    slli a0, a0, 3\n"
             "    li a7, 9\n"
             "    ecall\n"
             "    mv s3, a0\n"
             "    la t0, " << pn << "\n"
             "    sd t0, 0(s3)\n"
             "    li t1, 0\n"
             "ARGS_LOOP:\n"
             "    bge t1, s1, ARGS_DONE\n"
             "    slli t2, t1, 2\n"
             "    add t2, s2, t2\n"
             "    lw t3, 0(t2)\n"
             "    addi t4, t1, 1\n"
             "    slli t4, t4, 3\n"
             "    add t4, s3, t4\n"
             "    sd t3, 0(t4)\n"
             "    addi t1, t1, 1\n"
             "    j ARGS_LOOP\n"
             "ARGS_DONE:\n"
             "    addi t4, s1, 1\n"
             "    slli t4, t4, 3\n"
             "    add t4, s3, t4\n"
             "    sd zero, 0(t4)\n"
             "    addi a0, s1, 1\n"
             "    addi sp, sp, -16\n"
             "    sd a0, 0(sp)\n"
             "    sd s3, 8(sp)\n"
             "    jal ra, fn_main\n"
             "    addi sp, sp, 16\n"
             "    li a7, 93\n"
             "    ecall\n";
        return o.str();
    }

    /* ---------- driver ---------- */
    /* Parses all IR sections (.types, .data, .text) and emits the complete RISC-V assembly file. */
    string run(const string &ir)
    {
        vector<string> lines;
        {
            std::istringstream in(ir);
            string l;
            while (std::getline(in, l)) lines.push_back(l);
        }
        int section = 0;
        for (size_t i = 0; i < lines.size(); i++) {
            string l = trim(lines[i]);
            if (l == ".types") { section = 1; continue; }
            if (l == ".data") { section = 2; continue; }
            if (l == ".text") { section = 3; continue; }
            if (l.empty() || l[0] == ';') continue;
            if (section == 1) parseStruct(l);
            else if (section == 2) parseGlobal(l);
            else if (section == 3 && startsWith(l, "func ")) {
                FuncInfo f = parseHeader(l);
                funcs[f.name] = f;
            }
        }
        string stub;
        if (!runtime) {
            globals["stdin"] = mk(PTR, T_VOID());
            globals["stdout"] = mk(PTR, T_VOID());
            globals["stderr"] = mk(PTR, T_VOID());
            gdata += "    .align 3\nG_stdin:\n    .dword 0\nG_stdout:\n    .dword 1\nG_stderr:\n    .dword 2\n";
            stub = entryStub();
        }
        section = 0;
        for (size_t i = 0; i < lines.size(); i++) {
            string l = trim(lines[i]);
            if (l == ".text") { section = 3; continue; }
            if (section == 3 && startsWith(l, "func ")) genFunction(lines, i);
        }
        string out;
        out += "# RISC-V (RV64IMFD) assembly - run with: java -jar rars.jar rv64 nc me <file>.s\n";
        out += ".data\n" + gdata + emitPools() + "\n.text\n";
        out += stub + text.str();
        return out;
    }
};

} /* namespace */

bool generate_asm(const std::string &ir, const std::string &outfile, bool runtimeMode)
{
    Backend b;
    b.runtime = runtimeMode;
    string out = b.run(ir);
    FILE *f = fopen(outfile.c_str(), "w");
    if (!f) {
        fprintf(stderr, "Error: cannot write '%s'\n", outfile.c_str());
        return false;
    }
    fputs(out.c_str(), f);
    if (!runtimeMode && g_runtime_asm) fputs(g_runtime_asm, f);
    fclose(f);
    return true;
}
