// Expected: semantic errors
// EXPECT-EXIT: 1
// EXPECT-ERR: redeclaration of 'a' in the same scope
// EXPECT-ERR: use of undeclared identifier 'b'
// EXPECT-ERR: no matching function for call to 'twice' with 2 argument(s)
// EXPECT-ERR: call to undeclared function 'undefined_fn'
// EXPECT-ERR: cannot assign to read-only variable 'k'
// EXPECT-ERR: 'break' statement not within a loop or switch
// EXPECT-ERR: non-void function 'main' should return a value
// EXPECT-ERR: 7 semantic error(s)
// EXPECT-NOIR
int twice(int v) { return v * 2; }

int main()
{
    int a = 5;
    int a = 6;
    b = 7;
    int r = twice(1, 2);
    undefined_fn(3);
    const int k = 1;
    k = 2;
    break;
    return;
}
