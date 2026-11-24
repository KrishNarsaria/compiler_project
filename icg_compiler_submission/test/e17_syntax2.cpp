// Syntax errors: malformed statements and declarations.
// EXPECT-ERR: syntax error

int main()
{
    int x = ;                     /* missing initialiser */
    int y = 1 +;                  /* missing operand */
    if (x > 0                     /* missing ')' */
        x = 2;
    for (i = 0; i < 10; i++       /* missing ')' */
        x++;
    return 0;
}
