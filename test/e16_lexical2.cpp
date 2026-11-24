// Lexical errors: bad escapes, bad numbers, unterminated literals.
// EXPECT-ERR: Unknown escape sequence '\q'
// EXPECT-ERR: Invalid octal escape sequence '\8'
// EXPECT-ERR: Invalid octal literal '09'
// EXPECT-ERR: Malformed numeric literal '12abc'
// EXPECT-ERR: Multi-character literal 'ab'

int main()
{
    char a = '\q';                /* unknown escape */
    char b = '\8';                /* digit out of range 0-7 */
    int c = 09;                   /* 8/9 not valid in octal */
    int d = 12abc;                /* malformed number */
    char e = 'ab';                /* multi-character literal */
    return 0;
}
