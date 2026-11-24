// Array / initialiser errors.
// EXPECT-ERR: array size must be a non-negative integer constant
// EXPECT-ERR: too many initializers for the array
// EXPECT-ERR: initializer string is too long for the array
// EXPECT-ERR: array size is missing and cannot be deduced from an initializer
// EXPECT-ERR: initializer of static variable 'bad' is not a constant expression

int dynSize = 10;

int main()
{
    int a[-3];                    /* negative size */
    int b[dynSize];               /* non-constant size */
    int c[3] = {1, 2, 3, 4};      /* too many */
    char d[3] = "toolong";        /* string does not fit */
    int e[];                      /* no size, no initializer */
    static int bad = dynSize + 1; /* static init not constant */
    return 0;
}
