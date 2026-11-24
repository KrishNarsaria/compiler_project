/* ============================================================
 * lexer.l - Lexical analyzer for the C++ subset.
 *
 * Supplies tokens to the parser and still diagnoses every
 * lexical error (bad numbers, bad escapes, unterminated
 * literals and comments, stray characters).
 * ============================================================ */

%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "ast.h"
#include "parser.tab.hpp"

int line_num = 1;
static int comment_start = 0;

/* Tracks 'class X {' bodies so the lexer can recognise X( as a constructor name. */
struct ClassFrame { std::string name; int depth; };
static std::vector<ClassFrame> class_stack;
static int  brace_depth = 0;
static int  expect_tag = 0;
static std::string pending_tag;

static void note_tag(const std::string &name)
{
    if (expect_tag) { pending_tag = name; expect_tag = 0; }
}

static void note_punct(char c)
{
    switch (c) {
    case '{':
        brace_depth++;
        if (!pending_tag.empty()) {
            ClassFrame f; f.name = pending_tag; f.depth = brace_depth;
            class_stack.push_back(f);
        }
        pending_tag.clear();
        expect_tag = 0;
        break;
    case '}':
        if (!class_stack.empty() && class_stack.back().depth == brace_depth) class_stack.pop_back();
        brace_depth--;
        break;
    case ';': case '(': case ')': case '=':
        pending_tag.clear();
        expect_tag = 0;
        break;
    default:
        break;
    }
}

static bool next_is_paren();

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = line_num; \
    yylloc.first_column = yylloc.last_column = 0;

static void lex_error(const std::string &m) { report_error("LEXICAL", line_num, m); }
static void lex_error_at(int line, const std::string &m) { report_error("LEXICAL", line, m); }

static int is_hex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

/* Checks every backslash sequence inside a literal (quotes included). */
static void validate_escapes(const char *text)
{
    int len = (int)strlen(text);
    int i = 0;
    while (i < len) {
        if (text[i] == '\\' && i + 1 < len) {
            char c = text[i + 1];
            switch (c) {
            case 'n': case 't': case 'r': case 'a': case 'b': case 'f':
            case 'v': case '\\': case '\'': case '"': case '?': case '0':
                i += 2;
                break;
            case 'x':
                if (i + 2 < len && is_hex(text[i + 2])) {
                    i += 3;
                } else {
                    lex_error(std::string("Invalid hex escape sequence '\\x") +
                              ((i + 2 < len) ? text[i + 2] : '?') + "'");
                    i += 2;
                }
                break;
            default:
                if (c >= '1' && c <= '7') {
                    i += 2;
                } else if (c == '8' || c == '9') {
                    lex_error(std::string("Invalid octal escape sequence '\\") + c +
                              "' (digit out of range 0-7)");
                    i += 2;
                } else {
                    lex_error(std::string("Unknown escape sequence '\\") + c + "'");
                    i += 2;
                }
            }
        } else {
            i++;
        }
    }
}

/* Counts logical characters of a char literal (quotes included). */
static int count_char_literal(const char *text)
{
    int len = (int)strlen(text);
    int count = 0, i = 1, end = len - 1;
    while (i < end) {
        if (text[i] == '\\' && i + 1 < end) {
            char c = text[i + 1];
            if (c == 'x') {
                int j = i + 2;
                while (j < end && is_hex(text[j])) j++;
                i = (j > i + 2) ? j : i + 2;
            } else if (c >= '0' && c <= '7') {
                int j = i + 1, d = 0;
                while (j < end && d < 3 && text[j] >= '0' && text[j] <= '7') { j++; d++; }
                i = j;
            } else {
                i += 2;
            }
        } else {
            i++;
        }
        count++;
    }
    return count;
}

