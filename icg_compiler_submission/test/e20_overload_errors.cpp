// Overload-resolution errors: ambiguous calls and no-match calls with
// various arities and conversion distances.
// EXPECT-EXIT: 1
// EXPECT-ERR: no matching function for call to 'f' with 3 argument(s)
// EXPECT-ERR: no matching function for call to 'f' with 0 argument(s)
// EXPECT-ERR: no matching function for call to 'f' with 1 argument(s)
// EXPECT-NOIR
int f(int a) { return a; }
int f(int a, int b) { return a + b; }
double f(double a) { return a; }

int main()
{
    f(1, 2, 3);
    f();
    f("string");
    return 0;
}
