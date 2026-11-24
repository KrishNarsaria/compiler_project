/* ============================================================
 * runtime.c - C library for the RISC-V target
 * Compiled by this compiler itself and appended to every
 * generated assembly file.  System services use RARS ecalls
 * through the __ecall(number, a0, a1, a2) intrinsic.
 * ============================================================ */

long __ecall(long n, long a, long b, long c);
double __fsqrt(double x);

/* ---------------- memory ---------------- */

void *__rt_alloc(long n)
{
    long total = (n + 23) / 8 * 8;
    long p = __ecall(9, total, 0, 0);
    *(long *)p = n;
    return (void *)(p + 16);
}

void __rt_free(void *p)
{
}

void *__rt_memset(void *d, int c, long n)
{
    char *p = (char *)d;
    long i = 0;
    while (i < n) { p[i] = (char)c; i++; }
    return d;
}

void *__rt_memcpy(void *d, void *s, long n)
{
    char *a = (char *)d;
    char *b = (char *)s;
    long i = 0;
    while (i < n) { a[i] = b[i]; i++; }
    return d;
}

void *__rt_memmove(void *d, void *s, long n)
{
    char *a = (char *)d;
    char *b = (char *)s;
    long i;
    if (a < b) {
        i = 0;
        while (i < n) { a[i] = b[i]; i++; }
    } else {
        i = n;
        while (i > 0) { i--; a[i] = b[i]; }
    }
    return d;
}

void *__rt_realloc(void *p, long n)
{
    if (p == 0) return __rt_alloc(n);
    long old = *(long *)((char *)p - 16);
    void *q = __rt_alloc(n);
    __rt_memcpy(q, p, old < n ? old : n);
    return q;
}

/* ---------------- characters ---------------- */

