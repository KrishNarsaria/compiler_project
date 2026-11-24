// Control flow: if / else-if chains, while, do-while, for variants,
// switch with fallthrough and default, break / continue in nested loops,
// goto + labels, and the custom until / do-until loops.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func classify(int x) : int
// EXPECT-IR: func switchy(int x) : int
// EXPECT-IR: if x == 1 goto
// EXPECT-IR: L_loop:

int classify(int x)
{
    if (x < 0) return -1;
    else if (x == 0) return 0;
    else if (x < 10) return 1;
    else return 2;
}

int sumWhile(int n)
{
    int s = 0, i = 0;
    while (i < n) {
        s += i;
        i++;
    }
    return s;
}

int sumDoWhile(int n)
{
    int s = 0, i = 0;
    do {
        s += i;
        i++;
    } while (i < n);
    return s;
}

int sumFor(int n)
{
    int s = 0, i;
    for (i = 1; i <= n; i++)
        s += i;
    return s;
}

int sumUntil(int n)
{
    int s = 0, i = 0;
    until (i >= n) {          /* runs while the condition is false */
        s += i;
        i++;
    }
    return s;
}

int sumDoUntil(int n)
{
    int s = 0, i = 0;
    do {
        s += i;
        i++;
    } until (i >= n);         /* runs at least once, exits when true */
    return s;
}

int oddSum(int n)
{
    int s = 0, i;
    for (i = 0; i < n; i++) {
        if (i % 2 == 0) continue;
        s += i;
    }
    return s;
}

int withBreak(int n)
{
    int i, s = 0;
    for (i = 0; ; i++) {      /* empty condition: infinite for-loop */
        if (i >= n) break;
        if (i == 7) continue;
        s += i;
    }
    return s;
}

int nested(int n)
{
    int i, j, hits = 0;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (i == j) continue;
            if (i + j == 8) break;
            hits++;
        }
    }
    return hits;
}

int switchy(int x)
{
    int r = 0;
    switch (x) {
    case 0:
        r = 10;
        break;
    case 1:
    case 2:                    /* fallthrough */
        r = 20;
        break;
    case 3:
        r = 30;                /* deliberate fallthrough */
    case 4:
        r += 5;
        break;
    default:
        r = -1;
        break;
    }
    return r;
}

int switchDefaultFirst(int x)
{
    switch (x) {
    default:
        return 99;
    case 1:
        return 1;
    case 2:
        return 2;
    }
}

int gotoSum(int n)
{
    int s = 0, i = 0;
loop:
    if (i >= n) goto done;
    s += i;
    i++;
    goto loop;
done:
    return s;
}

int labelled(int n)
{
    int s = 0;
    while (1) {
        s += n;
        if (s > 100) goto out;
        n *= 2;
    }
out:
    return s;
}

int emptyLoops(int n)
{
    int i = 0;
    for (;;) {
        i++;
        if (i >= n) break;
    }
    while (i < 2 * n) i++;
    return i;
}

int condExpr(int x)
{
    return x % 2 == 0 ? x / 2 : x * 3 + 1;
}

int main()
{
    printf("%d\n", classify(-5));        // -1
    printf("%d\n", classify(0));         // 0
    printf("%d\n", classify(7));         // 1
    printf("%d\n", classify(100));       // 2
    printf("%d\n", sumWhile(10));        // 45
    printf("%d\n", sumDoWhile(10));      // 45
    printf("%d\n", sumFor(10));          // 55
    printf("%d\n", sumUntil(10));        // 45
    printf("%d\n", sumDoUntil(10));      // 45
    printf("%d\n", oddSum(10));          // 25
    printf("%d\n", withBreak(10));       // 45 - 7 = 38
    printf("%d\n", nested(5));           // 16
    printf("%d\n", switchy(0));          // 10
    printf("%d\n", switchy(2));          // 20
    printf("%d\n", switchy(3));          // 35
    printf("%d\n", switchy(9));          // -1
    printf("%d\n", switchDefaultFirst(1)); // 1
    printf("%d\n", gotoSum(10));         // 45
    printf("%d\n", labelled(1));         // 1+2+4+...+128 until >100
    printf("%d\n", emptyLoops(5));       // 10
    printf("%d\n", condExpr(6));         // 3
    return 0;
}
