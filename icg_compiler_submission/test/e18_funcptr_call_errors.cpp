// Function-pointer specific call errors.
// EXPECT-ERR: 'x' is not a function
// EXPECT-ERR: wrong number of arguments in call through function pointer

typedef int (*Bin)(int, int);

int add(int a, int b) { return a + b; }

int main()
{
    int (*f)(int, int) = add;
    int x = 7;
    int r1 = x(1, 2);             /* calling an int */
    int r2 = f(1);                /* too few arguments */
    int r3 = f(1, 2, 3);          /* too many arguments */
    return 0;
}