int __rt_isdigit(int c) { return c >= '0' && c <= '9'; }
int __rt_isupper(int c) { return c >= 'A' && c <= 'Z'; }
int __rt_islower(int c) { return c >= 'a' && c <= 'z'; }
int __rt_isalpha(int c) { return __rt_isupper(c) || __rt_islower(c); }
int __rt_isalnum(int c) { return __rt_isalpha(c) || __rt_isdigit(c); }
int __rt_isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int __rt_isxdigit(int c) { return __rt_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int __rt_ispunct(int c) { return c > 32 && c < 127 && !__rt_isalnum(c); }
int __rt_toupper(int c) { return __rt_islower(c) ? c - 32 : c; }
int __rt_tolower(int c) { return __rt_isupper(c) ? c + 32 : c; }

/* ---------------- strings ---------------- */

long __rt_strlen(char *s)
{
    long n = 0;
    while (s[n]) n++;
    return n;
}

char *__rt_strcpy(char *d, char *s)
{
    long i = 0;
    while (s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
    return d;
}

char *__rt_strncpy(char *d, char *s, long n)
{
    long i = 0;
    while (i < n && s[i]) { d[i] = s[i]; i++; }
    while (i < n) { d[i] = 0; i++; }
    return d;
}

char *__rt_strcat(char *d, char *s)
{
    long n = __rt_strlen(d);
    __rt_strcpy(d + n, s);
    return d;
}

char *__rt_strncat(char *d, char *s, long n)
{
    long k = __rt_strlen(d);
    long i = 0;
    while (i < n && s[i]) { d[k + i] = s[i]; i++; }
    d[k + i] = 0;
    return d;
}

int __rt_strcmp(char *a, char *b)
{
    long i = 0;
    while (a[i] && a[i] == b[i]) i++;
    return (int)((unsigned char)a[i]) - (int)((unsigned char)b[i]);
}

int __rt_strncmp(char *a, char *b, long n)
{
    long i = 0;
    while (i < n && a[i] && a[i] == b[i]) i++;
    if (i == n) return 0;
    return (int)((unsigned char)a[i]) - (int)((unsigned char)b[i]);
}

char *__rt_strchr(char *s, int c)
{
    long i = 0;
    while (1) {
        if (s[i] == (char)c) return s + i;
        if (s[i] == 0) return 0;
        i++;
    }
    return 0;
}

char *__rt_strrchr(char *s, int c)
{
    char *r = 0;
    long i = 0;
    while (1) {
        if (s[i] == (char)c) r = s + i;
        if (s[i] == 0) break;
        i++;
    }
    return r;
}

char *__rt_strstr(char *h, char *n)
{
    long hl = __rt_strlen(h);
    long nl = __rt_strlen(n);
    long i = 0;
    while (i + nl <= hl) {
        if (__rt_strncmp(h + i, n, nl) == 0) return h + i;
        i++;
    }
    return 0;
}

char *__rt_strdup(char *s)
{
    long n = __rt_strlen(s);
    char *d = (char *)__rt_alloc(n + 1);
    __rt_strcpy(d, s);
    return d;
}

long __rt_strtol(char *s, char **end, int base)
{
    long i = 0;
    long sign = 1;
    long v = 0;
    while (__rt_isspace(s[i])) i++;
    if (s[i] == '-') { sign = -1; i++; }
    else if (s[i] == '+') i++;
    if ((base == 0 || base == 16) && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { base = 16; i = i + 2; }
    else if (base == 0 && s[i] == '0') base = 8;
    else if (base == 0) base = 10;
    while (1) {
        int c = s[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        i++;
    }
    if (end) *end = s + i;
    return sign * v;
}

long __rt_atol(char *s) { return __rt_strtol(s, 0, 10); }
int __rt_atoi(char *s) { return (int)__rt_strtol(s, 0, 10); }

double __rt_atof(char *s)
{
    long i = 0;
    double sign = 1.0;
    double v = 0.0;
    while (__rt_isspace(s[i])) i++;
    if (s[i] == '-') { sign = -1.0; i++; }
    else if (s[i] == '+') i++;
    while (__rt_isdigit(s[i])) { v = v * 10.0 + (s[i] - '0'); i++; }
    if (s[i] == '.') {
        double scale = 0.1;
        i++;
        while (__rt_isdigit(s[i])) { v = v + (s[i] - '0') * scale; scale = scale / 10.0; i++; }
    }
    if (s[i] == 'e' || s[i] == 'E') {
        long e = __rt_strtol(s + i + 1, 0, 10);
        while (e > 0) { v = v * 10.0; e--; }
        while (e < 0) { v = v / 10.0; e++; }
    }
    return sign * v;
}

/* ---------------- numeric helpers ---------------- */

int __rt_abs(int x) { return x < 0 ? -x : x; }
long __rt_labs(long x) { return x < 0 ? -x : x; }
double __rt_fabs(double x) { return x < 0.0 ? -x : x; }
double __rt_sqrt(double x) { return __fsqrt(x); }

double __rt_floor(double x)
{
    long i = (long)x;
    double d = (double)i;
    if (d > x) return d - 1.0;
    return d;
}

double __rt_ceil(double x)
{
    long i = (long)x;
    double d = (double)i;
    if (d < x) return d + 1.0;
    return d;
}

double __rt_round(double x)
{
    if (x < 0.0) return -__rt_floor(-x + 0.5);
    return __rt_floor(x + 0.5);
}

double __rt_fmod(double a, double b)
{
    long q = (long)(a / b);
    return a - (double)q * b;
}

double __rt_exp(double x)
{
    long k = (long)(x / 0.6931471805599453);
    double r = x - (double)k * 0.6931471805599453;
    double term = 1.0;
    double sum = 1.0;
    int i = 1;
    while (i < 30) { term = term * r / (double)i; sum = sum + term; i++; }
    while (k > 0) { sum = sum * 2.0; k--; }
    while (k < 0) { sum = sum / 2.0; k++; }
    return sum;
}

double __rt_log(double x)
{
    if (x <= 0.0) return -1.0e300;
    long k = 0;
    while (x > 1.5) { x = x / 2.0; k++; }
    while (x < 0.75) { x = x * 2.0; k--; }
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double term = y;
    double sum = 0.0;
    int i = 1;
    while (i < 60) { sum = sum + term / (double)i; term = term * y2; i = i + 2; }
    return 2.0 * sum + (double)k * 0.6931471805599453;
}

double __rt_log10(double x) { return __rt_log(x) / 2.302585092994046; }

double __rt_pow(double b, double e)
{
    long n = (long)e;
    if ((double)n == e) {
        double r = 1.0;
        long m = n < 0 ? -n : n;
        double base = b;
        while (m > 0) {
            if (m % 2 == 1) r = r * base;
            base = base * base;
            m = m / 2;
        }
        return n < 0 ? 1.0 / r : r;
    }
    return __rt_exp(e * __rt_log(b));
}

double __rt_sin(double x)
{
    double two_pi = 6.283185307179586;
    x = x - (double)((long)(x / two_pi)) * two_pi;
    if (x > 3.141592653589793) x = x - two_pi;
    if (x < -3.141592653589793) x = x + two_pi;
    double term = x;
    double sum = x;
    int i = 1;
    while (i < 25) { term = -term * x * x / (double)((2 * i) * (2 * i + 1)); sum = sum + term; i++; }
    return sum;
}

double __rt_cos(double x) { return __rt_sin(x + 1.5707963267948966); }
double __rt_tan(double x) { return __rt_sin(x) / __rt_cos(x); }

double __rt_atan(double x)
{
    int neg = 0;
    int inv = 0;
    if (x < 0.0) { neg = 1; x = -x; }
    if (x > 1.0) { inv = 1; x = 1.0 / x; }
    double r = x;
    int k = 0;
    while (k < 3) { r = r / (1.0 + __fsqrt(1.0 + r * r)); k++; }
    double x2 = r * r;
    double term = r;
    double sum = 0.0;
    int i = 0;
    while (i < 30) { sum = sum + term / (double)(2 * i + 1); term = -term * x2; i++; }
    sum = sum * 8.0;
    if (inv) sum = 1.5707963267948966 - sum;
    return neg ? -sum : sum;
}

/* ---------------- process ---------------- */

long __rt_seed = 1;

int __rt_rand()
{
    __rt_seed = (__rt_seed * 1103515245 + 12345) % 2147483648;
    return (int)(__rt_seed / 65536 % 32768);
}

void __rt_srand(int s) { __rt_seed = s; }
long __rt_clock() { return __ecall(30, 0, 0, 0); }
long __rt_time(long *t) { long r = __ecall(30, 0, 0, 0) / 1000; if (t) *t = r; return r; }
void __rt_exit(int c) { __ecall(93, c, 0, 0); }

void __rt_qsort(void *base, long n, long size, int (*cmp)(void *, void *))
{
    char *a = (char *)base;
    char *tmp = (char *)__rt_alloc(size);
    long i = 1;
    while (i < n) {
        long j = i;
        __rt_memcpy(tmp, a + i * size, size);
        while (j > 0 && cmp((void *)(a + (j - 1) * size), (void *)tmp) > 0) {
            __rt_memcpy(a + j * size, a + (j - 1) * size, size);
            j--;
        }
        __rt_memcpy(a + j * size, tmp, size);
        i++;
    }
}

/* ---------------- files and console input ---------------- */

struct RtFile { long fd; long eof; long unget; long mode; };

long __rt_stdin_unget = -2;
long __rt_stdin_eof = 0;

long __rt_fdof(void *f)
{
    long v = (long)f;
    if (v < 4) return v;
    return ((struct RtFile *)f)->fd;
}

/* Reads the next byte from a file or stdin (honouring any ungetc pushback), returning -1 on EOF. */
int __rt_fgetc(void *f)
{
    long v = (long)f;
    long fd;
    char ch;
    long n;
    if (v < 4) {
        if (v != 0) return -1;
        if (__rt_stdin_unget != -2) {
            int c = (int)__rt_stdin_unget;
            __rt_stdin_unget = -2;
            return c;
        }
        fd = 0;
    } else {
        struct RtFile *o = (struct RtFile *)f;
        if (o->unget != -2) {
            int c = (int)o->unget;
            o->unget = -2;
            return c;
        }
        fd = o->fd;
    }
    n = __ecall(63, fd, (long)&ch, 1);
    if (n <= 0) {
        if (v >= 4) ((struct RtFile *)f)->eof = 1;
        else __rt_stdin_eof = 1;
        return -1;
    }
    return (int)((unsigned char)ch);
}

int __rt_ungetc(int c, void *f)
{
    long v = (long)f;
    if (c == -1) return -1;
    if (v < 4) { if (v == 0) __rt_stdin_unget = c; }
    else ((struct RtFile *)f)->unget = c;
    return c;
}

int __rt_feof(void *f)
{
    long v = (long)f;
    if (v < 4) return __rt_stdin_eof;
    return (int)((struct RtFile *)f)->eof;
}

int __rt_getchar() { return __rt_fgetc((void *)0); }

char *__rt_fgets(char *buf, int n, void *f)
{
    int i = 0;
    while (i < n - 1) {
        int c = __rt_fgetc(f);
        if (c == -1) {
            if (i == 0) return 0;
            break;
        }
        buf[i] = (char)c;
        i++;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return buf;
}

char *__rt_gets(char *buf)
{
    int i = 0;
    while (1) {
        int c = __rt_fgetc((void *)0);
        if (c == -1) {
            if (i == 0) return 0;
            break;
        }
        if (c == '\n') break;
        buf[i] = (char)c;
        i++;
    }
    buf[i] = 0;
    return buf;
}

void *__rt_fopen(char *name, char *mode)
{
    long flags = 0;
    if (mode[0] == 'w') flags = 1;
    else if (mode[0] == 'a') flags = 9;
    long fd = __ecall(1024, (long)name, flags, 0);
    if (fd < 0) return (void *)0;
    struct RtFile *o = (struct RtFile *)__rt_alloc(32);
    o->fd = fd;
    o->eof = 0;
    o->unget = -2;
    o->mode = flags;
    return (void *)o;
}

int __rt_fclose(void *f)
{
    if ((long)f < 4) return 0;
    __ecall(57, ((struct RtFile *)f)->fd, 0, 0);
    return 0;
}

int __rt_fseek(void *f, long off, int whence)
{
    if ((long)f < 4) return -1;
    struct RtFile *o = (struct RtFile *)f;
    o->eof = 0;
    o->unget = -2;
    __ecall(62, o->fd, off, whence);
    return 0;
}

long __rt_ftell(void *f)
{
    if ((long)f < 4) return -1;
    struct RtFile *o = (struct RtFile *)f;
    long p = __ecall(62, o->fd, 0, 1);
    if (o->unget != -2) p = p - 1;
    return p;
}

void __rt_rewind(void *f) { __rt_fseek(f, 0, 0); }
int __rt_fflush(void *f) { return 0; }
int __rt_remove(char *name) { return 0; }
int __rt_rename(char *a, char *b)
{
    void *src = __rt_fopen(a, "r");
    void *dst;
    int c;
    if (src == 0) return -1;
    dst = __rt_fopen(b, "w");
    if (dst == 0) { __rt_fclose(src); return -1; }
    c = __rt_fgetc(src);
    while (c != -1) { __rt_fputc(c, dst); c = __rt_fgetc(src); }
    __rt_fclose(src);
    __rt_fclose(dst);
    return 0;
}

long __rt_fread(void *p, long size, long n, void *f)
{
    char *d = (char *)p;
    long total = size * n;
    long i = 0;
    while (i < total) {
        int c = __rt_fgetc(f);
        if (c == -1) break;
        d[i] = (char)c;
        i++;
    }
    return size > 0 ? i / size : 0;
}

/* ---------------- output sinks ---------------- */

struct Sink { long kind; long dst; long count; };

void __rt_put(struct Sink *s, int c)
{
    if (s->kind == 0) {
        __ecall(11, c, 0, 0);
    } else if (s->kind == 1) {
        char ch = (char)c;
        __ecall(64, s->dst, (long)&ch, 1);
    } else {
        char *p = (char *)s->dst;
        *p = (char)c;
        s->dst = s->dst + 1;
    }
    s->count = s->count + 1;
}

void __rt_sink_file(struct Sink *s, void *f)
{
    long fd = __rt_fdof(f);
    s->count = 0;
    if (fd <= 2) { s->kind = 0; s->dst = fd; }
    else { s->kind = 1; s->dst = fd; }
}

int __rt_putchar(int c)
{
    __ecall(11, c, 0, 0);
    return c;
}

int __rt_fputc(int c, void *f)
{
    struct Sink s;
    __rt_sink_file(&s, f);
    __rt_put(&s, c);
    return c;
}

int __rt_fputs(char *str, void *f)
{
    struct Sink s;
    __rt_sink_file(&s, f);
    long i = 0;
    while (str[i]) { __rt_put(&s, str[i]); i++; }
    return 0;
}

int __rt_puts(char *str)
{
    long i = 0;
    while (str[i]) { __ecall(11, str[i], 0, 0); i++; }
    __ecall(11, 10, 0, 0);
    return 0;
}

long __rt_fwrite(void *p, long size, long n, void *f)
{
    struct Sink s;
    __rt_sink_file(&s, f);
    char *d = (char *)p;
    long total = size * n;
    long i = 0;
    while (i < total) { __rt_put(&s, d[i]); i++; }
    return n;
}

void __rt_perror(char *msg)
{
    __rt_fputs(msg, (void *)2);
    __rt_fputs(": error\n", (void *)2);
}

/* ---------------- number formatting ---------------- */

void __rt_pad(struct Sink *s, int n, int c)
{
    while (n > 0) { __rt_put(s, c); n--; }
}

void __rt_field(struct Sink *s, char *prefix, char *body, int len, int width, int left, int zero)
{
    int plen = (int)__rt_strlen(prefix);
    int total = plen + len;
    int i;
    if (!left && !zero) __rt_pad(s, width - total, ' ');
    for (i = 0; i < plen; i++) __rt_put(s, prefix[i]);
    if (!left && zero) __rt_pad(s, width - total, '0');
    for (i = 0; i < len; i++) __rt_put(s, body[i]);
    if (left) __rt_pad(s, width - total, ' ');
}

int __rt_utoa(char *out, unsigned long v, int base, int upper)
{
    char tmp[72];
    int n = 0;
    int i;
    if (v == 0) { tmp[0] = '0'; n = 1; }
    while (v > 0) {
        int d = (int)(v % (unsigned long)base);
        if (d < 10) tmp[n] = (char)('0' + d);
        else tmp[n] = (char)((upper ? 'A' : 'a') + d - 10);
        n++;
        v = v / (unsigned long)base;
    }
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
    return n;
}

/* Formats a non-negative double in fixed-point decimal notation with 'prec' fractional digits. */
int __rt_fixed(char *out, double x, int prec)
{
    double r = 0.5;
    int i;
    int len = 0;
    char tmp[40];
    int n = 0;
    for (i = 0; i < prec; i++) r = r / 10.0;
    x = x + r;
    long ip = (long)x;
    double fp = x - (double)ip;
    if (ip == 0) { tmp[0] = '0'; n = 1; }
    while (ip > 0) { tmp[n] = (char)('0' + ip % 10); n++; ip = ip / 10; }
    for (i = 0; i < n; i++) { out[len] = tmp[n - 1 - i]; len++; }
    if (prec > 0) {
        out[len] = '.';
        len++;
        for (i = 0; i < prec; i++) {
            fp = fp * 10.0;
            int d = (int)fp;
            if (d > 9) d = 9;
            out[len] = (char)('0' + d);
            len++;
            fp = fp - (double)d;
        }
    }
    out[len] = 0;
    return len;
}

int __rt_expo(char *out, double x, int prec, int upper)
{
    int e = 0;
    double m = x;
    int len;
    if (m != 0.0) {
        while (m >= 10.0) { m = m / 10.0; e++; }
        while (m < 1.0) { m = m * 10.0; e--; }
    }
    len = __rt_fixed(out, m, prec);
    if (out[0] == '1' && out[1] == '0') {
        m = m / 10.0;
        e++;
        len = __rt_fixed(out, m, prec);
    }
    out[len] = upper ? 'E' : 'e';
    len++;
    if (e < 0) { out[len] = '-'; e = -e; }
    else out[len] = '+';
    len++;
    if (e < 10) { out[len] = '0'; len++; }
    len = len + __rt_utoa(out + len, (unsigned long)e, 10, 0);
    return len;
}

int __rt_strip(char *out, int len)
{
    int i;
    int dot = 0;
    int epos = len;
    for (i = 0; i < len; i++) {
        if (out[i] == '.') dot = 1;
        if (out[i] == 'e' || out[i] == 'E') { epos = i; break; }
    }
    if (!dot) return len;
    int end = epos;
    while (end > 0 && out[end - 1] == '0') end--;
    if (end > 0 && out[end - 1] == '.') end--;
    for (i = epos; i < len; i++) { out[end] = out[i]; end++; }
    out[end] = 0;
    return end;
}

int __rt_dtoa(char *out, double x, char spec, int prec, int alt)
{
    int upper = spec == 'E' || spec == 'G' || spec == 'F';
    if (spec == 'f' || spec == 'F') return __rt_fixed(out, x, prec < 0 ? 6 : prec);
    if (spec == 'e' || spec == 'E') return __rt_expo(out, x, prec < 0 ? 6 : prec, upper);
    int p = prec < 0 ? 6 : (prec == 0 ? 1 : prec);
    int len = __rt_expo(out, x, p - 1, upper);
    int i;
    int x10 = 0;
    for (i = 0; i < len; i++) {
        if (out[i] == 'e' || out[i] == 'E') {
            int sign = out[i + 1] == '-' ? -1 : 1;
            x10 = (int)__rt_strtol(out + i + 2, 0, 10) * sign;
            break;
        }
    }
    if (x10 < -4 || x10 >= p) {
        if (!alt) len = __rt_strip(out, len);
        return len;
    }
    len = __rt_fixed(out, x, p - 1 - x10);
    if (!alt) len = __rt_strip(out, len);
    return len;
}

/* Formats one conversion of a double with sign, padding and flags. */
void __rt_fmt_double(struct Sink *s, double d, char spec, int prec, int width, int left, int zero, int plus, int space, int alt)
{
    char buf[400];
    char *prefix = "";
    int len;
    if (d != d) {
        __rt_field(s, "", "nan", 3, width, left, 0);
        return;
    }
    if (d < 0.0 || (d == 0.0 && 1.0 / d < 0.0)) { prefix = "-"; d = -d; }
    else if (plus) prefix = "+";
    else if (space) prefix = " ";
    if (d > 1.0e300) {
        __rt_field(s, prefix, "inf", 3, width, left, 0);
        return;
    }
    len = __rt_dtoa(buf, d, spec, prec, alt);
    __rt_field(s, prefix, buf, len, width, left, zero);
}

/* ---------------- printf engine ---------------- */

/* Walks the format string; arguments are read from consecutive 8-byte slots. */
int __rt_format(struct Sink *s, char *fmt, char *ap)
{
    while (*fmt) {
        if (*fmt != '%') { __rt_put(s, *fmt); fmt++; continue; }
        fmt++;
        int left = 0;
        int zero = 0;
        int plus = 0;
        int space = 0;
        int alt = 0;
        int width = 0;
        int prec = -1;
        int lng = 0;
        int more = 1;
        while (more) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '#') alt = 1;
            else more = 0;
            if (more) fmt++;
        }
        if (*fmt == '*') {
            width = *(int *)ap;
            ap = ap + 8;
            fmt++;
            if (width < 0) { left = 1; width = -width; }
        } else {
            while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        }
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') { prec = *(int *)ap; ap = ap + 8; fmt++; }
            else {
                while (*fmt >= '0' && *fmt <= '9') { prec = prec * 10 + (*fmt - '0'); fmt++; }
            }
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z' || *fmt == 'j' || *fmt == 't' || *fmt == 'L') {
            if (*fmt == 'l' || *fmt == 'z' || *fmt == 'j' || *fmt == 't') lng = 1;
            fmt++;
        }
        char c = *fmt;
        if (c == 0) break;
        fmt++;
        if (c == '%') { __rt_put(s, '%'); continue; }
        if (c == 'd' || c == 'i') {
            long v;
            char buf[80];
            char *prefix = "";
            int len;
            if (lng) v = *(long *)ap; else v = (long)(*(int *)ap);
            ap = ap + 8;
            if (v < 0) { prefix = "-"; len = __rt_utoa(buf, (unsigned long)(-v), 10, 0); }
            else { if (plus) prefix = "+"; else if (space) prefix = " "; len = __rt_utoa(buf, (unsigned long)v, 10, 0); }
            if (prec >= 0 && len < prec) {
                char z[80];
                int k;
                for (k = 0; k < prec - len; k++) z[k] = '0';
                for (k = 0; k < len; k++) z[prec - len + k] = buf[k];
                z[prec] = 0;
                __rt_field(s, prefix, z, prec, width, left, 0);
            } else __rt_field(s, prefix, buf, len, width, left, zero && prec < 0);
        } else if (c == 'u' || c == 'x' || c == 'X' || c == 'o') {
            unsigned long v;
            char buf[80];
            char *prefix = "";
            int len;
            int base = 10;
            if (c == 'x' || c == 'X') base = 16;
            if (c == 'o') base = 8;
            if (lng) v = *(unsigned long *)ap; else v = (unsigned long)(*(unsigned int *)ap);
            ap = ap + 8;
            len = __rt_utoa(buf, v, base, c == 'X');
            if (alt && v != 0 && base == 16) prefix = c == 'X' ? "0X" : "0x";
            if (prec >= 0 && len < prec) {
                char z[80];
                int k;
                for (k = 0; k < prec - len; k++) z[k] = '0';
                for (k = 0; k < len; k++) z[prec - len + k] = buf[k];
                z[prec] = 0;
                __rt_field(s, prefix, z, prec, width, left, 0);
            } else __rt_field(s, prefix, buf, len, width, left, zero && prec < 0);
        } else if (c == 'c') {
            char b[2];
            b[0] = (char)(*(int *)ap);
            b[1] = 0;
            ap = ap + 8;
            __rt_field(s, "", b, 1, width, left, 0);
        } else if (c == 's') {
            char *p = *(char **)ap;
            long len;
            ap = ap + 8;
            if (p == 0) p = "(null)";
            len = __rt_strlen(p);
            if (prec >= 0 && len > prec) len = prec;
            __rt_field(s, "", p, (int)len, width, left, 0);
        } else if (c == 'p') {
            char buf[40];
            long v = *(long *)ap;
            ap = ap + 8;
            int len = __rt_utoa(buf, (unsigned long)v, 16, 0);
            __rt_field(s, "0x", buf, len, width, left, 0);
        } else if (c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G') {
            double d = *(double *)ap;
            ap = ap + 8;
            __rt_fmt_double(s, d, c, prec, width, left, zero, plus, space, alt);
        }
    }
    return (int)s->count;
}

int __rt_printf(char *fmt, ...)
{
    va_list ap;
    struct Sink s;
    va_start(ap, fmt);
    s.kind = 0;
    s.dst = 1;
    s.count = 0;
    int n = __rt_format(&s, fmt, ap);
    va_end(ap);
    return n;
}

int __rt_fprintf(void *f, char *fmt, ...)
{
    va_list ap;
    struct Sink s;
    va_start(ap, fmt);
    __rt_sink_file(&s, f);
    int n = __rt_format(&s, fmt, ap);
    va_end(ap);
    return n;
}

int __rt_sprintf(char *buf, char *fmt, ...)
{
    va_list ap;
    struct Sink s;
    va_start(ap, fmt);
    s.kind = 2;
    s.dst = (long)buf;
    s.count = 0;
    int n = __rt_format(&s, fmt, ap);
    *(char *)s.dst = 0;
    va_end(ap);
    return n;
}

/* ---------------- scanf engine ---------------- */

struct Src { long kind; void *f; char *s; long pos; long pushed; };

int __rt_sgetc(struct Src *s)
{
    if (s->pushed != -2) {
        int c = (int)s->pushed;
        s->pushed = -2;
        return c;
    }
    if (s->kind == 0) return __rt_fgetc(s->f);
    int c = s->s[s->pos];
    if (c == 0) return -1;
    s->pos = s->pos + 1;
    return (int)((unsigned char)c);
}

void __rt_sunget(struct Src *s, int c)
{
    if (c != -1) s->pushed = c;
}

void __rt_sskip(struct Src *s)
{
    int c = __rt_sgetc(s);
    while (c != -1 && __rt_isspace(c)) c = __rt_sgetc(s);
    __rt_sunget(s, c);
}

int __rt_scan_int(struct Src *s, int base, long *out, int width)
{
    long v = 0;
    long sign = 1;
    int got = 0;
    int c = __rt_sgetc(s);
    if (c == '-') { sign = -1; c = __rt_sgetc(s); width--; }
    else if (c == '+') { c = __rt_sgetc(s); width--; }
    while (c != -1 && width != 0) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        got = 1;
        width--;
        c = __rt_sgetc(s);
    }
    __rt_sunget(s, c);
    *out = sign * v;
    return got;
}

int __rt_scan_double(struct Src *s, double *out)
{
    char buf[64];
    int n = 0;
    int c = __rt_sgetc(s);
    int got = 0;
    if (c == '-' || c == '+') { buf[n] = (char)c; n++; c = __rt_sgetc(s); }
    while (c != -1 && n < 60 && (__rt_isdigit(c) || c == '.' || c == 'e' || c == 'E' ||
           ((c == '-' || c == '+') && (buf[n - 1] == 'e' || buf[n - 1] == 'E')))) {
        if (__rt_isdigit(c)) got = 1;
        buf[n] = (char)c;
        n++;
        c = __rt_sgetc(s);
    }
    __rt_sunget(s, c);
    buf[n] = 0;
    *out = __rt_atof(buf);
    return got;
}

/* Walks the scanf format; returns the number of assigned items or -1 at end of input. */
int __rt_scan(struct Src *s, char *fmt, char *ap)
{
    int count = 0;
    while (*fmt) {
        if (__rt_isspace(*fmt)) { __rt_sskip(s); fmt++; continue; }
        if (*fmt != '%') {
            int c = __rt_sgetc(s);
            if (c != *fmt) {
                __rt_sunget(s, c);
                if (c == -1 && count == 0) return -1;
                return count;
            }
            fmt++;
            continue;
        }
        fmt++;
        int suppress = 0;
        int width = -1;
        int lng = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }
        if (*fmt >= '0' && *fmt <= '9') {
            width = 0;
            while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'L' || *fmt == 'z') {
            if (*fmt == 'l' || *fmt == 'z') lng = 1;
            fmt++;
        }
        char spec = *fmt;
        if (spec == 0) break;
        fmt++;
        if (spec == '%') { __rt_sskip(s); int c = __rt_sgetc(s); if (c != '%') { __rt_sunget(s, c); return count; } continue; }
        if (spec != 'c') __rt_sskip(s);
        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
            long v;
            int base = 10;
            if (spec == 'x' || spec == 'X') base = 16;
            if (spec == 'o') base = 8;
            if (!__rt_scan_int(s, base, &v, width)) {
                int c = __rt_sgetc(s);
                if (c == -1 && count == 0) return -1;
                __rt_sunget(s, c);
                return count;
            }
            if (!suppress) {
                char *p = *(char **)ap;
                ap = ap + 8;
                if (lng) *(long *)p = v; else *(int *)p = (int)v;
                count++;
            }
        } else if (spec == 'f' || spec == 'e' || spec == 'g' || spec == 'E' || spec == 'G') {
            double d;
            if (!__rt_scan_double(s, &d)) {
                int c = __rt_sgetc(s);
                if (c == -1 && count == 0) return -1;
                __rt_sunget(s, c);
                return count;
            }
            if (!suppress) {
                char *p = *(char **)ap;
                ap = ap + 8;
                if (lng) *(double *)p = d; else *(float *)p = (float)d;
                count++;
            }
        } else if (spec == 'c') {
            int c = __rt_sgetc(s);
            if (c == -1) return count == 0 ? -1 : count;
            if (!suppress) {
                char *p = *(char **)ap;
                ap = ap + 8;
                *p = (char)c;
                count++;
            }
        } else if (spec == 's') {
            char *p = 0;
            long n = 0;
            int c;
            if (!suppress) { p = *(char **)ap; ap = ap + 8; }
            c = __rt_sgetc(s);
            if (c == -1) return count == 0 ? -1 : count;
            while (c != -1 && !__rt_isspace(c) && width != 0) {
                if (p) p[n] = (char)c;
                n++;
                width--;
                c = __rt_sgetc(s);
            }
            __rt_sunget(s, c);
            if (p) { p[n] = 0; count++; }
        }
    }
    return count;
}

