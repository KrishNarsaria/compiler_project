// Lambdas: no capture, capture-by-value, capture-by-reference, named
// captures, return-type annotations, calling immediately, and lambdas
// that capture locals in nested scopes.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: nothing = __lambda1
// EXPECT-IR: typed = __lambda4
// EXPECT-IR: namedRef = __lambda8
// EXPECT-IR: capAllVal = __lambda11
// EXPECT-IR: func __lambda1(

int apply2(int (*f)(int, int), int a, int b)
{
    return f(a, b);
}

int main()
{
    int base = 10;
    int offset = 5;

    auto nothing = []() { printf("no capture\n"); };
    auto square = [](int v) { return v * v; };
    auto add = [](int a, int b) { return a + b; };
    auto typed = [](double d) -> double { return d / 2; };

    auto byValue = [=](int v) { return v + base + offset; };   /* [=] captures both */
    auto byRef = [&](int v) { base += v; offset += v; return base + offset; };
    auto named = [base](int v) { return v * base; };           /* named by-value */
    auto namedRef = [&offset](int v) { offset = v; return offset; };
    auto mixed = [&base, offset](int v) { return base + offset + v; };

    auto capAllRef = [&]() { base = base * 2; };
    auto capAllVal = [=]() { return base + offset; };

    nothing();
    printf("%d\n", square(6));            // 36
    printf("%d\n", add(20, 22));          // 42
    printf("%f\n", typed(9.0));           // 4.5

    printf("%d\n", byValue(1));           // 16
    printf("%d\n", byRef(1));             // 17 (base=11, offset=6)
    printf("%d %d\n", base, offset);      // 11 6
    printf("%d\n", named(3));             // 33
    printf("%d\n", namedRef(99));         // 99
    printf("%d\n", offset);               // 99
    printf("%d\n", mixed(1));             // 11 + 99 + 1 = 111

    capAllRef();
    printf("%d\n", base);                 // 22
    printf("%d\n", capAllVal());          // 121

    printf("%d\n", apply2(add, 3, 4));    // 7 (lambda converts to function pointer)
    printf("%d\n", ([](int x) { return x + 1; })(41));   // 42 (immediately invoked)

    {   /* captures of variables in nested scopes */
        int inner = 7;
        auto grab = [&inner](int v) { inner += v; return inner; };
        printf("%d\n", grab(3));         // 10
        printf("%d\n", inner);           // 10
    }
    return 0;
}
