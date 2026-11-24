// Scope errors: undeclared identifiers in nested blocks, duplicate globals,
// duplicate struct fields, and duplicate function definitions.
// EXPECT-EXIT: 1
// EXPECT-ERR: duplicate member 'v'
// EXPECT-ERR: redefinition of function 'dupfn'
// EXPECT-ERR: redefinition of global 'g'
// EXPECT-ERR: use of undeclared identifier 'missing'
// EXPECT-NOIR
#include <stdio.h>

struct S {
    int v;
    int v;
};

int g = 1;
int g = 2;

int dupfn(int x) { return x; }
int dupfn(int x) { return x + 1; }

int main()
{
    int ok = 1;
    {
        int hidden = 2;
        ok = ok + hidden;
        ok = ok + missing;
    }
    printf("%d\n", ok);
    return 0;
}
