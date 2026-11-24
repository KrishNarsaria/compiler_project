// Type system depth: bool/short/long/unsigned arithmetic, implicit int<->
// double conversions, explicit casts, unsigned wraparound, mixed-type
// comparisons, overload resolution via conversion rank, sizeof on types,
// expressions and variables.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func pick_int(int v) : int
// EXPECT-IR: func pick_double(double v) : double
// EXPECT-IR: local bool flag
// EXPECT-IR: flag = 1
// EXPECT-IR: local short s
// EXPECT-IR: local long big
// EXPECT-IR: local unsigned int u
// EXPECT-IR: u = 4000000000
// EXPECT-IR: local int small
// EXPECT-IR: small = (int) u
// EXPECT-IR: local double d
// EXPECT-IR: d = 3.5
// EXPECT-IR: local int t2_v
// EXPECT-IR: t2_v = (int) d
// EXPECT-IR: local char c2
// EXPECT-IR: c2 = 44
// EXPECT-IR: local double d2
// EXPECT-IR: d2 = (double) s
// EXPECT-IR: local int n1
// EXPECT-IR: n1 = 4
// EXPECT-IR: n2 = 8
// EXPECT-IR: n3 = 8
// EXPECT-IR: n4 = 1
// EXPECT-IR: local double mix
// EXPECT-IR: mix = t8 + d
// EXPECT-IR: local int cmp
// EXPECT-IR: if flag != 1 goto
// EXPECT-IR: ifFalse flag goto
// EXPECT-IR: call pick_int, 1
// EXPECT-IR: call pick_double, 1

int pick(int v) { return v + 1; }
double pick(double v) { return v + 0.5; }

int main()
{
    bool flag = true;
    short s = -3;
    long big = 100000;
    unsigned int u = 4000000000U;
    int small = (int)u;                    /* wraps to negative */
    double d = 3.5;
    int t2 = (int)d;                       /* truncate to 3 */
    char c2 = (char)300;                   /* truncate to 44 */
    double d2 = (double)s;                 /* -3.0 */
    unsigned int u2 = u + 1U;              /* 4000000001 */
    int neg = (int)u2;
    (void)neg;

    int n1 = sizeof(int);
    int n2 = sizeof(double);
    int n3 = sizeof(char *);
    int n4 = sizeof(flag);

    int i = 7;
    double mix = i + d;                    /* int promoted to double */
    double mix2 = i / 2 + 0.5;             /* integer division then promote */
    double mix3 = (double)i / 2;           /* float division */
    int cmp = (mix > mix2) && (mix3 < 4.0);
    if (flag == true)
        cmp = cmp + 1;
    if (flag)
        cmp = cmp + 2;
    while (u > 3000000000U)                /* unsigned comparison */
        u = u - 1000000000U;

    int a = pick(5);                       /* int overload */
    double b = pick(2.5);                  /* double overload */
    double c = pick(3);                    /* int arg -> which? int overload */
    double e = pick('x');                  /* char promotes to int */

    printf("%d %d %d %d %d %f %f %f %d %d %d %f %f\n",
           small, t2, c2, n1 + n2 + n3 + n4, cmp, mix, mix2, mix3,
           a, (int)u, i, b, c + e);
    printf("%ld %d\n", big, (int)s);
    return 0;
}
