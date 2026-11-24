// Advanced features: recursion, dynamic memory, typedef, references, enum, union,
// until loop, multi-level pointers, multi-dimensional arrays, overloading,
// command line input
#include <iostream>
#include <cstdlib>
using namespace std;

// EXPECT-IR: struct Node : size 16 { int value @0; struct Node* next @8; }
// EXPECT-IR: union Number : size 4 { int i @0; float f @0; }
// EXPECT-COUNT: call fib, 1 3
// EXPECT-IR: func area_int_int(int w, int h) : int
// EXPECT-IR: func area_double(double r) : double
// EXPECT-IR: func swapRef(int& x, int& y) : void
// EXPECT-IR: t = *x
// EXPECT-IR: *x = t2
// EXPECT-IR: func trace(int[3]* m, int n) : int
// EXPECT-IR: func main(int argc, char** argv) : int
// EXPECT-IR: local unsigned int big
// EXPECT-IR: big = 4000000000
// EXPECT-IR: local union Number n
// EXPECT-IR: c = 5
// EXPECT-IR: local int** pp
// EXPECT-IR: local int*** ppp
// EXPECT-IR: pp = &p
// EXPECT-IR: ppp = &pp
// EXPECT-IR: t13 = *ppp
// EXPECT-IR: t14 = *t13
// EXPECT-IR: *t14 = 42
// EXPECT-IR: t4 = argv[8]
// EXPECT-IR: call atoi, 1
// EXPECT-IR: grid[32] = 9
// EXPECT-IR: grid[20] = total
// EXPECT-IR: n.i = 65
// EXPECT-IR: alloc 20
// EXPECT-IR: head = alloc 16
// EXPECT-IR: arr2 = alloc 32
// EXPECT-IR: free head
// EXPECT-IR: free arr2
// EXPECT-IR: free dyn
// EXPECT-IR: if i >= 3 goto L13
// EXPECT-IR: if i > 0 goto L14
// EXPECT-IR: call area_int_int, 2
// EXPECT-IR: call area_double, 1
// EXPECT-IR: call __print_double, 1
// EXPECT-IR: call __print_endl, 0
// EXPECT-IR: call trace, 2

typedef unsigned int uint;
typedef struct Node {
    int value;
    struct Node *next;
} Node;

enum Color { RED, GREEN = 5, BLUE };
union Number { int i; float f; };

int fib(int n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int area(int w, int h) { return w * h; }
double area(double r) { return 3.14 * r * r; }

void swapRef(int &x, int &y)
{
    int t = x;
    x = y;
    y = t;
}

int trace(int m[][3], int n)
{
    int i, t = 0;
    for (i = 0; i < n; i++)
        t += m[i][i];
    return t;
}

int main(int argc, char **argv)
{
    int grid[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    int i, j, total = 0;
    int x = 1, y = 2;
    int *p = &x;
    int **pp = &p;
    int ***ppp = &pp;
    uint big = 4000000000;
    Number n;
    Color c = GREEN;
    int *dyn;
    Node *head;

    if (argc > 1)
        total = atoi(argv[1]);

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            total += grid[i][j];
    grid[1][2] = total;

    ***ppp = 42;
    swapRef(x, y);
    n.i = 65;

    dyn = (int *)malloc(5 * sizeof(int));
    dyn[2] = fib(10);
    head = new Node;
    head->value = 1;
    head->next = nullptr;
    int *arr2 = new int[8];
    arr2[3] = BLUE;
    delete head;
    delete[] arr2;
    free(dyn);

    i = 0;
    until (i >= 3) {
        i++;
    }
    do {
        i--;
    } until (i <= 0);

    cout << area(3, 4) << " " << area(2.0) << " " << trace(grid, 3) << endl;
    return c;
}
