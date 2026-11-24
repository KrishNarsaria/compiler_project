// Lambda depth: default captures [=] and [&], mixed explicit captures,
// captures of several variables, lambda in a loop, lambda calling another
// lambda, immediately-invoked lambda, lambdas with different signatures, and
// lambdas stored in arrays of function pointers.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local int a
// EXPECT-IR: local int b
// EXPECT-IR: local int c
// EXPECT-IR: local int(int)* f1
// EXPECT-IR: __cap1_a = a
// EXPECT-IR: f1 = __lambda1
// EXPECT-IR: func __lambda1(int a, int b, int c, int x) : int
// EXPECT-IR: t1 = x + a
// EXPECT-IR: t2 = t1 + b
// EXPECT-IR: t3 = t2 + c
// EXPECT-IR: local int(int)* f2
// EXPECT-IR: f2 = __lambda2
// EXPECT-IR: func __lambda2(int a, int& b, int x) : int
// EXPECT-IR: t1 = x * a
// EXPECT-IR: t2 = *b
// EXPECT-IR: t3 = t1 + t2
// EXPECT-IR: *b = t3
// EXPECT-IR: local int(int)* f3
// EXPECT-IR: f3 = __lambda3
// EXPECT-IR: func __lambda3(int a, int c, int x) : int
// EXPECT-IR: local int acc
// EXPECT-IR: acc = 0
// EXPECT-IR: f4 = __lambda4
// EXPECT-IR: func __lambda4(int& acc, int x) : int
// EXPECT-IR: __cap4_acc = &acc
// EXPECT-IR: local double(double)* half
// EXPECT-IR: half = __lambda5
// EXPECT-IR: func __lambda5(double v) : double
// EXPECT-IR: local double(double)* scale
// EXPECT-IR: scale = __lambda6
// EXPECT-IR: func __lambda6(double(double)* half, double k) : double
// EXPECT-IR: local int(int)* table[2]
// EXPECT-IR: table[0] = __lambda7
// EXPECT-IR: table[8] = __lambda8
// EXPECT-IR: t14 = call *t13, 1
// EXPECT-IR: t17 = call *t16, 1
// EXPECT-IR: local int boom
// EXPECT-IR: boom = call __lambda9, 3
// EXPECT-IR: func __lambda9(int a, int c, int x) : int

int main()
{
    int a = 2, b = 3, c = 5;
    int i, acc = 0;

    /* default capture by value: all three captured */
    auto f1 = [=](int x) { return x + a + b + c; };
    acc = acc + f1(1);

    /* mixed: a by value, b by reference (mutated) */
    auto f2 = [a, &b](int x) { b = x * a + b; return b; };
    acc = acc + f2(4);

    /* capture only a and c */
    auto f3 = [a, c](int x) { return (a + c) * x; };
    acc = acc + f3(3);

    /* lambda reassignment in a loop, default [&] capture */
    auto f4 = [&](int x) { acc = acc + x; return acc; };
    for (i = 0; i < 3; i++)
        f4(i + 1);

    /* lambda calling another lambda through a captured variable */
    auto half = [](double v) -> double { return v / 2.0; };
    auto scale = [half](double k) { return half(k) + 1.0; };
    double d = scale(8.0);

    /* table of lambdas indexed at runtime */
    int (*table[2])(int) = {
        [](int x) { return x + 2; },
        [](int x) { return x * 3; }
    };
    acc = acc + table[0](2) + table[1](3);

    /* immediately-invoked lambda */
    int boom = [a, c](int x) { return x + a + c; }(10);

    printf("%d %f %d\n", acc, d, boom);
    return 0;
}
