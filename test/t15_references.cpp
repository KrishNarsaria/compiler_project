// References: reference parameters, reference locals, references to array
// elements and struct members, reference-returning functions, assignment
// through references, chained references.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func swap(int& x, int& y) : void
// EXPECT-IR: func maxRef(int& x, int& y) : int
// EXPECT-IR: return
// EXPECT-IR: local int& r

void swap(int &x, int &y)
{
    int t = x;
    x = y;
    y = t;
}

int &maxRef(int &x, int &y)
{
    return x >= y ? x : y;         /* both branches are lvalues */
}

int &elem(int arr[], int i)
{
    return arr[i];
}

struct S { int v; int w; };

int &fieldRef(struct S *s)
{
    return s->v;
}


void incAll(int &a, int &b, int &c)
{
    a++; b++; c++;
}

int main()
{
    int x = 1, y = 2;
    int &r = x;                    /* reference local */
    int arr[3] = {10, 20, 30};
    struct S s = {5, 6};
    int z = 9;

    r = 100;                       /* writes through the reference */
    printf("%d %d\n", x, r);       // 100 100

    swap(x, y);
    printf("%d %d\n", x, y);       // 2 100

    maxRef(x, y) = 50;             /* assign through a returned reference */
    printf("%d %d\n", x, y);       // 2 50

    elem(arr, 1) = 200;
    printf("%d %d\n", arr[1], elem(arr, 1));  // 200 200

    fieldRef(&s) = 55;
    printf("%d %d\n", s.v, fieldRef(&s));     // 55 55

    int &r2 = arr[2];              /* reference to an element */
    r2 = 300;
    printf("%d\n", arr[2]);        // 300

    incAll(x, y, z);
    printf("%d %d %d\n", x, y, z); // 3 51 10

    {
        int &inner = z;            /* reference in an inner scope */
        inner += 5;
    }
    printf("%d\n", z);             // 15
    return 0;
}
