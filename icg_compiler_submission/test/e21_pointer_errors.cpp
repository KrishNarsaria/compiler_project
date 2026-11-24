// Pointer type errors: dereferencing non-pointers, arithmetic on void*,
// integer-to-pointer assignment, pointer+pointer, and mismatch in
// pointer arithmetic.
// EXPECT-EXIT: 1
// EXPECT-ERR: indirection requires a pointer operand
// EXPECT-ERR: arithmetic on a pointer to 'void'
// EXPECT-ERR: cannot convert 'int' to pointer type 'int*'
// EXPECT-ERR: cannot add two pointers
// EXPECT-ERR: cannot convert pointer type 'int*' to 'int'
// EXPECT-ERR: incompatible pointer types ('char*' to 'int*')
// EXPECT-NOIR
#include <stdio.h>

int main()
{
    int x = 5;
    int *p = &x;
    int y = *x;
    void *vp = p;
    vp = vp + 1;
    int *q = 10;
    int z = p + q;
    char c = 'a';
    int *r = &c;
    printf("%d %d %d %d\n", y, z, c, r);
    return 0;
}
