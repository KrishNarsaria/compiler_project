// struct / union / enum / typedef - definitions, nesting, whole-struct
// assignment and passing, unions, enums with explicit values, typedefs.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: struct Rect : size 24 { struct Point top_left @0; struct Point bottom_right @8; char label[8] @16; }
// EXPECT-IR: struct Node : size 16 { int value @0; struct Node* next @8; }
// EXPECT-IR: union Number : size 8 { int i @0; float f @0; char bytes[4] @0; double d @0; }

typedef unsigned int uint;
typedef unsigned long ulong;
typedef int *IntPtr;
typedef char *String;
typedef struct Point {
    int x;
    int y;
} Point;

struct Rect {
    Point top_left;
    Point bottom_right;
    char label[8];
};

typedef struct Node {
    int value;
    struct Node *next;
} Node;

struct Deep {
    struct Inner {
        int a;
        int b;
    } inner;
    int arr[3];
};

union Number {
    int i;
    float f;
    char bytes[4];
    double d;
};

union Mixed {
    int i;
    struct Point p;
    char s[8];
};

enum Color { RED, GREEN, BLUE };
enum Step { FIRST = 3, SECOND, THIRD = 10, FOURTH };

typedef enum Direction { NORTH = 1, EAST, SOUTH, WEST } Direction;

struct WithArray {
    int values[4];
    Point pts[2];
};

int pointSum(Point p)
{
    return p.x + p.y;
}

int pointSumPtr(Point *p)
{
    return p->x + p->y;
}

Point makePoint(int x, int y)
{
    Point p;
    p.x = x;
    p.y = y;
    return p;
}

int listSum(Node *n)
{
    int s = 0;
    while (n != NULL) {
        s += n->value;
        n = n->next;
    }
    return s;
}

int main()
{
    Point p1 = {3, 4};
    Point p2;
    struct Rect r;
    Node nodes[3];
    struct Deep d;
    union Number un;
    union Mixed mx;
    uint u = 7;
    ulong big = 10000000000UL;
    IntPtr ip;
    int x = 5;

    p2 = p1;                      /* whole-struct assignment */
    p2.x = 10;
    ip = &x;

    r.top_left = p1;
    r.bottom_right = makePoint(20, 30);
    r.label[0] = 'R';

    nodes[0].value = 1; nodes[0].next = &nodes[1];
    nodes[1].value = 2; nodes[1].next = &nodes[2];
    nodes[2].value = 3; nodes[2].next = NULL;

    d.inner.a = 11;
    d.inner.b = 22;
    d.arr[0] = 1; d.arr[1] = 2; d.arr[2] = 3;

    un.i = 0x41424344;
    mx.i = 99;

    printf("%d %d\n", p1.x, p1.y);              // 3 4
    printf("%d %d\n", p2.x, p2.y);              // 10 4
    printf("%d\n", pointSum(p1));               // 7
    printf("%d\n", pointSumPtr(&p2));           // 14
    printf("%d\n", pointSum(makePoint(2, 3)));  // 5
    printf("%d %d %d %d\n", r.top_left.x, r.top_left.y,
           r.bottom_right.x, r.bottom_right.y); // 3 4 20 30
    printf("%d\n", listSum(nodes));             // 6
    printf("%d %d %d\n", d.inner.a, d.inner.b, d.arr[2]);  // 11 22 3
    printf("%u %lu %d\n", u, big, *ip);         // 7 10000000000 5
    printf("%d %d\n", sizeof(Point), sizeof(un));   // 8 8
    printf("%d\n", RED + GREEN * 10 + BLUE * 100);   // 210
    printf("%d %d %d %d\n", FIRST, SECOND, THIRD, FOURTH); // 3 4 10 11
    printf("%d %d %d %d\n", NORTH, EAST, SOUTH, WEST);     // 1 2 3 4

    /* union: all members share storage */
    un.i = 65;
    printf("%d\n", un.bytes[0]);                // 65
    printf("%d\n", mx.p.x);                     // 99
    mx.p.x = 5;
    printf("%d\n", mx.i);                       // 5
    return 0;
}
