// Lambda and function-pointer call errors.
// EXPECT-ERR: a lambda can only initialise 'auto' or a function pointer
// EXPECT-ERR: 'x' is not a function
// EXPECT-ERR: no matching call to lambda 'f' with 2 argument(s)

typedef int (*Un)(int);

int main()
{
    int x = 5;
    int notAFn = [](int v) { return v; };   /* lambda into a plain int */
    int y = x(1);                           /* calling a non-function */
    Un f = [](int v) { return v * 2; };
    f(1, 2);                                /* wrong arg count via pointer */
    return 0;
}
