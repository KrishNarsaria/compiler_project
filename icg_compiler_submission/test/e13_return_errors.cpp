// Return-statement errors.
// EXPECT-ERR: void function 'noop' should not return a value
// EXPECT-ERR: non-void function 'mustReturn' should return a value
// EXPECT-ERR: cannot return a void value

void noop()
{
    return 3;                     /* value in a void function */
}

int mustReturn(int x)
{
    if (x > 0) return 1;
    return;                       /* bare return in a non-void function */
}

void other() { }

int alsoBad()
{
    return other();               /* returning a void expression */
}

int main()
{
    noop();
    mustReturn(1);
    alsoBad();
    return 0;
}
