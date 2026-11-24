// Switch-specific errors.
// EXPECT-ERR: case label is not an integer constant
// EXPECT-ERR: duplicate case value 1
// EXPECT-ERR: switch quantity is not an integer

int main()
{
    int x = 1;
    double d = 2.5;

    switch (x) {
    case 1:
        break;
    case 1:                       /* duplicate */
        break;
    }

    switch (x) {
    case x:                       /* not a constant */
        break;
    }

    switch (d) {                  /* non-integer quantity */
    case 1:
        break;
    }
    return 0;
}