int __rt_finish_src(struct Src *s, int r)
{
    if (s->kind == 0) {
        if (s->pushed != -2) __rt_ungetc((int)s->pushed, s->f);
    }
    return r;
}

int __rt_scanf(char *fmt, ...)
{
    va_list ap;
    struct Src s;
    va_start(ap, fmt);
    s.kind = 0;
    s.f = (void *)0;
    s.pushed = -2;
    int r = __rt_scan(&s, fmt, ap);
    va_end(ap);
    return __rt_finish_src(&s, r);
}

int __rt_fscanf(void *f, char *fmt, ...)
{
    va_list ap;
    struct Src s;
    va_start(ap, fmt);
    s.kind = 0;
    s.f = f;
    s.pushed = -2;
    int r = __rt_scan(&s, fmt, ap);
    va_end(ap);
    return __rt_finish_src(&s, r);
}

int __rt_sscanf(char *str, char *fmt, ...)
{
    va_list ap;
    struct Src s;
    va_start(ap, fmt);
    s.kind = 2;
    s.s = str;
    s.pos = 0;
    s.pushed = -2;
    int r = __rt_scan(&s, fmt, ap);
    va_end(ap);
    return r;
}

/* ---------------- stream helpers (cout / cin) ---------------- */

void __rt___print_int(int v)
{
    char buf[24];
    struct Sink s;
    s.kind = 0; s.dst = 1; s.count = 0;
    if (v < 0) { __rt_put(&s, '-'); __rt_utoa(buf, (unsigned long)(-(long)v), 10, 0); }
    else __rt_utoa(buf, (unsigned long)v, 10, 0);
    long i = 0;
    while (buf[i]) { __rt_put(&s, buf[i]); i++; }
}

