// Dynamic memory allocation: new / delete, new with initialiser, new[] /
// delete[], new of class objects (ctor runs), malloc / calloc / free /
// realloc, and allocation failure checks.
#include <stdio.h>
#include <stdlib.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: = alloc
// EXPECT-IR: free
// EXPECT-IR: p = alloc t
// EXPECT-IR: cells = alloc 12
// EXPECT-IR: m = (int*) t8
// EXPECT-IR: d = (double*) t10
// EXPECT-IR: call realloc, 2
// EXPECT-IR: call Cell::Cell_int, 2

class Cell {
public:
    int v;
    Cell() : v(0) { }
    Cell(int x) : v(x) { }
    int get() { return v; }
};

struct Blob {
    int data[4];
};

int *makeInts(int n)
{
    int *p = new int[n];
    int i;
    for (i = 0; i < n; i++) p[i] = i * i;
    return p;
}

int main()
{
    int *a = new int(42);          /* scalar new with initialiser */
    int *b = new int;              /* scalar new, uninitialised */
    int *arr = makeInts(5);        /* array new */
    char *s = new char[8];
    int i, sum = 0;
    Cell *c1 = new Cell(9);        /* constructor runs */
    Cell *cells = new Cell[3];     /* array of objects */
    struct Blob *bl = new Blob;
    int *m = (int *)malloc(4 * sizeof(int));
    int *big;
    double *d = (double *)calloc(3, sizeof(double));

    *b = 7;
    s[0] = 'O'; s[1] = 'K'; s[2] = 0;
    for (i = 0; i < 5; i++) sum += arr[i];
    printf("%d\n", sum);           // 0+1+4+9+16 = 30
    printf("%d %d\n", *a, *b);     // 42 7
    printf("%s\n", s);             // OK
    printf("%d\n", c1->get());     // 9

    cells[0].v = 10;
    cells[1].v = 20;
    cells[2].v = 30;
    printf("%d %d %d\n", cells[0].get(), cells[1].get(), cells[2].get()); // 10 20 30

    bl->data[0] = 1;
    bl->data[3] = 4;
    printf("%d\n", bl->data[0] + bl->data[3]);  // 5

    if (m == NULL) { printf("alloc failed\n"); return 1; }
    for (i = 0; i < 4; i++) m[i] = i + 1;
    printf("%d\n", m[0] + m[1] + m[2] + m[3]);  // 10

    big = (int *)realloc(m, 8 * sizeof(int));
    if (big == NULL) { printf("realloc failed\n"); free(m); return 1; }
    for (i = 4; i < 8; i++) big[i] = i + 1;
    printf("%d\n", big[7]);        // 8
    free(big);

    if (d == NULL) { printf("calloc failed\n"); return 1; }
    printf("%f\n", d[0] + d[1] + d[2]);  // 0.0

    delete a;
    delete b;
    delete[] arr;
    delete[] s;
    delete c1;
    delete[] cells;
    delete bl;
    free(d);
    return 0;
}
