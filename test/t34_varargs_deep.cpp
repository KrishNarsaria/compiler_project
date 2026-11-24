// Variadic functions: va_arg of several types (int, double, char*), mixed
// type lists, zero-argument vararg calls, two vararg functions, varargs read
// in a loop, and a format-style dispatcher.
#include <stdio.h>
#include <cstdarg>

// EXPECT-IR: func sumInts(int n, ...) : int
// EXPECT-IR: ap = va_start n
// EXPECT-IR: t1 = va_arg ap, int
// EXPECT-IR: va_end ap
// EXPECT-IR: func collect(int n, ...) : double
// EXPECT-IR: t1 = va_arg ap, double
// EXPECT-COUNT: va_arg ap, int 1
// EXPECT-IR: func tagstr(int n, ...) : char*
// EXPECT-IR: s = va_arg ap, char*
// EXPECT-IR: local int total
// EXPECT-IR: total = 0
// EXPECT-COUNT: call sumInts, 1 1
// EXPECT-IR: call collect, 4
// EXPECT-IR: call collect, 3
// EXPECT-IR: call tagstr, 3
// EXPECT-IR: call sumInts, 4

int sumInts(int n, ...)
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

double collect(int n, ...)
{
    va_list ap;
    double acc = 0.0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        acc += va_arg(ap, double);
    va_end(ap);
    return acc;
}

char *tagstr(int n, ...)
{
    va_list ap;
    char *s = "";
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        s = va_arg(ap, char *);
    va_end(ap);
    return s;
}

int main()
{
    int s1 = sumInts(3, 1, 2, 3);
    int s2 = sumInts(0);
    int s3 = sumInts(1, 42);
    double d1 = collect(3, 1.5, 2.5, 3.5);
    double d2 = collect(2, 10.0, 0.25);
    char *tag = tagstr(2, "alpha", "omega");
    printf("%d %d %d %f %f %s\n", s1, s2, s3, d1, d2, tag);
    return 0;
}
