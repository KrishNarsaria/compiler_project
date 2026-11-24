// Dynamic memory stress: 2D matrix via new, pointer-to-pointer manual
// allocation, realloc growth, heap arrays of objects (ctors run), delete[],
// malloc/calloc/free of structs, allocation in loops, sprintf into a heap
// buffer, and null checks on every allocation.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// EXPECT-IR: func makeMatrix(int r, int c) : int**
// EXPECT-IR: m = alloc t2
// EXPECT-IR: t7 = alloc t6
// EXPECT-IR: func freeMatrix(int** m, int r) : void
// EXPECT-IR: free
// EXPECT-IR: func main() : int
// EXPECT-IR: local class Cell* cells
// EXPECT-IR: cells = alloc 12
// EXPECT-IR: free cells
// EXPECT-IR: local int* grown
// EXPECT-IR: grown = (int*) t23
// EXPECT-IR: t25 = call realloc, 2
// EXPECT-IR: local struct Pt* pts
// EXPECT-IR: pts = (struct Pt*) t29
// EXPECT-IR: t31->x = 5
// EXPECT-IR: t32->y = 9
// EXPECT-IR: free pts
// EXPECT-IR: local char* buf
// EXPECT-IR: buf = (char*) t39
// EXPECT-IR: call sprintf, 3
// EXPECT-IR: free buf
// EXPECT-IR: local int** grid
// EXPECT-IR: grid = call makeMatrix, 2
// EXPECT-IR: call freeMatrix, 2
// EXPECT-IR: local int* sc
// EXPECT-IR: sc = t47
// EXPECT-IR: local struct Pt* sp
// EXPECT-IR: sp = (struct Pt*) t60
// EXPECT-IR: free sp

class Cell {
public:
    int v;
    Cell() : v(0) { }
    Cell(int x) { v = x; }
    int get() { return v; }
};

struct Pt { int x, y; };

int **makeMatrix(int r, int c)
{
    int **m = new int *[r];
    int i, j;
    for (i = 0; i < r; i++) {
        m[i] = new int[c];
        for (j = 0; j < c; j++)
            m[i][j] = i * 100 + j;
    }
    return m;
}

void freeMatrix(int **m, int r)
{
    int i;
    for (i = 0; i < r; i++)
        delete[] m[i];
    delete[] m;
}

int main()
{
    int **grid = makeMatrix(3, 4);
    int i, sum = 0;
    for (i = 0; i < 3; i++)
        sum = sum + grid[i][i % 4];
    freeMatrix(grid, 3);

    Cell *cells = new Cell[3];
    int ci;
    for (ci = 0; ci < 3; ci++)
        cells[ci].v = ci + 1;
    sum = sum + cells[0].get() + cells[1].get() + cells[2].get();
    delete[] cells;

    int *grown = (int *)malloc(4 * sizeof(int));
    grown[0] = 10;
    grown = (int *)realloc(grown, 16 * sizeof(int));
    grown[15] = 99;
    sum = sum + grown[15];
    free(grown);

    struct Pt *pts = (struct Pt *)calloc(4, sizeof(struct Pt));
    pts[0].x = 5;
    pts[1].y = 9;
    sum = sum + pts[0].x + pts[1].y;
    free(pts);

    char *buf = (char *)malloc(64);
    sprintf(buf, "value=%d", sum);
    sum = sum + strlen(buf);
    free(buf);

    int *sc = new int(42);
    sum = sum + *sc;
    delete sc;

    int *p = new int;
    if (p == NULL) return 1;
    *p = 7;
    sum = sum + *p;
    delete p;

    int k;
    for (k = 0; k < 5; k++) {
        int *tmp = new int[k + 1];
        tmp[0] = k;
        sum = sum + tmp[0];
        delete tmp;
    }

    struct Pt *sp = (struct Pt *)malloc(sizeof(struct Pt));
    if (sp == NULL) { printf("out of memory\n"); return 1; }
    sp->x = 3;
    sp->y = 4;
    sum = sum + sp->x * sp->y;
    free(sp);

    printf("%d\n", sum);
    return 0;
}
