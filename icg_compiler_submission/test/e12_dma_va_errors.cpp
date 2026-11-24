// Dynamic memory, delete and variadic-helper errors.
// EXPECT-ERR: 'delete' requires a pointer operand
// EXPECT-ERR: array size in 'new' must be an integer
// EXPECT-ERR: 'va_start' used in a function that is not variadic
// EXPECT-ERR: wrong number of arguments to 'free'

int notVariadic(int a, int b)
{
    return a + b;
}

int misuseVa(int n)
{
    va_list ap;
    int x;
    va_start(ap, n);              /* not variadic */
    x = va_arg(ap, int);
    va_end(ap);
    return x;
}

int main()
{
    int x = 3;
    double d = 2.5;
    delete x;                     /* not a pointer */
    int *p = new int[d];          /* non-integer array size */
    free(1, 2);                   /* wrong number of arguments */
    return 0;
}