/* Decodes the body of a literal (quotes already removed) into bytes. */
static std::string decode_body(const char *s, int n)
{
    std::string out;
    int i = 0;
    while (i < n) {
        if (s[i] != '\\' || i + 1 >= n) { out += s[i++]; continue; }
        char c = s[i + 1];
        i += 2;
        switch (c) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'a': out += '\a'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'v': out += '\v'; break;
        case 'x': {
            int v = 0, d = 0;
            while (i < n && is_hex(s[i])) { v = v * 16 + hex_val(s[i]); i++; d++; }
            out += (char)(d ? v : 'x');
            break;
        }
        default:
            if (c >= '0' && c <= '7') {
                int v = c - '0', d = 1;
                while (i < n && d < 3 && s[i] >= '0' && s[i] <= '7') { v = v * 8 + (s[i] - '0'); i++; d++; }
                out += (char)v;
            } else {
                out += c;
            }
        }
    }
    return out;
}

static int is_typename(const char *s)
{
    return g_typedefs.count(s) || g_tags.count(s);
}

static void handle_define(const char *text)
{
    const char *p = strstr(text, "define");
    if (!p) return;
    p += 6;
    while (*p == ' ' || *p == '\t') p++;
    const char *q = p;
    while (isalnum((unsigned char)*q) || *q == '_') q++;
    std::string name(p, q - p);
    if (*q == '(') return;                         /* function-like macros are ignored */
    std::string body(q);
    size_t c = body.find("//");
    if (c != std::string::npos) body = body.substr(0, c);
    size_t b = body.find_first_not_of(" \t\r");
    size_t e = body.find_last_not_of(" \t\r");
    body = (b == std::string::npos) ? "" : body.substr(b, e - b + 1);
    g_macros[name] = body;
}
static int int_token(const char *t)
{
    for (; *t; t++)
        if (*t == 'l' || *t == 'L') return LONG_LIT;
    return INT_LIT;
}

static int flt_token(const char *t)
{
    size_t n = strlen(t);
    if (n && (t[n - 1] == 'f' || t[n - 1] == 'F')) return FLOATF_LIT;
    return FLOAT_LIT;
}

%}

%option noyywrap
%x BLOCK

DIGIT      [0-9]
HEX_DIGIT  [0-9a-fA-F]
OCT_DIGIT  [0-7]
BIN_DIGIT  [01]
LETTER     [a-zA-Z_]
EXPO       [eE][+-]?{DIGIT}+
INT_SUF    ([uU]([lL]|[lL][lL])?|([lL]|[lL][lL])[uU]?)?
FLOAT_SUF  [fFlL]?

%%

[ \t\r]+                    { }
\n                          { line_num++; }
\\\r?\n                     { line_num++; }

"//".*                      { }
"/*"                        { comment_start = line_num; BEGIN(BLOCK); }
<BLOCK>"*/"                 { BEGIN(INITIAL); }
<BLOCK>\n                   { line_num++; }
<BLOCK>.                    { }
<BLOCK><<EOF>>              {
    lex_error_at(comment_start, "Unterminated block comment (opened here, no closing '*/' before end of file)");
    BEGIN(INITIAL);
    yyterminate();
}

^[ \t]*"#"[ \t]*"define"[ \t]+{LETTER}({LETTER}|{DIGIT})*[^\n]*   { handle_define(yytext); }
^[ \t]*"#"[^\n]*            { }

"std"[ \t]*"::"             { }
{LETTER}({LETTER}|{DIGIT})*[ \t]*"::" {
    std::string name(yytext);
    size_t e = name.find_first_of(" \t:");
    yylval.sval = strdup(name.substr(0, e).c_str());
    return CLASSQ;
}

