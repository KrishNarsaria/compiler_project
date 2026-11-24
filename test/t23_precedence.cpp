// Operator precedence and associativity torture test. Every one of these
// must compile and the IR must fold to the value shown in the comment.
#include <stdio.h>

// EXPECT-IR: func main() : int

int main()
{
    int a = 2, b = 3, c = 4;

    /* assignment is right-associative */
    int r1 = (a = b = c = 9);        // EXPECT-IR: r1 = 9

    /* arithmetic precedence: * / % bind tighter than + - */
    int r2 = 2 + 3 * 4;              // EXPECT-IR: r2 = 14
    int r3 = 10 - 4 / 2;             // EXPECT-IR: r3 = 8
    int r4 = 7 + 9 % 4;              // EXPECT-IR: r4 = 8
    int r5 = 2 * 3 + 4 * 5;          // EXPECT-IR: r5 = 26

    /* shifts bind tighter than comparisons but looser than + - */
    int r6 = 1 << 2 + 3;             // EXPECT-IR: r6 = 32
    int r7 = 16 >> 1 + 1;            // EXPECT-IR: r7 = 4
    int r8 = (1 << 3) - 1;           // EXPECT-IR: r8 = 7

    /* relational binds tighter than &&, which binds tighter than || */
    int r9 = 1 < 2 && 3 < 4 || 0;    // EXPECT-IR: r9 = 1
    int r10 = 1 || 0 && 0;           // EXPECT-IR: r10 = 1

    /* bitwise: & before ^ before |; comparisons looser than & */
    int r11 = 7 | 2 & 1;             // EXPECT-IR: r11 = 7
    int r12 = 7 ^ 3 & 1;             // EXPECT-IR: r12 = 6
    int r13 = 1 | 0 == 1;            // EXPECT-IR: r13 = 1
    int r14 = 1 & 3 == 3;            // EXPECT-IR: r14 = 1

    /* unary operators bind tighter than binary; postfix tighter than prefix */
    int x = 5;
    int r15 = -x + 10;               // EXPECT-IR: t1 = -x
    int r16 = !0 + 1;                // EXPECT-IR: r16 = 2
    int r17 = - -x;                  // EXPECT-IR: r17 = -t
    int r18 = ~x + 6;                // EXPECT-IR: t5 = ~x

    /* ternary is right-associative, comma is lowest */
    int r19 = 0 ? 1 : 0 ? 2 : 3;     // EXPECT-IR: r19 = 3
    int r20 = (1, 2, 3) + 4;         // EXPECT-IR: r20 = 7

    /* casts bind like unary operators */
    int r21 = (int)2.9 + 5;          // EXPECT-IR: r21 = 7
    int r22 = (int)(2.9 + 0.1);      // EXPECT-IR: r22 = 3

    /* mixed chains */
    int r23 = a + b * c + a;         // EXPECT-IR: t8 = a + t7
    int r24 = a << b + c;            // EXPECT-IR: r24 = a << t10
    int r25 = (a + b) * (c - 1);     // EXPECT-IR: r25 = t12 * t13
    int r26 = a * b % c;             // EXPECT-IR: r26 = t15 % c

    printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
           r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13,
           r14, r15, r16, r17, r18, r19, r20, r21, r22, r23, r24, r25, r26);
    return 0;
}
