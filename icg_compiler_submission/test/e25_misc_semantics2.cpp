// Miscellaneous semantic errors: void function returning a value, using a
// void call in an expression, goto to an undefined label, duplicate labels,
// and return type mismatches.
// EXPECT-EXIT: 1
// EXPECT-ERR: void function 'novalue' should not return a value
// EXPECT-ERR: void value used in an expression
// EXPECT-ERR: duplicate label 'top'
// EXPECT-ERR: use of undeclared label 'nowhere'
// EXPECT-NOIR
#include <stdio.h>

void novalue()
{
    return 5;
}

int returns_void_call()
{
    novalue();
    return novalue() + 1;
}

int badret()
{
    double d = 2.5;
    return d;
}

int main()
{
    int x = 1;
top:
    x = x + 1;
    if (x < 3) goto top;
    if (x > 10) goto nowhere;
top:
    x = x + 1;
    printf("%d %d\n", x, returns_void_call());
    return 0;
}
