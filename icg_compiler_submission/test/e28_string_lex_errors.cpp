// Lexical errors in literals: empty char literal, bad octal escape, bad hex
// escape, unterminated string.
// EXPECT-EXIT: 1
// EXPECT-ERR: Empty character literal
// EXPECT-ERR: Invalid octal escape sequence '\8' (digit out of range 0-7)
// EXPECT-ERR: Invalid hex escape sequence '\x''
// EXPECT-ERR: Unterminated string literal
// EXPECT-NOIR
int main()
{
    char a = '';
    char b = '\8';
    char c = '\x';
    char *s = "unterminated;
    return 0;
}
