// Every arithmetic, logical, bitwise and comparison operator, compound
// assignments, unary operators, ternary, comma - with constant folding
// verified through the generated IR.
#include <stdio.h>

// EXPECT-IR: func main() : int

int main()
{
    int a = 17, b = 5;
    double d = 2.5;

    /* ---- constant folding: each folded value must appear verbatim ---- */
    int c1 = 2 + 3 * 4;          // EXPECT-IR: c1 = 14
    int c2 = (2 + 3) << 1;       // EXPECT-IR: c2 = 10
    int c3 = 17 / 5;             // EXPECT-IR: c3 = 3
    int c4 = 17 % 5;             // EXPECT-IR: c4 = 2
    int c5 = 5 & 3;              // EXPECT-IR: c5 = 1
    int c6 = 5 | 2;              // EXPECT-IR: c6 = 7
    int c7 = 5 ^ 1;              // EXPECT-IR: c7 = 4
    int c8 = ~0;                 // EXPECT-IR: c8 = -1
    int c9 = !0;                 // EXPECT-IR: c9 = 1
    int c10 = -7;                // EXPECT-IR: c10 = -7
    int c11 = 256 >> 3;          // EXPECT-IR: c11 = 32
    int c12 = 1 << 10;           // EXPECT-IR: c12 = 1024
    int c13 = 3 > 2 && 2 < 4;    // EXPECT-IR: c13 = 1
    int c14 = 3 == 4 || 3 != 4;  // EXPECT-IR: c14 = 1
    int c15 = 10 % 3 * 2;        // EXPECT-IR: c15 = 2
    double cd1 = 5.0 / 2;        // EXPECT-IR: cd1 = 2.5
    double cd2 = 2.5 * 4;        // EXPECT-IR: cd2 = 10
    int c16 = (int)3.9;          // EXPECT-IR: c16 = 3
    double cd3 = (double)5;      // EXPECT-IR: cd3 = 5
    char cc = 'A' + 1;           // EXPECT-IR: cc = 66
    int c17 = 1 ? 10 : 20;       // EXPECT-IR: c17 = 10

    /* ---- arithmetic / relational / logical with variables ---- */
    int r1 = a + b * 2 - a / b + a % b;
    int r2 = (a & b) | (a ^ b) << 1;
    int r3 = (a > b && b > 0) || !(a == b);
    double r4 = d * 2 + 1;
    // EXPECT-IR: local int r1
    // EXPECT-IR: r2 = t

    /* ---- compound assignments (every one of them) ---- */
    int x = 10;
    x += 5;                      // EXPECT-IR: x = x + 5
    x -= 3;                      // EXPECT-IR: x = x - 3
    x *= 2;                      // EXPECT-IR: x = x * 2
    x /= 4;                      // EXPECT-IR: x = x / 4
    x %= 3;                      // EXPECT-IR: x = x % 3
    x &= 6;                      // EXPECT-IR: x = x & 6
    x |= 9;                      // EXPECT-IR: x = x | 9
    x ^= 1;                      // EXPECT-IR: x = x ^ 1
    x <<= 2;                     // EXPECT-IR: x = x << 2
    x >>= 1;                     // EXPECT-IR: x = x >> 1

    /* ---- unary +/-, !, ~ ---- */
    int u1 = -b;                 // EXPECT-IR: u1 = -b
    int u2 = !b;                 // EXPECT-IR: u2 = !b
    int u3 = ~b;                 // EXPECT-IR: u3 = ~b
    int u4 = +b;                 // EXPECT-IR: u4 = b

    /* ---- increment / decrement, prefix and postfix ---- */
    int y = 1;
    y++;                         // EXPECT-IR: y = y + 1
    y--;                         // EXPECT-IR: y = y - 1
    ++y;                         // EXPECT-IR: y = y + 1
    --y;                         // EXPECT-IR: y = y - 1
    int snap = y++;              // snapshot keeps the old value
    // EXPECT-IR: snap = t

    /* ---- ternary and comma ---- */
    int z = (a = 1, b = 2, a + b);
    // EXPECT-IR: a = 1
    // EXPECT-IR: b = 2
    // EXPECT-IR: z = a + b

    /* a variable named like a generated temp must be mangled, never clash */
    int t2 = a > b ? a : b;      // EXPECT-IR: local int t2_v

    /* ---- pointer arithmetic operators ---- */
    int arr[5] = {0, 1, 2, 3, 4};
    int *p = &arr[1];
    // EXPECT-IR: p = &arr[4]
    int *q = p + 2;              // EXPECT-IR: q = p + 8
    int *back = q - 1;           // EXPECT-IR: back = q - 4
    int diff = q - p;            // pointer difference: scaled by element size
    // EXPECT-IR: diff = (int) t
    // EXPECT-IR: / 4

    printf("%d %d %d %f %d %d %d %d %d\n", r1, r2, r3, r4, x, u1, snap, t2, diff);
    // EXPECT-IR: call printf, 10
    return 0;
}
