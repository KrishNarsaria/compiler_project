// Arrays (1-D, multi-dimensional, char/string), array initialisers,
// decay to pointers, pointer arithmetic, multi-level pointers.
#include <stdio.h>
#include <string.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local char text[6]
// EXPECT-IR: local int c[2][3][4]
// EXPECT-IR: local int** pp
// EXPECT-IR: local int*** ppp
// EXPECT-IR: flat[20] = 12

int sumArr(int a[], int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++)
        s += a[i];
    return s;
}

int sumArrPtr(int *p, int n)
{
    int s = 0;
    while (n-- > 0)
        s += *p++;
    return s;
}

int matrixSum(int m[][3], int rows)
{
    int i, j, s = 0;
    for (i = 0; i < rows; i++)
        for (j = 0; j < 3; j++)
            s += m[i][j];
    return s;
}

int cubeSum(int c[2][3][4])
{
    int i, j, k, s = 0;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 4; k++)
                s += c[i][j][k];
    return s;
}

int strLen(char *s)
{
    int n = 0;
    while (*s++) n++;
    return n;
}

int main()
{
    int a[6] = {10, 20, 30, 40, 50, 60};
    int partial[5] = {1, 2, 3};              /* rest zero */
    char name[8] = "icg";
    char text[] = "hello";
    int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int flat[2][3] = {7, 8, 9, 10, 11, 12}; /* flat init of a 2-D array */
    int c[2][3][4];
    int i, j, k;

    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 4; k++)
                c[i][j][k] = i * 100 + j * 10 + k;

    int *p = a;                              /* decay */
    int **pp = &p;                           /* multi-level */
    int ***ppp = &pp;
    char *cp = text;

    printf("%d\n", sumArr(a, 6));            // 210
    printf("%d\n", sumArrPtr(a, 6));         // 210
    printf("%d\n", matrixSum(m, 2));         // 21
    printf("%d\n", cubeSum(c));              // 660
    printf("%d\n", strLen(text));            // 5
    printf("%d %d\n", partial[2], partial[4]);   // 3 0
    printf("%d\n", flat[1][2]);              // 12

    printf("%d\n", a[0]);                    // 10
    printf("%d\n", *(a + 1));                // 20
    printf("%d\n", 2[a]);                    // 30  (a[i] == i[a])
    printf("%d\n", p[3]);                    // 40
    printf("%d\n", *pp == p ? 1 : 0);        // 1
    printf("%d\n", ***ppp);                  // 10
    printf("%c%c\n", cp[1], *(cp + 4));      // eo

    p = &a[2];
    printf("%d\n", *p);                      // 30
    p++;
    printf("%d\n", *p);                      // 40
    p -= 2;
    printf("%d\n", *p);                      // 20
    printf("%d\n", p[1]);                    // 30

    printf("%s %s\n", name, text);           // icg hello
    printf("%lu\n", strlen(text));           // 5
    printf("%lu\n", sizeof(a));              // 24
    return 0;
}
