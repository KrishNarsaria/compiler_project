// Initialiser errors: too many elements, wrong scalar type, string into
// int, and a string too long for its array.
// EXPECT-EXIT: 1
// EXPECT-ERR: too many initializers for the array
// EXPECT-ERR: too many initializers for a scalar
// EXPECT-ERR: cannot convert pointer type 'char*' to 'int'
// EXPECT-ERR: initializer string is too long for the array
// EXPECT-NOIR
#include <stdio.h>

int main()
{
    int arr[3] = {1, 2, 3, 4};
    int scalar = {1, 2};
    int bad = "hello";
    char tiny[3] = "way too long";
    int mixed[2] = {1, "x"};
    printf("%d %d %d %c %d\n", arr[0], scalar, bad, tiny[0], mixed[0]);
    return 0;
}
