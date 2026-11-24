// Constant folding: every constant expression must be evaluated at compile
// time and appear as a single folded value in the IR.
#include <stdio.h>

// EXPECT-IR: func main() : int

int main()
{
    /* arithmetic */
    int a1 = 1 + 2 * 3 - 4 / 2 + 7 % 4;       // EXPECT-IR: a1 = 8
    int a2 = (1 + 2) * (3 + 4);                // EXPECT-IR: a2 = 21
    int a3 = -(-5);                            // EXPECT-IR: a3 = 5
    int a4 = 100 / 7;                          // EXPECT-IR: a4 = 14
    int a5 = 100 % 7;                          // EXPECT-IR: a5 = 2
    int a6 = 2 * 2 * 2 * 2 * 2 * 2 * 2;        // EXPECT-IR: a6 = 128
    int a7 = 1 - 2 - 3 - 4;                    // EXPECT-IR: a7 = -8

    /* bitwise and shifts */
    int b1 = 1 << 10;                          // EXPECT-IR: b1 = 1024
    int b2 = 4096 >> 4;                        // EXPECT-IR: b2 = 256
    int b3 = (1 << 4) | (1 << 2) | 1;          // EXPECT-IR: b3 = 21
    int b4 = 0xFF & 0x0F;                      // EXPECT-IR: b4 = 15
    int b5 = 0x0F ^ 0x33;                      // EXPECT-IR: b5 = 60
    int b6 = ~0;                               // EXPECT-IR: b6 = -1
    int b7 = ~(-1);                            // EXPECT-IR: b7 = 0

    /* comparisons and logic */
    int c1 = 3 > 2 && 4 >= 4 && 5 <= 9;        // EXPECT-IR: c1 = 1
    int c2 = 1 < 0 || 2 == 3 || 4 != 5;        // EXPECT-IR: c2 = 1
    int c3 = !(1 && 0);                        // EXPECT-IR: c3 = 1
    int c4 = !!(42);                           // EXPECT-IR: c4 = 1
    int c5 = 1 ? 100 : 200;                    // EXPECT-IR: c5 = 100
    int c6 = 0 ? 100 : 200;                    // EXPECT-IR: c6 = 200
    int c7 = (3 > 2) ? (1 ? 7 : 8) : 9;        // EXPECT-IR: c7 = 7

    /* casts and conversions */
    int d1 = (int)3.9;                         // EXPECT-IR: d1 = 3
    int d2 = (int)3.1 + (int)4.8;              // EXPECT-IR: d2 = 7
    int d3 = (int)(2.5 * 2);                   // EXPECT-IR: d3 = 5
    double e1 = 1 / 2;                         /* integer division folds to 0 */
    // EXPECT-IR: e1 = 0
    double e2 = 1.0 / 2;                       // EXPECT-IR: e2 = 0.5
    double e3 = 2.5 + 1;                       // EXPECT-IR: e3 = 3.5
    double e4 = (double)5 / 2;                 // EXPECT-IR: e4 = 2.5

    /* char arithmetic */
    int f1 = 'B' - 'A';                        // EXPECT-IR: f1 = 1
    char f2 = 'A' + 1;                         // EXPECT-IR: f2 = 66
    int f3 = '0' + 7;                          // EXPECT-IR: f3 = 55

    /* sizeof is folded */
    int g1 = sizeof(int);                      // EXPECT-IR: g1 = 4
    int g2 = sizeof(double);                   // EXPECT-IR: g2 = 8
    int g3 = sizeof(char);                     // EXPECT-IR: g3 = 1

    printf("%d %d %d %d %d %d %d\n", a1, a2, a3, a4, a5, a6, a7);
    printf("%d %d %d %d %d %d %d\n", b1, b2, b3, b4, b5, b6, b7);
    printf("%d %d %d %d %d %d %d\n", c1, c2, c3, c4, c5, c6, c7);
    printf("%d %d %d %f %f %f %f\n", d1, d2, d3, e1, e2, e3, e4);
    printf("%d %d %d\n", f1, f2, f3);
    printf("%d %d %d\n", g1, g2, g3);
    return 0;
}
