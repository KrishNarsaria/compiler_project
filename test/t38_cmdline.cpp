// Command-line input: argc/argv are passed to main and parsed with atoi.
// EXPECT-IR: func main(int argc, char** argv) : int
// EXPECT-IR: call atoi, 1
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int i, sum = 0;
    printf("argc=%d\n", argc);
    for (i = 1; i < argc; i++) {
        printf("arg%d=%s\n", i, argv[i]);
        sum += atoi(argv[i]);
    }
    printf("sum=%d\n", sum);
    return 0;
}
