// Type errors: invalid operands, bad casts, void misuse, bad subscripts.
// EXPECT-ERR: cannot add two pointers
// EXPECT-ERR: invalid operands to comparison
// EXPECT-ERR: invalid cast to 'struct P'
// EXPECT-ERR: array subscript is not an integer
// EXPECT-ERR: indirection requires a pointer operand
// EXPECT-ERR: void value used in an expression
// EXPECT-ERR: invalid operand to '~'
// EXPECT-ERR: cannot subtract a pointer from an integer

struct P { int v; };

void nothing() { }

int main()
{
    int a[5];
    int x = 3;
    int *p = a;
    struct P s1, s2;

    int e1 = p + p;               /* cannot add two pointers */
    int e2 = s1 < s2;             /* invalid comparison */
    (struct P)x;                  /* invalid cast to a struct */
    int e4 = a[x + 0.5];          /* subscript not an integer */
    int e5 = *x;                  /* indirection of a non-pointer */
    int e6 = nothing() + 1;       /* void in an expression */
    double e7 = ~2.5;             /* ~ on a float */
    int *e8 = x - p;              /* subtract a pointer from an integer */
    return 0;
}