void __rt___print_long(long v)
{
    char buf[40];
    struct Sink s;
    s.kind = 0; s.dst = 1; s.count = 0;
    if (v < 0) { __rt_put(&s, '-'); __rt_utoa(buf, (unsigned long)(-v), 10, 0); }
    else __rt_utoa(buf, (unsigned long)v, 10, 0);
    long i = 0;
    while (buf[i]) { __rt_put(&s, buf[i]); i++; }
}

void __rt___print_char(char c) { __ecall(11, c, 0, 0); }

void __rt___print_str(char *p)
{
    long i = 0;
    while (p[i]) { __ecall(11, p[i], 0, 0); i++; }
}

void __rt___print_bool(int b) { __ecall(11, b ? '1' : '0', 0, 0); }

void __rt___print_ptr(void *p)
{
    char buf[40];
    long i = 0;
    __rt___print_str("0x");
    __rt_utoa(buf, (unsigned long)p, 16, 0);
    while (buf[i]) { __ecall(11, buf[i], 0, 0); i++; }
}

void __rt___print_double(double d)
{
    struct Sink s;
    s.kind = 0; s.dst = 1; s.count = 0;
    __rt_fmt_double(&s, d, 'g', -1, 0, 0, 0, 0, 0, 0);
}

void __rt___print_endl() { __ecall(11, 10, 0, 0); }

