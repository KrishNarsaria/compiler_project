// Expected: lexical errors
// EXPECT-EXIT: 1
// EXPECT-ERR: Invalid octal literal '09'
// EXPECT-ERR: Malformed numeric literal '12abc'
// EXPECT-ERR: Multi-character literal 'ab'
// EXPECT-ERR: Unknown escape sequence '\q'
// EXPECT-ERR: Unrecognized character '@' (ASCII 64)
// EXPECT-ERR: 5 lexical error(s)
// EXPECT-NOIR
int main()
{
    int a = 09;
    int b = 12abc;
    char c = 'ab';
    char *s = "bad \q escape";
    int x = 5 @ 3;
    return 0;
}
