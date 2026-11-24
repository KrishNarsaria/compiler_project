// Recursion stress: mutual recursion, ackermann, memoised fibonacci, recursive
// array sum, indirect recursion through a function pointer, recursion with
// static state, and deeply nested recursive calls.
#include <stdio.h>

// EXPECT-IR: func isEven(int n) : int
// EXPECT-IR: func isOdd(int n) : int
// EXPECT-IR: call isOdd, 1
// EXPECT-IR: call isEven, 1
// EXPECT-IR: func ack(int m, int n) : int
// EXPECT-COUNT: call ack, 2 6
// EXPECT-IR: func fibm(int n) : int
// EXPECT-IR: global int static_fibm_memo[40]
// EXPECT-IR: static_fibm_memo[t5] = t10
// EXPECT-IR: func rsum(int* a, int n) : int
// EXPECT-IR: t1 = *a
// EXPECT-IR: t2 = a + 4
// EXPECT-IR: t4 = call rsum, 2
// EXPECT-IR: func collatz(int n) : int
// EXPECT-IR: static_collatz_steps_1 = static_collatz_steps_1 + 1
// EXPECT-IR: func spin(int n) : int
// EXPECT-IR: local int(int)* fp
// EXPECT-IR: fp = spin
// EXPECT-IR: t2 = call *fp, 1
// EXPECT-IR: local int count
// EXPECT-IR: count = 0
// EXPECT-IR: local int total
// EXPECT-IR: total = 0

int isOdd(int n);
int isEven(int n);

int isOdd(int n)
{
    if (n == 0) return 0;
    return isEven(n - 1);
}

int isEven(int n)
{
    if (n == 0) return 1;
    return isOdd(n - 1);
}

int ack(int m, int n)
{
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

int fibm(int n)
{
    static int memo[40];
    if (n < 2) return n;
    if (memo[n] != 0) return memo[n];
    memo[n] = fibm(n - 1) + fibm(n - 2);
    return memo[n];
}

int rsum(int *a, int n)
{
    if (n <= 0) return 0;
    return *a + rsum(a + 1, n - 1);
}

int collatz(int n)
{
    static int steps = 0;
    steps = steps + 1;
    if (n == 1) return steps;
    if (n % 2 == 0) return collatz(n / 2);
    return collatz(3 * n + 1);
}

int spin(int n);

int spin(int n)
{
    int (*fp)(int) = spin;               /* indirect recursion */
    if (n <= 0) return 0;
    return 1 + fp(n - 1);
}

int main()
{
    int i, count = 0, total = 0;
    int data[6] = {1, 2, 3, 4, 5, 6};
    for (i = 0; i < 6; i++)
        if (isEven(data[i])) count = count + 1;
    total = count + ack(1, 2) + ack(2, 1) + fibm(10) + rsum(data, 6);
    total = total + spin(5) + collatz(6) + isOdd(9);
    printf("%d %d %d %d %d %d\n", total, count, fibm(8), ack(0, 3), spin(3), collatz(10));
    return 0;
}
