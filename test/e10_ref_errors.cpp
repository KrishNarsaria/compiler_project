// Reference errors.
// EXPECT-ERR: reference 'r' must be initialised
// EXPECT-ERR: reference 'q' must be initialised
// EXPECT-ERR: cannot return a reference to a non-lvalue
// EXPECT-ERR: global references are not supported

int &badGlobal = *(int *)0;      /* references at file scope */

int &returnsTemp()
{
    return 5;                     /* not an lvalue */
}

int &returnsExpr(int x)
{
    return x + 1;                 /* not an lvalue */
}

int main()
{
    int x = 1;
    int &r;                       /* uninitialised reference */
    int &q;                       /* uninitialised reference */
    r = x;
    return 0;
}
