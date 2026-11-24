// Struct/union misuse: member access on non-structs, comparisons of
// structs, and pointer-member mismatch.
// EXPECT-EXIT: 1
// EXPECT-ERR: member reference with '.' requires a struct or union
// EXPECT-ERR: invalid operands to comparison
// EXPECT-NOIR
#include <stdio.h>

struct Pt { int x, y; };

int main()
{
    int n = 3;
    int bad = n.x;
    struct Pt a, b;
    a.x = 1;
    a.y = 2;
    b.x = 3;
    b.y = 4;
    if (a == b) { }
    struct Pt *p = &a;
    int m = p.x;
    printf("%d %d %d\n", bad, m, a.x);
    return 0;
}
