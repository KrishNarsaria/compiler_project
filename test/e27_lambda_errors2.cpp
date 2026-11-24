// Lambda misuse: wrong arity calls, capturing an undeclared variable, and
// calling a lambda through the wrong signature.
// EXPECT-EXIT: 1
// EXPECT-ERR: no matching call to lambda 'f' with 0 argument(s)
// EXPECT-ERR: no matching call to lambda 'f' with 2 argument(s)
// EXPECT-ERR: use of undeclared identifier 'nope'
// EXPECT-NOIR
#include <stdio.h>

int main()
{
    auto f = [](int v) { return v * 2; };
    f();
    f(1, 2);
    auto g = [nope](int v) { return v + nope; };
    int x = f(2);
    int (*fp)(int, int) = [](int a) { return a; };
    (void)g; (void)x; (void)fp;
    return 0;
}
