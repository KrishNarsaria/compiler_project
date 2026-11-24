// Function pointers, lambdas, variable-argument functions
#include <iostream>
#include <cstdarg>
using namespace std;

// EXPECT-IR: func add(int a, int b) : int
// EXPECT-IR: func mul(int a, int b) : int
// EXPECT-IR: func apply(int(int,int)* f, int x, int y) : int
// EXPECT-IR: t1 = call *f, 2
// EXPECT-IR: func cmp(void* a, void* b) : int
// EXPECT-IR: func sum(int n, ...) : int
// EXPECT-IR: ap = va_start n
// EXPECT-IR: t1 = va_arg ap, int
// EXPECT-IR: va_end ap
// EXPECT-IR: local int(int,int)* fp
// EXPECT-IR: fp = add
// EXPECT-IR: op = mul
// EXPECT-IR: t1 = call *fp, 2
// EXPECT-IR: t2 = call *op, 2
// EXPECT-IR: param cmp
// EXPECT-IR: call qsort, 4
// EXPECT-IR: square = __lambda1
// EXPECT-IR: __cap2_base = base
// EXPECT-IR: addBase = __lambda2
// EXPECT-IR: bump = __lambda3
// EXPECT-IR: half = __lambda4
// EXPECT-IR: func __lambda1(int v) : int
// EXPECT-IR: func __lambda2(int base, int v) : int
// EXPECT-IR: func __lambda3(int& base, int v) : void
// EXPECT-IR: func __lambda4(double d) : double
// EXPECT-IR: call __print_double, 1

typedef int (*BinOp)(int, int);

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }

int apply(int (*f)(int, int), int x, int y)
{
    return f(x, y);
}

int cmp(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

int sum(int n, ...)
{
    va_list ap;
    int total = 0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

int main()
{
    int (*fp)(int, int) = add;
    BinOp op = mul;
    int base = 10;
    int data[4] = {4, 2, 9, 1};

    printf("%d %d\n", fp(2, 3), op(4, 5));
    printf("%d\n", apply(add, 6, 7));
    fp = mul;
    printf("%d\n", fp(3, 3));

    auto square = [](int v) { return v * v; };
    auto addBase = [base](int v) { return v + base; };
    auto bump = [&base](int v) { base += v; };
    auto half = [](double d) -> double { return d / 2; };

    printf("%d %d\n", square(5), addBase(1));
    bump(5);
    cout << half(9.0) << endl;

    printf("%d %d\n", sum(3, 1, 2, 3), sum(5, 10, 20, 30, 40, 50));
    qsort(data, 4, sizeof(int), cmp);
    return 0;
}