"int"                       { return INT; }
"char"                      { return CHAR; }
"short"                     { return SHORT; }
"long"                      { return LONG; }
"float"                     { return FLOAT; }
"double"                    { return DOUBLE; }
"void"                      { return VOID; }
"bool"                      { return BOOL; }
"signed"                    { return SIGNED; }
"unsigned"                  { return UNSIGNED; }
"const"                     { return CONST; }
"static"                    { return STATIC; }
"extern"                    { return EXTERN; }
"typedef"                   { return TYPEDEF; }
"struct"                    { expect_tag = 1; return STRUCT; }
"class"                     { expect_tag = 1; return CLASS; }
"union"                     { expect_tag = 1; return UNION; }
"public"                    { return PUBLIC; }
"private"                   { return PRIVATE; }
"protected"                 { return PROTECTED; }
"auto"                      { return AUTO; }
"va_arg"                    { return VA_ARG; }
"enum"                      { return ENUM; }
"inline"|"register"|"volatile" { return INLINE; }
"if"                        { return IF; }
"else"                      { return ELSE; }
"while"                     { return WHILE; }
"do"                        { return DO; }
"for"                       { return FOR; }
"until"                     { return UNTIL; }
"switch"                    { return SWITCH; }
"case"                      { return CASE; }
"default"                   { return DEFAULT; }
"break"                     { return BREAK; }
"continue"                  { return CONTINUE; }
"return"                    { return RETURN; }
"goto"                      { return GOTO; }
"sizeof"                    { return SIZEOF; }
"new"                       { return NEW; }
"delete"[ \t]*"["[ \t]*"]"  { return DELETE_ARR; }
"delete"                    { return DELETE; }
"true"                      { return TRUE; }
"false"                     { return FALSE; }
"nullptr"|"NULL"            { return NULLPTR; }
"using"                     { return USING; }
"namespace"                 { return NAMESPACE; }
"cout"|"cerr"               { return COUT; }
"cin"                       { return CIN; }
"endl"                      { return ENDL; }

{LETTER}({LETTER}|{DIGIT})* {
    std::string name(yytext);
    note_tag(name);
    if (!class_stack.empty() && brace_depth == class_stack.back().depth &&
        name == class_stack.back().name && next_is_paren()) {
        yylval.sval = strdup(name.c_str());
        return CTORNAME;
    }
    std::map<std::string, std::string>::iterator m = g_macros.find(name);
    if (m != g_macros.end() && m->second.find(name) == std::string::npos) {
        const std::string body = m->second;
        for (int i = (int)body.size() - 1; i >= 0; i--) unput(body[i]);
    } else if (is_typename(name.c_str())) {
        yylval.sval = strdup(name.c_str());
        return TYPENAME;
    } else {
        yylval.sval = strdup(name.c_str());
        return IDENTIFIER;
    }
}

0[xX]{HEX_DIGIT}+{INT_SUF}  { yylval.ival = (long long)strtoull(yytext, NULL, 16); return int_token(yytext); }
0[bB]{BIN_DIGIT}+{INT_SUF}  { yylval.ival = (long long)strtoull(yytext + 2, NULL, 2); return int_token(yytext); }
0{OCT_DIGIT}+{INT_SUF}      { yylval.ival = (long long)strtoull(yytext, NULL, 8); return int_token(yytext); }
0[0-9]*[89][0-9]*{INT_SUF}  {
    lex_error(std::string("Invalid octal literal '") + yytext + "' (digits 8 and 9 are not valid in base 8)");
    yylval.ival = 0; return INT_LIT;
}

{DIGIT}+"."{DIGIT}*{EXPO}{FLOAT_SUF}   { yylval.fval = strtod(yytext, NULL); return flt_token(yytext); }
{DIGIT}*"."{DIGIT}+{EXPO}{FLOAT_SUF}   { yylval.fval = strtod(yytext, NULL); return flt_token(yytext); }
{DIGIT}+{EXPO}{FLOAT_SUF}              { yylval.fval = strtod(yytext, NULL); return flt_token(yytext); }
{DIGIT}+"."{DIGIT}*{FLOAT_SUF}         { yylval.fval = strtod(yytext, NULL); return flt_token(yytext); }
{DIGIT}*"."{DIGIT}+{FLOAT_SUF}         { yylval.fval = strtod(yytext, NULL); return flt_token(yytext); }
{DIGIT}+{INT_SUF}                      { yylval.ival = (long long)strtoull(yytext, NULL, 10); return int_token(yytext); }

