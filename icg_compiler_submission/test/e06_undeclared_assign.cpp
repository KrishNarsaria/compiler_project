// Undeclared names, non-assignable targets, read-only variables.
// EXPECT-ERR: use of undeclared identifier 'nope'
// EXPECT-ERR: call to undeclared function 'missing'
// EXPECT-ERR: enumeration constant 'RED' is not assignable
// EXPECT-ERR: array type is not assignable
// EXPECT-ERR: cannot assign to read-only variable 'k'
// EXPECT-ERR: cannot assign to read-only variable 'c'

enum Color { RED, GREEN };

int main()
{
    const int k = 5;
    const char c = 'x';
    int arr[3];
    int a = nope;                 /* undeclared variable */
    int b = missing(1);           /* undeclared function */
    RED = 7;                      /* enum constant not assignable */
    arr = a;                      /* array type not assignable */
    k = 6;                        /* write to a const */
    c = 'y';
    return 0;
}
