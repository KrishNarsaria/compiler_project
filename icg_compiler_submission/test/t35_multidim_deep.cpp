// Multi-dimensional arrays: 3D and 4D arrays, ragged nested initialisers,
// partial row indexing (arr[i] as a row pointer), 2D array parameters, row
// pointer arithmetic, and diagonal-style index math.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local int cube[2][3][4]
// EXPECT-IR: cube[0] = 1
// EXPECT-IR: cube[4] = 2
// EXPECT-IR: cube[48] = 13
// EXPECT-IR: cube[92] = 24
// EXPECT-IR: local int hyper[2][2][2][2]
// EXPECT-IR: hyper[0] = 0
// EXPECT-IR: hyper[4] = 1
// EXPECT-IR: hyper[60] = 15
// EXPECT-IR: local int rag[3][3]
// EXPECT-IR: rag[0] = 1
// EXPECT-IR: rag[4] = 0
// EXPECT-IR: rag[8] = 0
// EXPECT-IR: rag[12] = 4
// EXPECT-IR: rag[32] = 9
// EXPECT-IR: local int* row
// EXPECT-IR: row = &cube[48]
// EXPECT-IR: t37 = row[12]
// EXPECT-IR: local int* flat
// EXPECT-IR: flat = &cube[0]
// EXPECT-IR: t40 = flat[92]
// EXPECT-IR: func trace2(int[3]* m, int n) : int
// EXPECT-IR: t1 = i * 12
// EXPECT-IR: t2 = i * 4
// EXPECT-IR: t3 = t1 + t2
// EXPECT-IR: t4 = m[t3]
// EXPECT-IR: func sumRows(int[3]* m, int n) : int
// EXPECT-IR: param t42
// EXPECT-IR: call trace2, 2
// EXPECT-IR: call sumRows, 2

int trace2(int m[][3], int n)
{
    int i, t = 0;
    for (i = 0; i < n; i++)
        t += m[i][i];
    return t;
}

int sumRows(int m[][3], int n)
{
    int i, j, t = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 3; j++)
            t += m[i][j];
    return t;
}

int main()
{
    int cube[2][3][4] = {
        {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}},
        {{13, 14, 15, 16}, {17, 18, 19, 20}, {21, 22, 23, 24}}
    };
    int hyper[2][2][2][2] = {
        {{{0, 1}, {2, 3}}, {{4, 5}, {6, 7}}},
        {{{8, 9}, {10, 11}}, {{12, 13}, {14, 15}}}
    };
    int rag[3][3] = {{1}, {4, 5}, {7, 8, 9}};      /* ragged: rest zero-filled */
    int grid[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    int i, j, k, sum = 0;

    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 4; k++)
                sum += cube[i][j][k];
    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++)
            for (k = 0; k < 2; k++)
                sum += hyper[i][j][k][0] + hyper[i][j][k][1];
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            sum += rag[i][j];

    int *row = cube[1][0];                    /* partial indexing -> row ptr */
    sum += row[3];
    int *flat = &cube[0][0][0];               /* whole cube flattened */
    sum += flat[23];

    sum += trace2(grid, 3) + sumRows(grid, 3);
    printf("%d %d %d\n", sum, cube[1][2][3], hyper[1][1][1][1]);
    return 0;
}
