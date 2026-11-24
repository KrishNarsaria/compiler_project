// Operator precedence torture: assignment chains (right assoc), ternary
// nesting, comma sequencing, unary stacking, mixed shifts and bitwise,
// pointer/member chains, casts inside expressions, and sizeof in arithmetic.
#include <stdio.h>
#include <string.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local int a
// EXPECT-IR: local int b
// EXPECT-IR: local int c
// EXPECT-IR: a = 0
// EXPECT-IR: b = 0
// EXPECT-IR: c = 0
// EXPECT-IR: a = 1
// EXPECT-IR: b = 1
// EXPECT-IR: c = 1
// EXPECT-IR: local int t
// EXPECT-IR: t = 1
// EXPECT-IR: t1 = t * 2
// EXPECT-IR: t = t1
// EXPECT-IR: t = t + t1
// EXPECT-IR: t = t - 1
// EXPECT-IR: local int q
// EXPECT-IR: q = t4
// EXPECT-IR: local int r
// EXPECT-IR: r = t5
// EXPECT-IR: local int s
// EXPECT-IR: a = a + 1
// EXPECT-IR: b = b + 1
// EXPECT-IR: s = t10 + c
// EXPECT-IR: local int u1
// EXPECT-IR: t12 = -a
// EXPECT-IR: u1 = -t12
// EXPECT-IR: local int u2
// EXPECT-IR: t14 = ~a
// EXPECT-IR: u2 = ~t14
// EXPECT-IR: local int u3
// EXPECT-IR: t16 = !a
// EXPECT-IR: u3 = !t17
// EXPECT-IR: local int u4
// EXPECT-IR: u4 = a
// EXPECT-IR: local int e
// EXPECT-IR: t19 = 1 << a
// EXPECT-IR: t20 = t19 + 3
// EXPECT-IR: t21 = t20 >> 2
// EXPECT-IR: e = t21 | 7
// EXPECT-IR: local int g
// EXPECT-IR: t23 = b & c
// EXPECT-IR: g = a ^ t23
// EXPECT-IR: local int h
// EXPECT-IR: t25 = a << 2
// EXPECT-IR: t26 = 2 + t25
// EXPECT-IR: h = a << t26
// EXPECT-IR: local struct Pt pts[2]
// EXPECT-IR: t28->x = 3
// EXPECT-IR: t29->x = 9
// EXPECT-IR: local int m
// EXPECT-IR: t30 = a + 8
// EXPECT-IR: m = t30 * 3
// EXPECT-IR: local double dd
// EXPECT-IR: t32 = (double) a
// EXPECT-IR: dd = t32 + 0.5
// EXPECT-IR: local int y
// EXPECT-IR: t34 = x
// EXPECT-IR: x = x + 1
// EXPECT-IR: y = t34 + t36

struct Pt { int x, y; };

int main()
{
    int a, b, c, x = 5;
    a = b = c = 0;                       /* right-assoc assignment */
    c = b = a = 1;
    int t = 1;
    t += t *= 2;                         /* compound on compound */
    t -= 1;
    int q = a > b ? a : b;
    int r = a > b ? (a > c ? a : c) : (b > c ? b : c);
    int s = (a = a + 1, b = b + 1, a + b + c);   /* comma seq */
    int u1 = -(-a);                      /* stacked unary */
    int u2 = ~(~a);
    int u3 = !(!!a);
    int u4 = +(+a);
    int e = (1 << a) + 3 >> 2 | 7;       /* shift vs additive vs bitwise */
    int g = a ^ b & c;                   /* & binds tighter than ^ */
    int h = a << 2 + (a << 2);           /* + binds tighter than << */
    struct Pt pts[2];
    pts[0].x = 3;
    pts[1].x = 9;
    int m = (a + (int)sizeof(struct Pt)) * 3;
    double dd = (double)a + 0.5;
    int y = x++ + ++x;                   /* sequencing of increments */
    printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %f %d\n",
           a, b, c, t, q, r, s, u1, u2, u3, u4, e, g, h, m, dd, y);
    return 0;
}
