// Functions: overloading (count and type), recursion, prototypes,
// void returns, nested calls, functions returning structs.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func add_int_int(int a, int b) : int
// EXPECT-IR: func add_int_int_int(int a, int b, int c) : int
// EXPECT-IR: func add_double_double(double a, double b) : double
// EXPECT-IR: func mkPair(int a, int b) : struct Pair
// EXPECT-IR: func fact(int n) : int
// EXPECT-IR: func ack(int m, int n) : int

int add(int a, int b) { return a + b; }
int add(int a, int b, int c) { return a + b + c; }
double add(double a, double b) { return a + b; }
int add(int a) { return a + 1; }

int neg(int x);                  /* prototype */

struct Pair { int a; int b; };
struct Pair mkPair(int a, int b)
{
    struct Pair p;
    p.a = a;
    p.b = b;
    return p;
}

int fact(int n)
{
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int fib(int n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int ack(int m, int n)
{
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

void noReturn(int x)
{
    printf("noReturn %d\n", x);
}

int deep(int n)
{
    if (n == 0) return 0;
    return 1 + deep(n - 1);
}

int neg(int x) { return -x; }

int forward(int x);              /* used before its definition */

int forward(int x)
{
    return x * 2;
}

int main()
{
    struct Pair pr = mkPair(7, 8);
    printf("%d %d %d\n", add(1, 2), add(1, 2, 3), add(1));     // 3 6 2
    printf("%f\n", add(1.5, 2.25));                            // 3.75
    printf("%d\n", fact(6));                                   // 720
    printf("%d\n", fib(10));                                   // 55
    printf("%d\n", ack(2, 2));                                 // 7
    printf("%d\n", deep(100));                                 // 100
    printf("%d\n", neg(42));                                   // -42
    printf("%d\n", forward(21));                               // 42
    printf("%d %d\n", pr.a, pr.b);                             // 7 8
    printf("%d\n", add(fact(3), fib(5)));                      // 6 + 5 = 11
    noReturn(3);
    return 0;
}
