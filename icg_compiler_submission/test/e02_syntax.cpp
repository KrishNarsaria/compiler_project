// Expected: syntax errors
// EXPECT-EXIT: 1
// EXPECT-ERR: unexpected ';' (near ';')
// EXPECT-ERR: unexpected '{', expecting ')' or ',' (near '{')
// EXPECT-ERR: unexpected RETURN, expecting ';' or ',' (near 'return')
// EXPECT-ERR: unexpected '}' (near '}')
// EXPECT-ERR: 4 syntax error(s)
// EXPECT-NOIR
int main()
{
    int a = ;
    if (a > 1 {
        a = 2;
    }
    int b = 3
    return 0;
}
