// Variable-argument functions: va_start / va_arg / va_end with several
// argument types, a printf-style wrapper, and zero extra arguments.
#include <stdio.h>
#include <stdarg.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func sumInts(int n, ...) : int
// EXPECT-IR: va_start n
// EXPECT-IR: va_arg ap, int
// EXPECT-IR: va_arg ap, double
// EXPECT-IR: va_end ap

int sumInts(int n, ...)
{
    va_list ap;
    int i, total = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

double sumDoubles(int n, ...)
{
    va_list ap;
    int i;
    double total = 0;
    va_start(ap, n);
    for (i = 0; i < n; i++)
        total += va_arg(ap, double);
    va_end(ap);
    return total;
}

int maxOf(int first, ...)
{
    va_list ap;
    int best = first, v;
    va_start(ap, first);
    while ((v = va_arg(ap, int)) != -9999)
        if (v > best) best = v;
    va_end(ap);
    return best;
}

void printPairs(const char *label, int n, ...)
{
    va_list ap;
    int i;
    va_start(ap, n);
    printf("%s: ", label);
    for (i = 0; i < n; i++) {
        printf("(%d,", va_arg(ap, int));
        printf("%f) ", va_arg(ap, double));
    }
    printf("\n");
    va_end(ap);
}

int firstOnly(int n, ...)          /* zero varargs consumed */
{
    va_list ap;
    int r;
    va_start(ap, n);
    r = n;
    va_end(ap);
    return r;
}

int main()
{
    printf("%d\n", sumInts(0));                          // 0
    printf("%d\n", sumInts(5, 1, 2, 3, 4, 5));           // 15
    printf("%d\n", sumInts(3, 10, -5, 100));             // 105
    printf("%f\n", sumDoubles(3, 1.5, 2.5, 3.0));        // 7.0
    printf("%d\n", maxOf(3, 7, 2, 9, 1, -9999));         // 9
    printPairs("pairs", 2, 1, 0.5, 2, 1.5);              // (1,0.5) (2,1.5)
    printf("%d\n", firstOnly(42, 1, 2, 3));              // 42
    return 0;
}