void __rt___read_int(int *p)
{
    struct Src s;
    long v = 0;
    s.kind = 0; s.f = (void *)0; s.pushed = -2;
    __rt_sskip(&s);
    __rt_scan_int(&s, 10, &v, -1);
    *p = (int)v;
    __rt_finish_src(&s, 0);
}

void __rt___read_long(long *p)
{
    struct Src s;
    long v = 0;
    s.kind = 0; s.f = (void *)0; s.pushed = -2;
    __rt_sskip(&s);
    __rt_scan_int(&s, 10, &v, -1);
    *p = v;
    __rt_finish_src(&s, 0);
}

void __rt___read_char(char *p)
{
    struct Src s;
    s.kind = 0; s.f = (void *)0; s.pushed = -2;
    __rt_sskip(&s);
    int c = __rt_sgetc(&s);
    if (c != -1) *p = (char)c;
    __rt_finish_src(&s, 0);
}

void __rt___read_double(double *p)
{
    struct Src s;
    double d = 0.0;
    s.kind = 0; s.f = (void *)0; s.pushed = -2;
    __rt_sskip(&s);
    __rt_scan_double(&s, &d);
    *p = d;
    __rt_finish_src(&s, 0);
}

void __rt___read_str(char *p)
{
    struct Src s;
    long n = 0;
    s.kind = 0; s.f = (void *)0; s.pushed = -2;
    __rt_sskip(&s);
    int c = __rt_sgetc(&s);
    while (c != -1 && !__rt_isspace(c)) { p[n] = (char)c; n++; c = __rt_sgetc(&s); }
    __rt_sunget(&s, c);
    p[n] = 0;
    __rt_finish_src(&s, 0);
}