{DIGIT}+({LETTER}|{DIGIT}|".")*        {
    lex_error(std::string("Malformed numeric literal '") + yytext + "'");
    yylval.ival = 0; return INT_LIT;
}
"."{DIGIT}+({LETTER}|{DIGIT}|".")*     {
    lex_error(std::string("Malformed numeric literal '") + yytext + "'");
    yylval.fval = 0; return FLOAT_LIT;
}

\"([^"\\\n]|\\.)*\"         {
    validate_escapes(yytext);
    std::string body = decode_body(yytext + 1, (int)strlen(yytext) - 2);
    yylval.sstr = new std::string(body);
    return STRING_LIT;
}
\"([^"\\\n]|\\.)*\n         {
    line_num++;
    std::string t(yytext);
    t.erase(t.size() - 1);
    lex_error_at(line_num - 1, "Unterminated string literal " + t.substr(0, 60));
    yylval.sstr = new std::string();
    return STRING_LIT;
}
\"([^"\\\n]|\\.)*           {
    lex_error("Unterminated string literal " + std::string(yytext).substr(0, 60));
    yylval.sstr = new std::string();
    return STRING_LIT;
}

\'([^'\\\n]|\\.)*\'         {
    validate_escapes(yytext);
    int n = count_char_literal(yytext);
    std::string body = decode_body(yytext + 1, (int)strlen(yytext) - 2);
    if (n == 0) {
        lex_error("Empty character literal");
        yylval.ival = 0;
    } else {
        if (n > 1)
            lex_error(std::string("Multi-character literal ") + yytext + " (only 1 character allowed)");
        yylval.ival = body.empty() ? 0 : (long long)(signed char)body[0];
    }
    return CHAR_LIT;
}
\'([^'\\\n]|\\.)*\n         {
    line_num++;
    std::string t(yytext);
    t.erase(t.size() - 1);
    lex_error_at(line_num - 1, "Unterminated character literal " + t.substr(0, 60));
    yylval.ival = 0;
    return CHAR_LIT;
}
\'([^'\\\n]|\\.)*           {
    lex_error("Unterminated character literal " + std::string(yytext).substr(0, 60));
    yylval.ival = 0;
    return CHAR_LIT;
}

"<<="                       { return SHL_ASSIGN; }
">>="                       { return SHR_ASSIGN; }
"..."                       { return ELLIPSIS; }
"->"                        { return ARROW; }
"++"                        { return INC; }
"--"                        { return DEC; }
"<<"                        { return LSHIFT; }
">>"                        { return RSHIFT; }
"<="                        { return LE; }
">="                        { return GE; }
"=="                        { return EQ; }
"!="                        { return NE; }
"&&"                        { return ANDAND; }
"||"                        { return OROR; }
"+="                        { return ADD_ASSIGN; }
"-="                        { return SUB_ASSIGN; }
"*="                        { return MUL_ASSIGN; }
"/="                        { return DIV_ASSIGN; }
"%="                        { return MOD_ASSIGN; }
"&="                        { return AND_ASSIGN; }
"|="                        { return OR_ASSIGN; }
"^="                        { return XOR_ASSIGN; }

[-+*/%&|^~!<>=?:;,.(){}\[\]] { note_punct(yytext[0]); return yytext[0]; }

.                           {
    char buf[96];
    snprintf(buf, sizeof(buf), "Unrecognized character '%s' (ASCII %d)", yytext, (unsigned char)yytext[0]);
    lex_error(buf);
}

%%
static bool next_is_paren()
{
    int c;
    std::string skipped;
    while ((c = yyinput()) == ' ' || c == '\t') skipped += (char)c;
    bool r = (c == '(');
    if (c != EOF && c != 0) unput(c);
    for (int i = (int)skipped.size() - 1; i >= 0; i--) unput(skipped[i]);
    return r;
}

